// test_policy.cpp - repair policies and long-term cost.   (Plan: Day 9 optional P2; EXP-004; TC-J-01 .. TC-J-14)
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "reliability/errors.hpp"
#include "reliability/failure_model.hpp"
#include "reliability/mttf.hpp"
#include "reliability/policy.hpp"
#include "reliability/signature.hpp"
#include "reliability/state.hpp"
#include "reliability/system_model.hpp"

using namespace reliability;

namespace {

const std::string kReferenceConfig = std::string(RELIABILITY_CONFIG_DIR) + "/satellite_support.json";

SystemModel load_reference() { return SystemModel::from_json_file(kReferenceConfig); }

// The paper's Fig. 1 system: n = 3, Phi = max{x3, min{x1, x2}}.
SystemModel paper_figure_one() {
  return SystemModel::from_function(3, [](StateMask x) {
    return is_working(x, 2) || (is_working(x, 0) && is_working(x, 1));
  });
}

SystemModel bridge_network() {
  return SystemModel::from_function(5, [](StateMask x) {
    const bool c1 = is_working(x, 0), c2 = is_working(x, 1), c3 = is_working(x, 2),
               c4 = is_working(x, 3), c5 = is_working(x, 4);
    return (c1 && c4) || (c2 && c5) || (c1 && c3 && c5) || (c2 && c3 && c4);
  });
}

SystemModel random_monotone_system(int n, std::mt19937& rng) {
  const int terms = 1 + static_cast<int>(rng() % 4U);
  std::vector<StateMask> masks;
  for (int t = 0; t < terms; ++t) {
    StateMask m = 0;
    while (m == 0) {
      m = static_cast<StateMask>(rng()) & all_working(n);
    }
    masks.push_back(m);
  }
  return SystemModel::from_function(n, [masks](StateMask x) {
    return std::any_of(masks.begin(), masks.end(), [x](StateMask m) { return (x & m) == m; });
  });
}

void ExpectRel(double actual, double expected, double rel, const std::string& label) {
  EXPECT_NEAR(actual, expected, rel * std::max(1.0, std::fabs(expected))) << label;
}

// Closed forms for independent exponential lifetimes that use only the system signature and the failure-time
// increments, never the policy machinery. The system fails at failure number K with probability s_K, and K is
// independent of the failure times; a cycle ends at failure number min(K, r) (r = n for corrective maintenance).
struct SignatureForm {
  double length;
  double cost;
};

SignatureForm signature_closed_form(const SystemModel& model, int r, const RepairCosts& costs, double lambda) {
  const int n = model.n();
  const SweepResult sweep_result = sweep(model, nullptr, SweepOptions{true});
  const std::vector<double> s = signature(sweep_result.vectors[all_working(n)]);  // s[k-1] = P(K = k)
  const IndependentExponential fm(lambda);
  auto time_of_failure = [&](int j) {  // E T_{j:n} = sum_{i<j} delta(n, i)
    double t = 0.0;
    for (int i = 0; i < j; ++i) {
      t += fm.increment(n, i);
    }
    return t;
  };
  SignatureForm out{0.0, costs.setup};
  for (int k = 1; k <= n; ++k) {
    const double p = s[static_cast<std::size_t>(k - 1)];
    out.length += p * time_of_failure(std::min(k, r));
    out.cost += costs.component * p * std::min(k, r) + (k <= r ? costs.system * p : 0.0);
  }
  return out;
}

// The paper's Proposition 3, an INDEPENDENT route to the cost of an MTTF-threshold policy (it needs the MTTF to be
// monotone, which holds for the independent, lethal-shock and binomial-failure-rate models). With f_w the
// probability that the number of working components ever takes the value w, started at n, and
//     tau_w = f_w / (C(n,w) nu_w)           expected time spent in ONE given state with w working components,
//     phi_w = nu_w sum_{w'<w} (w - w') P(w,w')   expected number of failures per unit time,
// the cycle of a policy that continues exactly on the set C has
//     length T = sum_{x in C} tau_|x|,   N = sum_{x in C} tau_|x| phi_|x|   (expected failed components at the repair),
//     P = sum_{x in C} tau_|x| nu_|x| sum_{w'<|x|} P(|x|,w') (1 - W^x_{|x|-w'} / C(|x|,|x|-w'))   (chance the repair is corrective),
// and the cycle cost is c_set + c_comp N + c_sys P. No sub-state enumeration and no policy recursion is involved.
SignatureForm proposition_3(const SystemModel& model, const FailureModel& fm, const RepairSet& repair,
                            const RepairCosts& costs) {
  const int n = model.n();
  const SweepResult sweep_result = sweep(model, nullptr, SweepOptions{true});
  std::vector<double> f(static_cast<std::size_t>(n) + 1, 0.0);
  f[static_cast<std::size_t>(n)] = 1.0;
  for (int v = n - 1; v >= 0; --v) {
    for (int above = v + 1; above <= n; ++above) {
      f[static_cast<std::size_t>(v)] += f[static_cast<std::size_t>(above)] * fm.transition_prob(above, v);
    }
  }
  double length = 0.0, failed_components = 0.0, corrective = 0.0;
  for (std::size_t x = 0; x < repair.size(); ++x) {
    if (repair[x] != 0) {
      continue;
    }
    const int w = working_count(static_cast<StateMask>(x));
    const double nu = fm.exit_rate(w);
    const double tau = f[static_cast<std::size_t>(w)] / (static_cast<double>(binomial(n, w)) * nu);
    double phi = 0.0, failing = 0.0;
    for (int w2 = 0; w2 < w; ++w2) {
      const double p = fm.transition_prob(w, w2);
      phi += nu * (w - w2) * p;
      const double working_fraction = static_cast<double>(sweep_result.vectors[x][static_cast<std::size_t>(w - w2)]) /
                                      static_cast<double>(binomial(w, w - w2));
      failing += nu * p * (1.0 - working_fraction);
    }
    length += tau;
    failed_components += tau * phi;
    corrective += tau * failing;
  }
  return {length, costs.setup + costs.component * failed_components + costs.system * corrective};
}

}  // namespace

