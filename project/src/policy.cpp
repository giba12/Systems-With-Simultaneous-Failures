// policy.cpp - repair policies and their long-term cost.      (Plan: Day 9 optional P2; EXP-004)
#include "reliability/policy.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>

#include "level_index.hpp"
#include "reliability/errors.hpp"
#include "reliability/mttf.hpp"
#include "reliability/state.hpp"

namespace reliability {

namespace {

void require_policy_size(const SystemModel& model, const char* what) {
  if (model.n() < 2 || model.n() > kMaxPolicyComponents) {
    throw ModelError(std::string(what) + " needs between 2 and " + std::to_string(kMaxPolicyComponents) +
                     " components, got " + std::to_string(model.n()));
  }
}

void require_enumerated_size(const SystemModel& model, const char* what) {
  if (model.n() < 2 || model.n() > kMaxEnumeratedPolicyComponents) {
    throw ModelError(std::string(what) + " needs between 2 and " + std::to_string(kMaxEnumeratedPolicyComponents) +
                     " components, got " + std::to_string(model.n()));
  }
}

void require_valid_costs(const RepairCosts& costs) {
  for (double value : {costs.component, costs.system, costs.setup}) {
    if (!std::isfinite(value) || value < 0.0) {
      throw ModelError("repair costs must be finite and non-negative, got " + std::to_string(value));
    }
  }
}

// Operational ratios closer than this (relative) are treated as the same value: states whose MTTF is
// mathematically equal can differ in the last bits of a double.
constexpr double kRatioTolerance = 1e-12;

// Dinkelbach's method ends after finitely many passes; this is only a guard against a loop caused by rounding.
constexpr int kMaxOptimalPolicyPasses = 200;

}  // namespace

// ALGORITHM   see policy.hpp (enumeration)
PolicyEvaluation evaluate_policy_by_enumeration(const SystemModel& model, const FailureModel& failure_model,
                                                const RepairSet& repair_at, const RepairCosts& costs) {
  require_enumerated_size(model, "evaluate_policy_by_enumeration");
  require_valid_costs(costs);
  const int n = model.n();
  const std::uint64_t states = std::uint64_t{1} << n;
  if (repair_at.size() != states) {
    throw ModelError("the repair set must have one entry per state (" + std::to_string(states) + "), got " +
                     std::to_string(repair_at.size()));
  }

  std::vector<std::uint8_t> works(static_cast<std::size_t>(states));
  for (std::uint64_t x = 0; x < states; ++x) {
    works[static_cast<std::size_t>(x)] = model.phi(static_cast<StateMask>(x)) ? 1 : 0;
  }
  if (repair_at[static_cast<std::size_t>(states - 1)] != 0) {
    throw ModelError("a policy must not repair at the all-working state (the system would never run)");
  }
  for (std::uint64_t x = 0; x < states; ++x) {
    if (works[static_cast<std::size_t>(x)] == 0 && repair_at[static_cast<std::size_t>(x)] == 0) {
      throw ModelError("a policy must repair at every failed state; state " + std::to_string(x) + " has failed");
    }
  }

  // weight[w][v] = P(w, v) / C(w, w - v): probability that the next event leads to one SPECIFIC sub-state with v working
  std::vector<std::vector<double>> weight(static_cast<std::size_t>(n) + 1,
                                          std::vector<double>(static_cast<std::size_t>(n) + 1, 0.0));
  for (int w = 1; w <= n; ++w) {
    for (int v = 0; v < w; ++v) {
      weight[static_cast<std::size_t>(w)][static_cast<std::size_t>(v)] =
          failure_model.transition_prob(w, v) / static_cast<double>(binomial(w, w - v));
    }
  }

  std::vector<double> length(static_cast<std::size_t>(states), 0.0);
  std::vector<double> cost(static_cast<std::size_t>(states), 0.0);
  for (std::uint64_t xi = 0; xi < states; ++xi) {
    const StateMask x = static_cast<StateMask>(xi);
    const int w = working_count(x);
    if (repair_at[static_cast<std::size_t>(xi)] != 0) {
      cost[static_cast<std::size_t>(xi)] = costs.setup + (n - w) * costs.component +
                                           (works[static_cast<std::size_t>(xi)] == 0 ? costs.system : 0.0);
      continue;  // length stays 0: the cycle ends the moment the system arrives here
    }
    const double nu = failure_model.exit_rate(w);
    double acc_length = 1.0 / nu;
    double acc_cost = 0.0;
    for (StateMask y = (x - 1U) & x;; y = (y - 1U) & x) {
      const double p = weight[static_cast<std::size_t>(w)][static_cast<std::size_t>(working_count(y))];
      if (p != 0.0) {
        acc_length += p * length[y];
        acc_cost += p * cost[y];
      }
      if (y == 0U) {
        break;
      }
    }
    length[static_cast<std::size_t>(xi)] = acc_length;
    cost[static_cast<std::size_t>(xi)] = acc_cost;
  }

  PolicyEvaluation result;
  result.mean_cycle_length = length[static_cast<std::size_t>(states - 1)];
  result.mean_cycle_cost = cost[static_cast<std::size_t>(states - 1)];
  result.long_term_mean_cost = result.mean_cycle_cost / result.mean_cycle_length;
  return result;
}

// ALGORITHM   repair exactly at the failed states
RepairSet corrective_policy(const SystemModel& model) {
  require_policy_size(model, "corrective_policy");
  RepairSet repair(std::size_t{1} << model.n(), 0);
  for (std::size_t x = 0; x < repair.size(); ++x) {
    repair[x] = model.phi(static_cast<StateMask>(x)) ? 0 : 1;
  }
  return repair;
}

// ALGORITHM   repair at the failed states and at every state with at least r failed components
RepairSet r_out_of_n_policy(const SystemModel& model, int r) {
  require_policy_size(model, "r_out_of_n_policy");
  if (r < 1 || r > model.n()) {
    throw ModelError("r must be between 1 and " + std::to_string(model.n()) + ", got " + std::to_string(r));
  }
  RepairSet repair = corrective_policy(model);
  for (std::size_t x = 0; x < repair.size(); ++x) {
    if (model.n() - working_count(static_cast<StateMask>(x)) >= r) {
      repair[x] = 1;
    }
  }
  return repair;
}

// ALGORITHM   paper Eq. (13): repair when MTTF(x) / MTTF(all working) < gamma; failed states have MTTF 0
RepairSet threshold_policy(const SystemModel& model, const std::vector<double>& mttf, double gamma) {
  require_policy_size(model, "threshold_policy");
  if (!(gamma > 0.0 && gamma <= 1.0)) {
    throw ModelError("gamma must lie in (0, 1], got " + std::to_string(gamma));
  }
  const std::size_t states = std::size_t{1} << model.n();
  if (mttf.size() != states) {
    throw ModelError("the MTTF vector must have one entry per state");
  }
  const double reference = mttf[states - 1];
  RepairSet repair(states, 0);
  for (std::size_t x = 0; x < states; ++x) {
    repair[x] = (mttf[x] / reference < gamma) ? 1 : 0;
  }
  repair[states - 1] = 0;  // gamma <= 1 never repairs a new system (its ratio is exactly 1)
  return repair;
}

// ALGORITHM   see policy.hpp
ThresholdSearch search_threshold_policies_by_enumeration(const SystemModel& model, const FailureModel& failure_model,
                                                         const RepairCosts& costs) {
  require_enumerated_size(model, "search_threshold_policies_by_enumeration");
  const std::vector<double> mttf = compute_all_mttf(model, failure_model);
  const std::size_t states = mttf.size();
  const double reference = mttf[states - 1];

  // distinct operational ratios, ascending; the last one is the new system's (1)
  std::vector<double> ratios;
  for (std::size_t x = 0; x < states; ++x) {
    if (model.phi(static_cast<StateMask>(x))) {
      ratios.push_back(mttf[x] / reference);
    }
  }
  std::sort(ratios.begin(), ratios.end());
  std::vector<double> breakpoints;
  for (double r : ratios) {
    if (breakpoints.empty() || r > breakpoints.back() * (1.0 + kRatioTolerance)) {
      breakpoints.push_back(r);
    }
  }

  ThresholdSearch search;
  for (std::size_t j = 0; j < breakpoints.size(); ++j) {
    // policy j repairs the failed states and the operational states whose ratio is <= breakpoints[j-1]
    RepairSet repair(states, 0);
    std::size_t count = 0;
    for (std::size_t x = 0; x < states; ++x) {
      const bool failed = !model.phi(static_cast<StateMask>(x));
      const bool below = j > 0 && mttf[x] / reference <= breakpoints[j - 1] * (1.0 + kRatioTolerance);
      repair[x] = (failed || below) ? 1 : 0;
    }
    repair[states - 1] = 0;
    for (std::uint8_t flag : repair) {
      count += flag;
    }
    ThresholdPolicyResult entry;
    entry.gamma_from = j == 0 ? 0.0 : breakpoints[j - 1];
    entry.gamma_to = breakpoints[j];
    entry.states_repaired = count;
    entry.evaluation = evaluate_policy_by_enumeration(model, failure_model, repair, costs);
    search.policies.push_back(entry);
  }
  for (std::size_t j = 1; j < search.policies.size(); ++j) {
    if (search.policies[j].evaluation.long_term_mean_cost <
        search.policies[search.best].evaluation.long_term_mean_cost * (1.0 - kRatioTolerance)) {
      search.best = j;
    }
  }
  return search;
}

// ALGORITHM   see policy.hpp: Dinkelbach's method over all state-based policies
OptimalPolicyResult optimal_state_policy_by_enumeration(const SystemModel& model, const FailureModel& failure_model,
                                                        const RepairCosts& costs) {
  require_enumerated_size(model, "optimal_state_policy_by_enumeration");
  require_valid_costs(costs);
  const int n = model.n();
  const std::size_t states = std::size_t{1} << n;

  std::vector<std::uint8_t> works(states);
  std::vector<double> repair_cost(states);  // C(x), Eq. (10)
  for (std::size_t x = 0; x < states; ++x) {
    const StateMask mask = static_cast<StateMask>(x);
    works[x] = model.phi(mask) ? 1 : 0;
    repair_cost[x] = costs.setup + (n - working_count(mask)) * costs.component + (works[x] == 0 ? costs.system : 0.0);
  }
  std::vector<std::vector<double>> weight(static_cast<std::size_t>(n) + 1,
                                          std::vector<double>(static_cast<std::size_t>(n) + 1, 0.0));
  for (int w = 1; w <= n; ++w) {
    for (int v = 0; v < w; ++v) {
      weight[static_cast<std::size_t>(w)][static_cast<std::size_t>(v)] =
          failure_model.transition_prob(w, v) / static_cast<double>(binomial(w, w - v));
    }
  }

  OptimalPolicyResult best;
  best.repair_at = corrective_policy(model);
  best.evaluation = evaluate_policy_by_enumeration(model, failure_model, best.repair_at, costs);
  double g = best.evaluation.long_term_mean_cost;

  std::vector<double> h(states, 0.0);
  for (int pass = 1; pass <= kMaxOptimalPolicyPasses; ++pass) {
    best.iterations = pass;
    RepairSet policy(states, 0);
    for (std::size_t xi = 0; xi < states; ++xi) {
      const StateMask x = static_cast<StateMask>(xi);
      if (works[xi] == 0) {
        h[xi] = repair_cost[xi];
        policy[xi] = 1;
        continue;
      }
      const int w = working_count(x);
      double cont = -g / failure_model.exit_rate(w);
      for (StateMask y = (x - 1U) & x;; y = (y - 1U) & x) {
        const double p = weight[static_cast<std::size_t>(w)][static_cast<std::size_t>(working_count(y))];
        if (p != 0.0) {
          cont += p * h[y];
        }
        if (y == 0U) {
          break;
        }
      }
      if (xi + 1 == states) {  // the all-working state always continues
        h[xi] = cont;
      } else if (repair_cost[xi] < cont - kRatioTolerance * std::max(std::abs(cont), repair_cost[xi])) {
        h[xi] = repair_cost[xi];
        policy[xi] = 1;
      } else {
        h[xi] = cont;
      }
    }
    const PolicyEvaluation evaluation = evaluate_policy_by_enumeration(model, failure_model, policy, costs);
    if (!(evaluation.long_term_mean_cost < g * (1.0 - kRatioTolerance))) {
      break;  // g did not decrease: the policy held in `best` is optimal
    }
    g = evaluation.long_term_mean_cost;
    best.repair_at = std::move(policy);
    best.evaluation = evaluation;
  }
  for (std::uint8_t flag : best.repair_at) {
    best.states_repaired += flag;
  }
  return best;
}

namespace {

// coef[w][j] = P(w, w-j) / C(w, j): the chance that the next event leads to ONE given sub-state with j fewer working
// components. Entry 0 of each row is unused.
std::vector<std::vector<double>> sub_state_coefficients(int n, const FailureModel& failure_model) {
  std::vector<std::vector<double>> coef(static_cast<std::size_t>(n) + 1);
  for (int w = 1; w <= n; ++w) {
    coef[static_cast<std::size_t>(w)].assign(static_cast<std::size_t>(w) + 1, 0.0);
    for (int j = 1; j <= w; ++j) {
      coef[static_cast<std::size_t>(w)][static_cast<std::size_t>(j)] =
          failure_model.transition_prob(w, w - j) / static_cast<double>(binomial(w, j));
    }
  }
  return coef;
}

// ALGORITHM of level_pass   (the level sums of policy.hpp)
//   `channels` values are defined on the states. For every state x, level by level (w = |x| = 0 .. n):
//       sums[c][j] = (1/j) * sum over the sons s of x of S_s[c][j-1]       j = 1 .. w     (S_s[c][0] = value_c(s))
//   visit(x, w, sums, stride, out) reads sums[c * stride + j] (j >= 1) and writes the channel values of x into out[c];
//   the engine then keeps  S_x[c][0] = out[c], S_x[c][j] = sums[c][j]  for the next level. Only two levels are alive.
template <class Visit>
void level_pass(int n, int channels, Visit&& visit) {
  static const detail::BinomialTable binom;
  const std::size_t ch = static_cast<std::size_t>(channels);
  const std::size_t stride_sums = static_cast<std::size_t>(n) + 1;
  std::vector<double> previous;  // level w-1: per state, per channel, w entries (j = 0 .. w-1)
  std::vector<double> current;   // level w:   per state, per channel, w+1 entries
  std::vector<std::uint64_t> son_rank;
  std::vector<double> sums(ch * stride_sums, 0.0);
  std::vector<double> out(ch, 0.0);

  for (int w = 0; w <= n; ++w) {
    const std::vector<StateMask> states = masks_with_working(n, w);
    const std::size_t width = static_cast<std::size_t>(w) + 1;
    current.assign(states.size() * ch * width, 0.0);
    for (std::size_t r = 0; r < states.size(); ++r) {
      const StateMask x = states[r];
      detail::son_ranks(x, binom, son_rank);
      for (std::size_t c = 0; c < ch; ++c) {
        double* total = &sums[c * stride_sums];
        std::fill(total + 1, total + w + 1, 0.0);
        for (std::uint64_t sr : son_rank) {  // contiguous in j: S_s[c][0 .. w-1] adds to total[1 .. w]
          const double* son = &previous[(static_cast<std::size_t>(sr) * ch + c) * static_cast<std::size_t>(w)];
          for (int j = 1; j <= w; ++j) {
            total[j] += son[j - 1];
          }
        }
        for (int j = 1; j <= w; ++j) {
          total[j] /= static_cast<double>(j);
        }
      }
      visit(x, w, sums.data(), stride_sums, out.data());
      for (std::size_t c = 0; c < ch; ++c) {
        double* row = &current[(r * ch + c) * width];
        row[0] = out[c];
        for (int j = 1; j <= w; ++j) {
          row[j] = sums[c * stride_sums + static_cast<std::size_t>(j)];
        }
      }
    }
    previous.swap(current);
  }
}

std::vector<std::uint8_t> operational_flags(const SystemModel& model) {
  std::vector<std::uint8_t> works(std::size_t{1} << model.n());
  for (std::size_t x = 0; x < works.size(); ++x) {
    works[x] = model.phi(static_cast<StateMask>(x)) ? 1 : 0;
  }
  return works;
}

struct VectorHash {
  std::size_t operator()(const CountingVector& v) const {
    std::size_t h = 1469598103934665603ULL;
    for (std::uint64_t entry : v) {
      h = (h ^ static_cast<std::size_t>(entry)) * 1099511628211ULL;
    }
    return h;
  }
};

}  // namespace

// ALGORITHM   see policy.hpp (level sums)
PolicyEvaluation evaluate_policy(const SystemModel& model, const FailureModel& failure_model, const RepairSet& repair_at,
                                 const RepairCosts& costs) {
  require_policy_size(model, "evaluate_policy");
  require_valid_costs(costs);
  const int n = model.n();
  const std::size_t states = std::size_t{1} << n;
  if (repair_at.size() != states) {
    throw ModelError("the repair set must have one entry per state (" + std::to_string(states) + "), got " +
                     std::to_string(repair_at.size()));
  }
  const std::vector<std::uint8_t> works = operational_flags(model);
  if (repair_at[states - 1] != 0) {
    throw ModelError("a policy must not repair at the all-working state (the system would never run)");
  }
  for (std::size_t x = 0; x < states; ++x) {
    if (works[x] == 0 && repair_at[x] == 0) {
      throw ModelError("a policy must repair at every failed state; state " + std::to_string(x) + " has failed");
    }
  }
  const std::vector<std::vector<double>> coef = sub_state_coefficients(n, failure_model);

  double cycle_length = 0.0;
  double cycle_cost = 0.0;
  level_pass(n, 2, [&](StateMask x, int w, const double* sums, std::size_t stride, double* out) {
    if (repair_at[x] != 0) {
      out[0] = 0.0;  // the cycle ends the moment the system arrives here
      out[1] = costs.setup + (n - w) * costs.component + (works[x] == 0 ? costs.system : 0.0);
    } else {
      double length = 1.0 / failure_model.exit_rate(w);
      double cost = 0.0;
      for (int j = 1; j <= w; ++j) {
        const double p = coef[static_cast<std::size_t>(w)][static_cast<std::size_t>(j)];
        length += p * sums[static_cast<std::size_t>(j)];
        cost += p * sums[stride + static_cast<std::size_t>(j)];
      }
      out[0] = length;
      out[1] = cost;
    }
    if (x + 1 == states) {
      cycle_length = out[0];
      cycle_cost = out[1];
    }
  });
  PolicyEvaluation result;
  result.mean_cycle_length = cycle_length;
  result.mean_cycle_cost = cycle_cost;
  result.long_term_mean_cost = cycle_cost / cycle_length;
  return result;
}

// ALGORITHM   see policy.hpp (Proposition 3): one sweep, the classes and what each of their states contributes
PolicyClassTable build_policy_classes(const SystemModel& model, const FailureModel& failure_model) {
  require_policy_size(model, "build_policy_classes");
  const int n = model.n();

  // classes of operational states with equal counting vectors, with their multiplicity
  std::unordered_map<CountingVector, std::uint64_t, VectorHash> class_count;
  sweep(model, [&](StateMask, const CountingVector& vector) {
    if (vector[0] == 0U) {
      return;
    }
    auto it = class_count.find(vector);
    if (it == class_count.end()) {
      class_count.emplace(vector, 1U);
    } else {
      ++it->second;
    }
  });

  std::vector<double> f(static_cast<std::size_t>(n) + 1, 0.0);  // chance that the working count ever equals w
  f[static_cast<std::size_t>(n)] = 1.0;
  for (int v = n - 1; v >= 0; --v) {
    for (int above = v + 1; above <= n; ++above) {
      f[static_cast<std::size_t>(v)] += f[static_cast<std::size_t>(above)] * failure_model.transition_prob(above, v);
    }
  }

  PolicyClassTable table;
  table.n = n;
  table.classes.reserve(class_count.size());
  double reference = 0.0;
  for (const auto& [vector, count] : class_count) {
    const int w = static_cast<int>(vector.size()) - 1;
    const double nu = failure_model.exit_rate(w);
    const double tau = f[static_cast<std::size_t>(w)] / (static_cast<double>(binomial(n, w)) * nu);
    double phi = 0.0;
    double failing = 0.0;
    for (int w2 = 0; w2 < w; ++w2) {
      const double p = failure_model.transition_prob(w, w2);
      const double working_fraction =
          static_cast<double>(vector[static_cast<std::size_t>(w - w2)]) / static_cast<double>(binomial(w, w - w2));
      phi += nu * (w - w2) * p;
      failing += nu * p * (1.0 - working_fraction);
    }
    PolicyClass entry;
    entry.working = w;
    entry.mttf_ratio = mttf_from_counting_vector(vector, failure_model);
    entry.count = count;
    entry.length = tau * static_cast<double>(count);
    entry.failures = tau * phi * static_cast<double>(count);
    entry.corrective = tau * failing * static_cast<double>(count);
    table.classes.push_back(entry);
    if (w == n) {
      reference = entry.mttf_ratio;  // the all-working state is a class of its own
    }
  }
  for (PolicyClass& c : table.classes) {
    c.mttf_ratio /= reference;
  }
  return table;
}

namespace {

// The policy that continues exactly on `continue_here` (an up-closed set of operational states): cycle length T,
// and cycle cost c_set + c_comp N + c_sys P, from the partial sums of the classes inside it
PolicyEvaluation cycle_of_continue_set(double length, double failures, double corrective, const RepairCosts& costs) {
  PolicyEvaluation e;
  e.mean_cycle_length = length;
  e.mean_cycle_cost = costs.setup + costs.component * failures + costs.system * corrective;
  e.long_term_mean_cost = e.mean_cycle_cost / length;
  return e;
}

}  // namespace

// ALGORITHM   see policy.hpp: sort the classes by MTTF, group equal ratios (one group per breakpoint), suffix sums
ThresholdSearch search_threshold_policies(const PolicyClassTable& table, const RepairCosts& costs) {
  require_valid_costs(costs);
  const std::uint64_t states = std::uint64_t{1} << table.n;
  std::vector<PolicyClass> classes = table.classes;
  std::sort(classes.begin(), classes.end(),
            [](const PolicyClass& a, const PolicyClass& b) { return a.mttf_ratio < b.mttf_ratio; });

  struct Group {
    double ratio = 0.0;
    std::uint64_t count = 0;
    double length = 0.0, failures = 0.0, corrective = 0.0;
  };
  std::vector<Group> groups;
  for (const PolicyClass& c : classes) {
    if (groups.empty() || c.mttf_ratio > groups.back().ratio * (1.0 + kRatioTolerance)) {
      groups.push_back(Group{c.mttf_ratio, 0, 0.0, 0.0, 0.0});
    }
    groups.back().count += c.count;
    groups.back().length += c.length;
    groups.back().failures += c.failures;
    groups.back().corrective += c.corrective;
  }

  // policy j continues on the groups j, j+1, ...  (it repairs the failed states and the groups below j)
  ThresholdSearch search;
  search.policies.resize(groups.size());
  double length = 0.0, failures = 0.0, corrective = 0.0;
  std::uint64_t continuing = 0;
  for (std::size_t j = groups.size(); j-- > 0;) {
    length += groups[j].length;
    failures += groups[j].failures;
    corrective += groups[j].corrective;
    continuing += groups[j].count;
    ThresholdPolicyResult& entry = search.policies[j];
    entry.gamma_from = j == 0 ? 0.0 : groups[j - 1].ratio;
    entry.gamma_to = groups[j].ratio;
    entry.states_repaired = static_cast<std::size_t>(states - continuing);
    entry.evaluation = cycle_of_continue_set(length, failures, corrective, costs);
  }
  for (std::size_t j = 1; j < search.policies.size(); ++j) {
    if (search.policies[j].evaluation.long_term_mean_cost <
        search.policies[search.best].evaluation.long_term_mean_cost * (1.0 - kRatioTolerance)) {
      search.best = j;
    }
  }
  return search;
}

ThresholdSearch search_threshold_policies(const SystemModel& model, const FailureModel& failure_model,
                                          const RepairCosts& costs) {
  require_policy_size(model, "search_threshold_policies");
  return search_threshold_policies(build_policy_classes(model, failure_model), costs);
}

// ALGORITHM   see policy.hpp: the r-out-of-n policy continues on the operational states with more than n - r
//   working components, an up-closed set, so Proposition 3 applies; r = n is corrective maintenance
std::vector<RPolicyResult> evaluate_r_out_of_n_policies(const PolicyClassTable& table, const RepairCosts& costs) {
  require_valid_costs(costs);
  std::vector<RPolicyResult> results;
  for (int r = 1; r <= table.n; ++r) {
    double length = 0.0, failures = 0.0, corrective = 0.0;
    for (const PolicyClass& c : table.classes) {
      if (c.working > table.n - r) {
        length += c.length;
        failures += c.failures;
        corrective += c.corrective;
      }
    }
    results.push_back({r, cycle_of_continue_set(length, failures, corrective, costs)});
  }
  return results;
}

std::vector<RPolicyResult> evaluate_r_out_of_n_policies(const SystemModel& model, const FailureModel& failure_model,
                                                        const RepairCosts& costs) {
  require_policy_size(model, "evaluate_r_out_of_n_policies");
  return evaluate_r_out_of_n_policies(build_policy_classes(model, failure_model), costs);
}

// ALGORITHM   see policy.hpp (Dinkelbach, level sums)
OptimalPolicyResult optimal_state_policy(const SystemModel& model, const FailureModel& failure_model,
                                         const RepairCosts& costs) {
  require_policy_size(model, "optimal_state_policy");
  require_valid_costs(costs);
  const int n = model.n();
  const std::size_t states = std::size_t{1} << n;
  const std::vector<std::uint8_t> works = operational_flags(model);
  const std::vector<std::vector<double>> coef = sub_state_coefficients(n, failure_model);

  OptimalPolicyResult best;
  best.repair_at = corrective_policy(model);
  best.evaluation = evaluate_policy(model, failure_model, best.repair_at, costs);
  double g = best.evaluation.long_term_mean_cost;

  for (int pass = 1; pass <= kMaxOptimalPolicyPasses; ++pass) {
    best.iterations = pass;
    RepairSet policy(states, 0);
    PolicyEvaluation evaluation;
    // channels: 0 = h, 1 = cycle length of the policy being built, 2 = its cycle cost
    level_pass(n, 3, [&](StateMask x, int w, const double* sums, std::size_t stride, double* out) {
      const double repair_cost = costs.setup + (n - w) * costs.component + (works[x] == 0 ? costs.system : 0.0);
      if (works[x] == 0) {
        policy[x] = 1;
        out[0] = repair_cost;
        out[1] = 0.0;
        out[2] = repair_cost;
        return;
      }
      const double nu = failure_model.exit_rate(w);
      double cont = -g / nu;
      double length = 1.0 / nu;
      double cost = 0.0;
      for (int j = 1; j <= w; ++j) {
        const double p = coef[static_cast<std::size_t>(w)][static_cast<std::size_t>(j)];
        cont += p * sums[static_cast<std::size_t>(j)];
        length += p * sums[stride + static_cast<std::size_t>(j)];
        cost += p * sums[2 * stride + static_cast<std::size_t>(j)];
      }
      const bool full = static_cast<std::size_t>(x) + 1 == states;  // the all-working state always continues
      if (!full && repair_cost < cont - kRatioTolerance * std::max(std::abs(cont), repair_cost)) {
        policy[x] = 1;
        out[0] = repair_cost;
        out[1] = 0.0;
        out[2] = repair_cost;
      } else {
        out[0] = cont;
        out[1] = length;
        out[2] = cost;
      }
      if (full) {
        evaluation.mean_cycle_length = length;
        evaluation.mean_cycle_cost = cost;
        evaluation.long_term_mean_cost = cost / length;
      }
    });
    if (!(evaluation.long_term_mean_cost < g * (1.0 - kRatioTolerance))) {
      break;  // g did not decrease: the policy held in `best` is optimal
    }
    g = evaluation.long_term_mean_cost;
    best.repair_at = std::move(policy);
    best.evaluation = evaluation;
  }
  for (std::uint8_t flag : best.repair_at) {
    best.states_repaired += flag;
  }
  return best;
}

}  // namespace reliability
