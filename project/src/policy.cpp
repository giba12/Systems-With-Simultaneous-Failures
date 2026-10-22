// policy.cpp - repair policies and their long-term cost.      (Plan: Day 9 optional P2; EXP-004)
#include "reliability/policy.hpp"

#include <algorithm>
#include <cmath>
#include <string>

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

}  // namespace

// ALGORITHM   see policy.hpp
PolicyEvaluation evaluate_policy(const SystemModel& model, const FailureModel& failure_model,
                                 const RepairSet& repair_at, const RepairCosts& costs) {
  require_policy_size(model, "evaluate_policy");
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
ThresholdSearch search_threshold_policies(const SystemModel& model, const FailureModel& failure_model,
                                          const RepairCosts& costs) {
  require_policy_size(model, "search_threshold_policies");
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
    entry.evaluation = evaluate_policy(model, failure_model, repair, costs);
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

// ALGORITHM   r = 1 .. n
std::vector<RPolicyResult> evaluate_r_out_of_n_policies(const SystemModel& model, const FailureModel& failure_model,
                                                        const RepairCosts& costs) {
  require_policy_size(model, "evaluate_r_out_of_n_policies");
  std::vector<RPolicyResult> results;
  for (int r = 1; r <= model.n(); ++r) {
    results.push_back({r, evaluate_policy(model, failure_model, r_out_of_n_policy(model, r), costs)});
  }
  return results;
}

}  // namespace reliability