// Traceability: TC-J-06
// Every threshold policy of every test system, under all three failure models, against the paper's Proposition 3.
// This is the only check of the policy costs under SIMULTANEOUS failures that does not use the policy recursion.
TEST(Policy, ThresholdPoliciesMatchThePaperProposition3UnderAllFailureModels) {
  std::mt19937 rng(31337);
  std::vector<SystemModel> systems = {load_reference(), paper_figure_one(), bridge_network()};
  for (int t = 0; t < 10; ++t) {
    systems.push_back(random_monotone_system(4 + static_cast<int>(rng() % 5U), rng));
  }
  const RepairCosts cost_sets[] = {{0.0, 1.0, 0.0}, {0.1, 1.0, 0.05}, {0.5, 2.0, 0.3}};
  std::size_t compared = 0;
  for (std::size_t idx = 0; idx < systems.size(); ++idx) {
    const int n = systems[idx].n();
    std::vector<std::unique_ptr<FailureModel>> models;
    models.push_back(make_exponential(1.0));
    models.push_back(make_lethal_shock(0.3, n));
    models.push_back(make_binomial_failure_rate(0.3, n));
    for (const auto& fm : models) {
      for (const RepairCosts& costs : cost_sets) {
        const std::vector<double> mttf = compute_all_mttf(systems[idx], *fm);
        const ThresholdSearch search = search_threshold_policies(systems[idx], *fm, costs);
        // rebuild each policy's repair set from its gamma interval: the policy for gamma = gamma_to is the one that
        // repairs strictly below gamma_to ... but pi_gamma with gamma just ABOVE gamma_from repairs ratio <= gamma_from
        for (const ThresholdPolicyResult& p : search.policies) {
          const double gamma = p.gamma_from == 0.0 ? 1e-9 : p.gamma_from * (1.0 + 1e-9);
          const RepairSet repair = threshold_policy(systems[idx], mttf, gamma);
          const SignatureForm want = proposition_3(systems[idx], *fm, repair, costs);
          const PolicyEvaluation direct = evaluate_policy(systems[idx], *fm, repair, costs);
          ExpectRel(direct.mean_cycle_length, want.length, 1e-9, fm->name() + " length, system " + std::to_string(idx));
          ExpectRel(direct.mean_cycle_cost, want.cost, 1e-9, fm->name() + " cost, system " + std::to_string(idx));
          ExpectRel(p.evaluation.long_term_mean_cost, want.cost / want.length, 1e-9,
                    fm->name() + " ltmc, system " + std::to_string(idx));
          ++compared;
        }
      }
    }
  }
  EXPECT_GT(compared, 300U);
}

// Traceability: TC-J-01
// The paper's Fig. 1 example: c_comp = 1/30, c_sys = 1, c_set = 0, iid exponential lifetimes with unit mean.
// There are exactly three distinct threshold policies, with long-term mean costs 97/105, 11/15 and 1/10.
TEST(Policy, PaperFigureOneThresholdPoliciesMatchThePublishedCosts) {
  const SystemModel model = paper_figure_one();
  const IndependentExponential fm(1.0);
  const RepairCosts costs{1.0 / 30.0, 1.0, 0.0};
  const ThresholdSearch search = search_threshold_policies(model, fm, costs);
  ASSERT_EQ(search.policies.size(), 3U);

  // gamma in (0, 3/7]: corrective maintenance; (3/7, 6/7]: also repair the (1,1,0) state; (6/7, 1]: repair at the first failure
  EXPECT_NEAR(search.policies[0].gamma_from, 0.0, 1e-15);
  EXPECT_NEAR(search.policies[0].gamma_to, 3.0 / 7.0, 1e-12);
  EXPECT_NEAR(search.policies[1].gamma_to, 6.0 / 7.0, 1e-12);
  EXPECT_NEAR(search.policies[2].gamma_to, 1.0, 1e-12);
  EXPECT_NEAR(search.policies[0].evaluation.long_term_mean_cost, 97.0 / 105.0, 1e-12);
  EXPECT_NEAR(search.policies[1].evaluation.long_term_mean_cost, 11.0 / 15.0, 1e-12);
  EXPECT_NEAR(search.policies[2].evaluation.long_term_mean_cost, 1.0 / 10.0, 1e-12);
  EXPECT_EQ(search.policies[0].states_repaired, 3U);  // the three failed states
  EXPECT_EQ(search.policies[1].states_repaired, 4U);
  EXPECT_EQ(search.policies[2].states_repaired, 7U);  // everything except the all-working state

  // "so repairing at the first failure is the cheapest"
  EXPECT_EQ(search.best, 2U);
  // the corrective cycle is a whole lifetime: 7/6; one repair per cycle
  EXPECT_NEAR(search.policies[0].evaluation.mean_cycle_length, 7.0 / 6.0, 1e-12);
  EXPECT_NEAR(search.policies[0].evaluation.mean_cycle_cost, 97.0 / 90.0, 1e-12);
}

// Traceability: TC-J-02
// Every distinct policy is evaluated: one per distinct operational MTTF ratio. For the satellite the operational
// states have MTTF 0.7, 8/15, 5/12 and 1/3 (0, 1, 2, 3 failures), so there are exactly four.
TEST(Policy, SearchEvaluatesEveryBreakpointOfTheSatellite) {
  const SystemModel model = load_reference();
  const IndependentExponential fm(1.0);
  const ThresholdSearch search = search_threshold_policies(model, fm, RepairCosts{0.1, 1.0, 0.05});
  ASSERT_EQ(search.policies.size(), 4U);
  const double ratios[] = {(1.0 / 3.0) / 0.7, (5.0 / 12.0) / 0.7, (8.0 / 15.0) / 0.7, 1.0};
  for (std::size_t j = 0; j < 4; ++j) {
    EXPECT_NEAR(search.policies[j].gamma_to, ratios[j], 1e-12) << j;
    if (j > 0) {
      EXPECT_NEAR(search.policies[j].gamma_from, ratios[j - 1], 1e-12) << j;  // intervals tile (0, 1]
    }
  }
  // states repaired grow with gamma: 37 failed, then +8 (3 failures), +12 (2 failures), +6 (1 failure)
  EXPECT_EQ(search.policies[0].states_repaired, 37U);
  EXPECT_EQ(search.policies[1].states_repaired, 45U);
  EXPECT_EQ(search.policies[2].states_repaired, 57U);
  EXPECT_EQ(search.policies[3].states_repaired, 63U);
  // best is genuinely the minimum over all of them
  for (const ThresholdPolicyResult& p : search.policies) {
    EXPECT_GE(p.evaluation.long_term_mean_cost, search.policies[search.best].evaluation.long_term_mean_cost - 1e-15);
  }
}

// Traceability: TC-J-02
// The paper warns that the cost curve over gamma is NOT unimodal: it can fall, rise and fall again. A concrete example
// (found by random search over the paper's cost grid): an 8-component system whose minimal path sets are the
// bitmasks 90, 203, 22 and 41, with c_comp = 10/8 (so that n c_comp / c_sys = 10), c_sys = 1, c_set = 0. The curve has
// three local minima (policies 3, 6 and 8 of 11). A search that decreases gamma from 1 and stops when the cost first
// rises ends at policy 8 and misses the true optimum, policy 6.
TEST(Policy, CostCurveIsNotUnimodalSoEveryBreakpointMustBeSearched) {
  const std::vector<StateMask> terms = {90, 203, 22, 41};
  const SystemModel model = SystemModel::from_function(8, [terms](StateMask x) {
    return std::any_of(terms.begin(), terms.end(), [x](StateMask m) { return (x & m) == m; });
  });
  ASSERT_NO_THROW(model.validate());
  const IndependentExponential fm(1.0);
  const ThresholdSearch search = search_threshold_policies(model, fm, RepairCosts{10.0 / 8.0, 1.0, 0.0});
  ASSERT_EQ(search.policies.size(), 11U);

  const double expected[] = {9.816666667, 9.776583710, 9.751764706, 9.742857143, 9.746710526, 9.680288462,
                             9.628571429, 9.653225806, 9.642857143, 9.84375, 10.0};
  for (std::size_t j = 0; j < 11; ++j) {
    EXPECT_NEAR(search.policies[j].evaluation.long_term_mean_cost, expected[j], 1e-8) << "policy " << j;
  }
  EXPECT_EQ(search.best, 6U);

  std::vector<std::size_t> local_minima;
  for (std::size_t j = 1; j + 1 < search.policies.size(); ++j) {
    const double c = search.policies[j].evaluation.long_term_mean_cost;
    if (c < search.policies[j - 1].evaluation.long_term_mean_cost && c < search.policies[j + 1].evaluation.long_term_mean_cost) {
      local_minima.push_back(j);
    }
  }
  EXPECT_EQ(local_minima, (std::vector<std::size_t>{3, 6, 8}));  // more than one: not unimodal

  // the naive search: start at gamma = 1 (the last policy) and stop as soon as lowering gamma makes the cost rise
  std::size_t naive = search.policies.size() - 1;
  while (naive > 0 && search.policies[naive - 1].evaluation.long_term_mean_cost <
                          search.policies[naive].evaluation.long_term_mean_cost) {
    --naive;
  }
  EXPECT_EQ(naive, 8U);
  EXPECT_GT(search.policies[naive].evaluation.long_term_mean_cost,
            search.policies[search.best].evaluation.long_term_mean_cost * 1.001);  // it loses 0.15 %
}

// Traceability: TC-J-03
// gamma -> 0 is corrective maintenance; gamma = 1 repairs at the first failure of a relevant component, which for
// the satellite (every component relevant) is the r = 1 policy; the r = n policy is corrective maintenance.
TEST(Policy, ThresholdExtremesCoincideWithSimplePolicies) {
  for (const SystemModel& model : {load_reference(), paper_figure_one(), bridge_network()}) {
    const IndependentExponential fm(1.0);
    const RepairCosts costs{0.15, 1.0, 0.1};
    const ThresholdSearch search = search_threshold_policies(model, fm, costs);
    const PolicyEvaluation corrective = evaluate_policy(model, fm, corrective_policy(model), costs);
    const PolicyEvaluation first_failure = evaluate_policy(model, fm, r_out_of_n_policy(model, 1), costs);
    const PolicyEvaluation r_n = evaluate_policy(model, fm, r_out_of_n_policy(model, model.n()), costs);
    ExpectRel(search.policies.front().evaluation.long_term_mean_cost, corrective.long_term_mean_cost, 1e-12, "gamma -> 0");
    ExpectRel(search.policies.back().evaluation.long_term_mean_cost, first_failure.long_term_mean_cost, 1e-12, "gamma = 1");
    ExpectRel(r_n.long_term_mean_cost, corrective.long_term_mean_cost, 1e-12, "r = n");

    // and the same through the explicit policy builders
    const std::vector<double> mttf = compute_all_mttf(model, fm);
    const double tiny = 1e-9;
    ExpectRel(evaluate_policy(model, fm, threshold_policy(model, mttf, tiny), costs).long_term_mean_cost,
              corrective.long_term_mean_cost, 1e-12, "threshold_policy(gamma tiny)");
    ExpectRel(evaluate_policy(model, fm, threshold_policy(model, mttf, 1.0), costs).long_term_mean_cost,
              first_failure.long_term_mean_cost, 1e-12, "threshold_policy(1)");
  }
}

// Traceability: TC-J-04
// Corrective maintenance, independent exponential lifetimes: the cycle is one lifetime, and at its end the system has
// failed with K components down. So  LTMC = (c_set + c_sys + c_comp E[K]) / MTTF  with E[K] = sum_k k s_k.
// Checked on the satellite, the bridge and random systems against the closed form built from the SIGNATURE.
TEST(Policy, CorrectiveMaintenanceMatchesTheSignatureClosedForm) {
  std::mt19937 rng(5551212);
  std::vector<SystemModel> systems = {load_reference(), paper_figure_one(), bridge_network()};
  for (int t = 0; t < 25; ++t) {
    systems.push_back(random_monotone_system(3 + static_cast<int>(rng() % 6U), rng));
  }
  const RepairCosts cost_sets[] = {{0.0, 1.0, 0.0}, {0.1, 1.0, 0.05}, {0.4, 1.0, 0.5}, {2.0 / 3.0 / 6.0, 3.0, 0.2}};
  for (std::size_t idx = 0; idx < systems.size(); ++idx) {
    for (double lambda : {1.0, 2.5}) {
      const IndependentExponential fm(lambda);
      for (const RepairCosts& costs : cost_sets) {
        const PolicyEvaluation got = evaluate_policy(systems[idx], fm, corrective_policy(systems[idx]), costs);
        const SignatureForm want = signature_closed_form(systems[idx], systems[idx].n(), costs, lambda);
        ExpectRel(got.mean_cycle_length, want.length, 1e-12, "length, system " + std::to_string(idx));
        ExpectRel(got.mean_cycle_cost, want.cost, 1e-12, "cost, system " + std::to_string(idx));
        ExpectRel(got.long_term_mean_cost, want.cost / want.length, 1e-12, "ltmc, system " + std::to_string(idx));
      }
    }
  }
}

// Traceability: TC-J-04
// The r-out-of-n policy, for every r, against the signature closed form (cycle ends at failure min(K, r)).
TEST(Policy, ROutOfNPoliciesMatchTheSignatureClosedForm) {
  std::mt19937 rng(8080);
  std::vector<SystemModel> systems = {load_reference(), paper_figure_one(), bridge_network()};
  for (int t = 0; t < 15; ++t) {
    systems.push_back(random_monotone_system(3 + static_cast<int>(rng() % 6U), rng));
  }
  const RepairCosts costs{0.12, 1.0, 0.07};
  for (std::size_t idx = 0; idx < systems.size(); ++idx) {
    const IndependentExponential fm(1.0);
    const std::vector<RPolicyResult> results = evaluate_r_out_of_n_policies(systems[idx], fm, costs);
    ASSERT_EQ(static_cast<int>(results.size()), systems[idx].n());
    for (const RPolicyResult& result : results) {
      const SignatureForm want = signature_closed_form(systems[idx], result.r, costs, 1.0);
      ExpectRel(result.evaluation.mean_cycle_length, want.length, 1e-12, "length, r=" + std::to_string(result.r));
      ExpectRel(result.evaluation.mean_cycle_cost, want.cost, 1e-12, "cost, r=" + std::to_string(result.r));
    }
  }
}

// Traceability: TC-J-05
// Scaling laws that must hold for any policy: doubling every cost doubles the cost rate; doubling the failure rate
// halves the cycle length and doubles the cost rate; zero costs cost nothing; the cost rate is never negative.
TEST(Policy, ScalingLawsAndSanity) {
  const SystemModel model = load_reference();
  const RepairSet policy = r_out_of_n_policy(model, 2);
  const RepairCosts costs{0.1, 1.0, 0.05};
  const PolicyEvaluation base = evaluate_policy(model, IndependentExponential(1.0), policy, costs);
  const PolicyEvaluation twice_costs =
      evaluate_policy(model, IndependentExponential(1.0), policy, RepairCosts{0.2, 2.0, 0.1});
  ExpectRel(twice_costs.long_term_mean_cost, 2.0 * base.long_term_mean_cost, 1e-12, "costs x2");
  ExpectRel(twice_costs.mean_cycle_length, base.mean_cycle_length, 1e-12, "costs do not change the length");
  const PolicyEvaluation fast = evaluate_policy(model, IndependentExponential(2.0), policy, costs);
  ExpectRel(fast.mean_cycle_length, base.mean_cycle_length / 2.0, 1e-12, "lambda x2: length");
  ExpectRel(fast.long_term_mean_cost, 2.0 * base.long_term_mean_cost, 1e-12, "lambda x2: cost rate");
  EXPECT_EQ(evaluate_policy(model, IndependentExponential(1.0), policy, RepairCosts{0, 0, 0}).long_term_mean_cost, 0.0);
  // only the system-failure penalty: LTMC = c_sys * P(cycle ends in a failure) / E[length], all in (0, c_sys / length]
  const PolicyEvaluation penalty_only = evaluate_policy(model, IndependentExponential(1.0), policy, RepairCosts{0, 1, 0});
  EXPECT_GT(penalty_only.long_term_mean_cost, 0.0);
  EXPECT_LT(penalty_only.mean_cycle_cost, 1.0 + 1e-12);  // a probability
}

// Traceability: TC-J-05
// Policies are also defined for the simultaneous-failure models; the same laws hold, and common cause changes the answer.
TEST(Policy, WorksUnderSimultaneousFailureModels) {
  const SystemModel model = load_reference();
  const RepairCosts costs{0.1, 1.0, 0.05};
  const std::unique_ptr<FailureModel> lethal = make_lethal_shock(0.3, 6);
  const std::unique_ptr<FailureModel> bfr = make_binomial_failure_rate(0.3, 6);
  const IndependentExponential independent(1.0);
  // corrective maintenance: one cycle is a lifetime, so the cycle length is the system MTTF under each model
  for (const FailureModel* fm : {static_cast<const FailureModel*>(lethal.get()), static_cast<const FailureModel*>(bfr.get()),
                                 static_cast<const FailureModel*>(&independent)}) {
    const PolicyEvaluation e = evaluate_policy(model, *fm, corrective_policy(model), costs);
    ExpectRel(e.mean_cycle_length, compute_all_mttf(model, *fm)[63], 1e-10, fm->name());
    EXPECT_GT(e.long_term_mean_cost, 0.0);
  }
  const double a = evaluate_policy(model, independent, corrective_policy(model), costs).long_term_mean_cost;
  const double b = evaluate_policy(model, *lethal, corrective_policy(model), costs).long_term_mean_cost;
  EXPECT_GT(std::fabs(a - b), 0.01);

  // the threshold search runs and finds the best policy among all breakpoints
  const ThresholdSearch search = search_threshold_policies(model, *bfr, costs);
  EXPECT_GE(search.policies.size(), 2U);
  for (const ThresholdPolicyResult& p : search.policies) {
    EXPECT_GE(p.evaluation.long_term_mean_cost, search.policies[search.best].evaluation.long_term_mean_cost - 1e-15);
  }
}

// Traceability: TC-J-05
TEST(Policy, PreconditionsAreChecked) {
  const SystemModel model = load_reference();
  const IndependentExponential fm(1.0);
  const RepairCosts costs{0.1, 1.0, 0.0};
  RepairSet policy = corrective_policy(model);

  RepairSet repairs_new_system = policy;
  repairs_new_system[63] = 1;
  EXPECT_THROW(evaluate_policy(model, fm, repairs_new_system, costs), ModelError);   // would never run

  RepairSet ignores_failure = policy;
  ignores_failure[0] = 0;
  EXPECT_THROW(evaluate_policy(model, fm, ignores_failure, costs), ModelError);       // must repair failed states

  EXPECT_THROW(evaluate_policy(model, fm, RepairSet(10, 1), costs), ModelError);      // wrong size
  for (const RepairCosts& bad : {RepairCosts{-1, 1, 0}, RepairCosts{0.1, std::nan(""), 0},
                                 RepairCosts{0.1, 1, std::numeric_limits<double>::infinity()}}) {
    EXPECT_THROW(evaluate_policy(model, fm, policy, bad), ModelError);
  }
  EXPECT_THROW(r_out_of_n_policy(model, 0), ModelError);
  EXPECT_THROW(r_out_of_n_policy(model, 7), ModelError);
  const std::vector<double> mttf = compute_all_mttf(model, fm);
  EXPECT_THROW(threshold_policy(model, mttf, 0.0), ModelError);
  EXPECT_THROW(threshold_policy(model, mttf, 1.5), ModelError);
  EXPECT_THROW(threshold_policy(model, std::vector<double>(3, 1.0), 0.5), ModelError);
  EXPECT_THROW(search_threshold_policies(SystemModel::parallel(kMaxPolicyComponents + 1), fm, costs), ModelError);
  EXPECT_THROW(evaluate_policy(SystemModel::from_function(1, [](StateMask x) { return x != 0U; }), fm, RepairSet(2, 1), costs),
               ModelError);
}

namespace {

// Every state-based policy of a small system: repair at every failed state, never at the all-working state, and any
// choice at the other operational states. Returns the lowest long-term mean cost found by evaluating all of them.
double brute_force_best_cost(const SystemModel& model, const FailureModel& fm, const RepairCosts& costs) {
  std::vector<std::size_t> free_states;
  for (std::size_t x = 0; x + 1 < (std::size_t{1} << model.n()); ++x) {
    if (model.phi(static_cast<StateMask>(x))) {
      free_states.push_back(x);
    }
  }
  EXPECT_LE(free_states.size(), 10U) << "too many free states for an exhaustive search";
  double best = std::numeric_limits<double>::infinity();
  for (std::uint32_t choice = 0; choice < (1U << free_states.size()); ++choice) {
    RepairSet policy = corrective_policy(model);
    for (std::size_t i = 0; i < free_states.size(); ++i) {
      policy[free_states[i]] = (choice >> i) & 1U;
    }
    best = std::min(best, evaluate_policy(model, fm, policy, costs).long_term_mean_cost);
  }
  return best;
}

std::vector<RepairCosts> cost_grid(int n) {
  std::vector<RepairCosts> grid;
  for (double rho : {0.1, 0.5, 2.0, 10.0}) {
    for (double sigma : {0.0, 0.1, 0.5}) {
      grid.push_back({rho / n, 1.0, sigma});
    }
  }
  return grid;
}

}  // namespace

// Traceability: TC-J-07
TEST(Policy, OptimalStatePolicyEqualsTheBestOfAnExhaustiveSearchOverAllStateBasedPolicies) {
  std::mt19937 rng(20261022);
  std::vector<SystemModel> systems = {paper_figure_one()};
  while (systems.size() < 7) {  // random systems of 4 and 5 components with at most 10 free states (2^10 policies each)
    const SystemModel candidate = random_monotone_system(systems.size() < 4 ? 4 : 5, rng);
    std::size_t free_states = 0;
    for (std::size_t x = 0; x + 1 < (std::size_t{1} << candidate.n()); ++x) {
      free_states += candidate.phi(static_cast<StateMask>(x)) ? 1 : 0;
    }
    if (free_states >= 2 && free_states <= 10) {
      systems.push_back(candidate);
    }
  }
  int compared = 0;
  for (const SystemModel& model : systems) {
    const int n = model.n();
    const std::vector<std::shared_ptr<FailureModel>> models = {
        std::make_shared<IndependentExponential>(1.0), make_lethal_shock(0.3, n), make_binomial_failure_rate(0.3, n),
        make_gamma_process(0.3, n)};
    for (const auto& fm : models) {
      for (const RepairCosts& costs : cost_grid(n)) {
        const OptimalPolicyResult optimal = optimal_state_policy(model, *fm, costs);
        const double exhaustive = brute_force_best_cost(model, *fm, costs);
        ExpectRel(optimal.evaluation.long_term_mean_cost, exhaustive, 1e-10,
                  fm->name() + " n=" + std::to_string(n) + " rho*sigma index " + std::to_string(compared));
        // the policy that is returned really has the cost that is reported
        ExpectRel(evaluate_policy(model, *fm, optimal.repair_at, costs).long_term_mean_cost,
                  optimal.evaluation.long_term_mean_cost, 1e-12, "returned policy");
        ++compared;
      }
    }
  }
  EXPECT_EQ(compared, 7 * 4 * 12);
}

// Traceability: TC-J-08
TEST(Policy, OptimalStatePolicyIsNeverWorseThanAnyOtherFamilyAndSometimesBetter) {
  const SystemModel satellite = load_reference();
  std::mt19937 rng(77);
  std::vector<SystemModel> systems = {satellite, paper_figure_one(), bridge_network()};
  for (int i = 0; i < 4; ++i) {
    systems.push_back(random_monotone_system(7, rng));
  }
  int strictly_better_than_threshold = 0;
  for (const SystemModel& model : systems) {
    const int n = model.n();
    const IndependentExponential fm(1.0);
    const auto lsh = make_lethal_shock(0.2, n);
    for (const FailureModel* m : {static_cast<const FailureModel*>(&fm), static_cast<const FailureModel*>(lsh.get())}) {
      for (const RepairCosts& costs : cost_grid(n)) {
        const double v = optimal_state_policy(model, *m, costs).evaluation.long_term_mean_cost;
        const ThresholdSearch threshold = search_threshold_policies(model, *m, costs);
        const double best_threshold = threshold.policies[threshold.best].evaluation.long_term_mean_cost;
        double best_r = std::numeric_limits<double>::infinity();
        for (const RPolicyResult& r : evaluate_r_out_of_n_policies(model, *m, costs)) {
          best_r = std::min(best_r, r.evaluation.long_term_mean_cost);
        }
        const double corrective = evaluate_policy(model, *m, corrective_policy(model), costs).long_term_mean_cost;
        EXPECT_LE(v, best_threshold * (1 + 1e-10));
        EXPECT_LE(v, best_r * (1 + 1e-10));
        EXPECT_LE(v, corrective * (1 + 1e-10));
        if (v < best_threshold * (1 - 1e-6)) {
          ++strictly_better_than_threshold;
        }
      }
    }
  }
  // the optimum is a different thing from the best MTTF threshold on at least some systems; if it never were, this test
  // would be checking nothing about the algorithm
  EXPECT_GT(strictly_better_than_threshold, 0);
}

// Traceability: TC-J-09
TEST(Policy, OptimalStatePolicyOnThePapersFigureOneRepairsAtTheFirstFailure) {
  const SystemModel model = paper_figure_one();
  const IndependentExponential fm(1.0);
  const OptimalPolicyResult optimal = optimal_state_policy(model, fm, RepairCosts{1.0 / 30.0, 1.0, 0.0});
  ExpectRel(optimal.evaluation.long_term_mean_cost, 1.0 / 10.0, 1e-12, "the cheapest threshold policy of the paper is optimal");
  EXPECT_GE(optimal.iterations, 1);
  // it repairs at every state with at least one failed component (and at the failed ones)
  for (StateMask x = 0; x + 1 < 8; ++x) {
    EXPECT_EQ(optimal.repair_at[x], 1) << x;
  }
  EXPECT_EQ(optimal.repair_at[7], 0);
  EXPECT_EQ(optimal.states_repaired, 7U);
}

// Traceability: TC-J-10
TEST(Policy, OptimalStatePolicyDegenerateCasesAndPreconditions) {
  const SystemModel model = load_reference();
  const IndependentExponential fm(1.0);
  // costly repairs: corrective maintenance is optimal and the first pass already shows it
  const OptimalPolicyResult corrective = optimal_state_policy(model, fm, RepairCosts{10.0 / 6.0, 1.0, 0.5});
  ExpectRel(corrective.evaluation.long_term_mean_cost,
            evaluate_policy(model, fm, corrective_policy(model), RepairCosts{10.0 / 6.0, 1.0, 0.5}).long_term_mean_cost, 1e-12,
            "corrective");
  // all costs zero: every policy costs 0 and the algorithm must stop, not loop
  const OptimalPolicyResult free_repairs = optimal_state_policy(model, fm, RepairCosts{0.0, 0.0, 0.0});
  EXPECT_EQ(free_repairs.evaluation.long_term_mean_cost, 0.0);
  EXPECT_LE(free_repairs.iterations, 3);
  // scaling: costs x 2 -> cost x 2 and the same policy
  const RepairCosts base{0.2, 1.0, 0.1};
  const OptimalPolicyResult one = optimal_state_policy(model, fm, base);
  const OptimalPolicyResult two = optimal_state_policy(model, fm, RepairCosts{0.4, 2.0, 0.2});
  ExpectRel(two.evaluation.long_term_mean_cost, 2 * one.evaluation.long_term_mean_cost, 1e-10, "scaling");
  EXPECT_EQ(one.repair_at, two.repair_at);
  EXPECT_THROW(optimal_state_policy(model, fm, RepairCosts{-1.0, 1.0, 0.0}), ModelError);
  EXPECT_THROW(optimal_state_policy(SystemModel::parallel(kMaxPolicyComponents + 1), fm, base), ModelError);
}

// Traceability: TC-J-11
// The level-sum code against the enumeration code it replaces, on random policies, every breakpoint of the threshold search
// and the optimal policy, under independent failures and four simultaneous-failure models.
TEST(Policy, LevelSumCodeAgreesWithTheEnumerationCode) {
  std::mt19937 rng(424242);
  std::vector<SystemModel> systems = {load_reference(), paper_figure_one(), bridge_network()};
  for (int i = 0; i < 4; ++i) {
    systems.push_back(random_monotone_system(7 + i, rng));
  }
  int policies_compared = 0, thresholds_compared = 0;
  for (const SystemModel& model : systems) {
    const int n = model.n();
    const std::vector<std::shared_ptr<FailureModel>> models = {
        std::make_shared<IndependentExponential>(1.0), make_lethal_shock(0.3, n), make_binomial_failure_rate(0.4, n),
        make_compound_poisson(0.2, n), make_inverse_gaussian(0.3, n)};
    for (const auto& fm : models) {
      const RepairCosts costs{0.15 / n, 1.0, 0.07};
      // random state-based policies: repair at the failed states and at a random subset of the others
      for (int trial = 0; trial < 6; ++trial) {
        RepairSet policy = corrective_policy(model);
        for (std::size_t x = 0; x + 1 < policy.size(); ++x) {
          policy[x] = policy[x] | static_cast<std::uint8_t>(rng() % 3U == 0U);
        }
        const PolicyEvaluation fast = evaluate_policy(model, *fm, policy, costs);
        const PolicyEvaluation slow = evaluate_policy_by_enumeration(model, *fm, policy, costs);
        ExpectRel(fast.mean_cycle_length, slow.mean_cycle_length, 1e-10, "length " + fm->name());
        ExpectRel(fast.mean_cycle_cost, slow.mean_cycle_cost, 1e-10, "cost " + fm->name());
        ExpectRel(fast.long_term_mean_cost, slow.long_term_mean_cost, 1e-10, "ltmc " + fm->name());
        ++policies_compared;
      }
      // every breakpoint of the threshold search
      const ThresholdSearch fast = search_threshold_policies(model, *fm, costs);
      const ThresholdSearch slow = search_threshold_policies_by_enumeration(model, *fm, costs);
      ASSERT_EQ(fast.policies.size(), slow.policies.size()) << fm->name();
      EXPECT_EQ(fast.best, slow.best);
      for (std::size_t j = 0; j < fast.policies.size(); ++j) {
        ExpectRel(fast.policies[j].gamma_from, slow.policies[j].gamma_from, 1e-9, "gamma_from");
        ExpectRel(fast.policies[j].gamma_to, slow.policies[j].gamma_to, 1e-9, "gamma_to");
        EXPECT_EQ(fast.policies[j].states_repaired, slow.policies[j].states_repaired) << fm->name() << " policy " << j;
        ExpectRel(fast.policies[j].evaluation.mean_cycle_length, slow.policies[j].evaluation.mean_cycle_length, 1e-9, "T");
        ExpectRel(fast.policies[j].evaluation.mean_cycle_cost, slow.policies[j].evaluation.mean_cycle_cost, 1e-9, "cost");
        ExpectRel(fast.policies[j].evaluation.long_term_mean_cost, slow.policies[j].evaluation.long_term_mean_cost, 1e-9, "ltmc");
        ++thresholds_compared;
      }
      // the optimal policy (the policies themselves may differ on exact ties; the costs may not)
      const OptimalPolicyResult optimal_fast = optimal_state_policy(model, *fm, costs);
      const OptimalPolicyResult optimal_slow = optimal_state_policy_by_enumeration(model, *fm, costs);
      ExpectRel(optimal_fast.evaluation.long_term_mean_cost, optimal_slow.evaluation.long_term_mean_cost, 1e-10, "optimal " + fm->name());
      ExpectRel(evaluate_policy_by_enumeration(model, *fm, optimal_fast.repair_at, costs).long_term_mean_cost,
                optimal_fast.evaluation.long_term_mean_cost, 1e-10, "the fast optimum, evaluated the slow way");
    }
  }
  EXPECT_EQ(policies_compared, 7 * 5 * 6);
  EXPECT_GT(thresholds_compared, 100);
}

// Traceability: TC-J-12
// Beyond the reach of the enumeration code: 16 components. The costs are checked against closed forms that use only the
// system signature (corrective maintenance and every r-out-of-n policy), and the single-pass threshold search against the
// r-out-of-n family it must coincide with on a system of redundant pairs.
TEST(Policy, SixteenComponentSystemsAgreeWithTheSignatureClosedForms) {
  const int n = 16;
  // eight redundant pairs, every pair needed: operational states with k failures (k <= 8) are all equivalent
  const SystemModel model = SystemModel::from_function(n, [](StateMask x) {
    for (int pair = 0; pair < 8; ++pair) {
      if (((x >> (2 * pair)) & 3U) == 0U) {
        return false;
      }
    }
    return true;
  });
  const double lambda = 1.0;
  const IndependentExponential fm(lambda);
  const RepairCosts costs{0.2 / n, 1.0, 0.05};

  std::vector<double> r_costs;
  for (int r = 1; r <= 8; ++r) {
    const SignatureForm form = signature_closed_form(model, r, costs, lambda);
    const double expected = form.cost / form.length;
    const PolicyEvaluation got = evaluate_policy(model, fm, r_out_of_n_policy(model, r), costs);
    ExpectRel(got.long_term_mean_cost, expected, 1e-9, "r = " + std::to_string(r));
    r_costs.push_back(expected);
  }
  const SignatureForm corrective = signature_closed_form(model, n, costs, lambda);
  ExpectRel(evaluate_policy(model, fm, corrective_policy(model), costs).long_term_mean_cost, corrective.cost / corrective.length,
            1e-9, "corrective");
  r_costs.push_back(corrective.cost / corrective.length);

  // on this system the threshold policies ARE the r-out-of-n policies r = 1..8 and corrective maintenance
  const ThresholdSearch search = search_threshold_policies(model, fm, costs);
  ASSERT_EQ(search.policies.size(), 9U);
  std::vector<double> threshold_costs;
  for (const ThresholdPolicyResult& p : search.policies) {
    threshold_costs.push_back(p.evaluation.long_term_mean_cost);
  }
  std::sort(r_costs.begin(), r_costs.end());
  std::sort(threshold_costs.begin(), threshold_costs.end());
  for (std::size_t i = 0; i < r_costs.size(); ++i) {
    ExpectRel(threshold_costs[i], r_costs[i], 1e-9, "sorted cost " + std::to_string(i));
  }

  // the optimum is a lower bound of everything, and a simultaneous-failure model works at this size too
  const OptimalPolicyResult optimal = optimal_state_policy(model, fm, costs);
  EXPECT_LE(optimal.evaluation.long_term_mean_cost, *std::min_element(r_costs.begin(), r_costs.end()) * (1 + 1e-10));
  const auto lsh = make_lethal_shock(0.2, n);
  const ThresholdSearch under_lsh = search_threshold_policies(model, *lsh, costs);
  const OptimalPolicyResult optimal_lsh = optimal_state_policy(model, *lsh, costs);
  // (at n = 16 the lethal-shock kernel is accurate to about 1e-10 and the two routes accumulate that differently,
  // so this comparison allows 1e-7 rather than the 1e-10 used for independent failures)
  EXPECT_LE(optimal_lsh.evaluation.long_term_mean_cost,
            under_lsh.policies[under_lsh.best].evaluation.long_term_mean_cost * (1 + 1e-7));
}

// Traceability: TC-J-13
TEST(Policy, SizeLimitsOfTheLevelSumAndEnumerationCodes) {
  const IndependentExponential fm(1.0);
  const RepairCosts costs{0.1, 1.0, 0.0};
  // 15 components: too many for the enumeration code, fine for the level-sum code
  const SystemModel fifteen = SystemModel::parallel(kMaxEnumeratedPolicyComponents + 1);
  EXPECT_THROW(evaluate_policy_by_enumeration(fifteen, fm, corrective_policy(fifteen), costs), ModelError);
  EXPECT_THROW(search_threshold_policies_by_enumeration(fifteen, fm, costs), ModelError);
  EXPECT_THROW(optimal_state_policy_by_enumeration(fifteen, fm, costs), ModelError);
  EXPECT_NO_THROW(evaluate_policy(fifteen, fm, corrective_policy(fifteen), costs));
  // one more than the level-sum limit
  const SystemModel too_big = SystemModel::parallel(kMaxPolicyComponents + 1);
  EXPECT_THROW(search_threshold_policies(too_big, fm, costs), ModelError);
  EXPECT_THROW(optimal_state_policy(too_big, fm, costs), ModelError);
  EXPECT_THROW(r_out_of_n_policy(too_big, 1), ModelError);
}

// Traceability: TC-J-14
// The r-out-of-n policies from the class table (Proposition 3, no policy recursion) against the level-sum evaluation of the
// same policy as an explicit set of states, on random systems under five failure models; r = n is corrective maintenance.
TEST(Policy, ROutOfNPoliciesFromTheClassTableMatchTheLevelSumEvaluation) {
  std::mt19937 rng(8675309);
  std::vector<SystemModel> systems = {load_reference(), paper_figure_one(), bridge_network()};
  for (int i = 0; i < 5; ++i) {
    systems.push_back(random_monotone_system(6 + i, rng));
  }
  int compared = 0;
  for (const SystemModel& model : systems) {
    const int n = model.n();
    const std::vector<std::shared_ptr<FailureModel>> models = {
        std::make_shared<IndependentExponential>(1.0), make_lethal_shock(0.3, n), make_binomial_failure_rate(0.4, n),
        make_gamma_process(0.2, n), make_stable_subordinator(0.5, n)};
    for (const auto& fm : models) {
      const RepairCosts costs{0.12 / n, 1.0, 0.04};
      const PolicyClassTable table = build_policy_classes(model, *fm);
      const std::vector<RPolicyResult> from_classes = evaluate_r_out_of_n_policies(table, costs);
      ASSERT_EQ(from_classes.size(), static_cast<std::size_t>(n));
      for (const RPolicyResult& r : from_classes) {
        const PolicyEvaluation direct = evaluate_policy(model, *fm, r_out_of_n_policy(model, r.r), costs);
        ExpectRel(r.evaluation.mean_cycle_length, direct.mean_cycle_length, 1e-9, "length " + fm->name());
        ExpectRel(r.evaluation.mean_cycle_cost, direct.mean_cycle_cost, 1e-9, "cost " + fm->name());
        ExpectRel(r.evaluation.long_term_mean_cost, direct.long_term_mean_cost, 1e-9, "ltmc " + fm->name());
        ++compared;
      }
      const PolicyEvaluation corrective = evaluate_policy(model, *fm, corrective_policy(model), costs);
      ExpectRel(from_classes.back().evaluation.long_term_mean_cost, corrective.long_term_mean_cost, 1e-9, "r = n is corrective");
      // the table is a plain value: both families can be taken from it, in any order, with different costs
      const ThresholdSearch search = search_threshold_policies(table, RepairCosts{0.5 / n, 1.0, 0.0});
      EXPECT_EQ(search.policies.size(), search_threshold_policies(model, *fm, RepairCosts{0.5 / n, 1.0, 0.0}).policies.size());
    }
  }
  EXPECT_GT(compared, 200);
}
