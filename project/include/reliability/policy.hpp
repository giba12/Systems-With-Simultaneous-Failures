// policy.hpp - repair policies and their long-term cost.      (Plan: Day 9 optional P2; EXP-004)
//
// IDEA
//   Once the system is observed in a partially failed state x, an operator can keep running ("continue") or
//   replace every failed component ("repair"). Repairs bring the system back to the all-working state, so the
//   life of the system is a sequence of independent cycles (a renewal process) and its long-term mean cost is
//       LTMC = (expected cost of one cycle) / (expected length of one cycle)        (paper Eq. (12))
//   A repair at state x costs   C(x) = c_set + (n - |x|) c_comp + [Phi(x) = 0] c_sys        (Eq. (10)):
//   a fixed set-up cost, a price per replaced component, and a penalty if the system had already failed.
//
// THE POLICIES
//   state-based policy       any set of states at which to repair (never the all-working state; always every failed state)
//   corrective maintenance   repair only when the system has failed
//   r-out-of-n policy        repair as soon as r components have failed (or the system has)        [paper Section VII-E]
//   MTTF-threshold policy    repair when MTTF(x) / MTTF(all working) < gamma                        (Eq. (13))
//
// ALGORITHM of evaluate_policy   (paper Eqs. (9) and (11); the chain only moves to sub-states, so there is no cycle:
//   process the states in increasing numeric order, every proper sub-state of x is a smaller number)
//   for each state x, with w = |x|:
//       if repair(x):          length(x) = 0                         cost(x) = C(x)
//       otherwise:             length(x) = 1/nu_w + sum_y P(w,|y|) / C(w, w-|y|) * length(y)
//                              cost(x)   =          sum_y P(w,|y|) / C(w, w-|y|) * cost(y)
//                              (y runs over the proper sub-states of x)
//   result: cycle length = length(all), cycle cost = cost(all), LTMC = cost / length
//
// ALGORITHM of search_threshold_policies
//   ratio(x) = MTTF(x) / MTTF(all working), 0 for failed states.
//   The distinct values of the operational ratios, r_1 < ... < r_k = 1, are the breakpoints: pi_gamma repairs the
//   states with ratio < gamma, so for gamma in (r_j, r_{j+1}] it repairs exactly the operational states with ratio <= r_j
//   (plus the failed states). That gives k distinct policies (j = 0 is corrective maintenance, gamma in (0, r_1]).
//   EVERY one of them is evaluated: the paper shows that the cost curve over gamma is not unimodal, so a search that
//   stops when the cost first rises can miss the best threshold.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "reliability/failure_model.hpp"
#include "reliability/system_model.hpp"

namespace reliability {

// Policies are evaluated by enumerating sub-states (about 3^n work per policy).
inline constexpr int kMaxPolicyComponents = 14;

struct RepairCosts {
  double component = 0.0;  // c_comp: cost of replacing one failed component
  double system = 1.0;     // c_sys : extra cost when the system has failed
  double setup = 0.0;      // c_set : fixed cost of any repair visit
};

struct PolicyEvaluation {
  double mean_cycle_length = 0.0;
  double mean_cycle_cost = 0.0;
  double long_term_mean_cost = 0.0;
};

// repair_at[x] != 0  means "repair as soon as the system reaches state x".
using RepairSet = std::vector<std::uint8_t>;

// Throws ModelError if: n is outside 2..kMaxPolicyComponents; the set has the wrong size; it repairs at the
// all-working state or fails to repair at a failed state; a cost is negative or not finite.
PolicyEvaluation evaluate_policy(const SystemModel& model, const FailureModel& failure_model,
                                 const RepairSet& repair_at, const RepairCosts& costs);

RepairSet corrective_policy(const SystemModel& model);
RepairSet r_out_of_n_policy(const SystemModel& model, int r);  // 1 <= r <= n
RepairSet threshold_policy(const SystemModel& model, const std::vector<double>& mttf, double gamma);

struct ThresholdPolicyResult {
  double gamma_from = 0.0;           // the policy is pi_gamma for gamma in (gamma_from, gamma_to]
  double gamma_to = 0.0;
  std::size_t states_repaired = 0;   // states at which it repairs, including the failed ones
  PolicyEvaluation evaluation;
};

struct ThresholdSearch {
  std::vector<ThresholdPolicyResult> policies;  // in order of increasing gamma
  std::size_t best = 0;                         // index of the cheapest (the one that repairs least on ties)
};

ThresholdSearch search_threshold_policies(const SystemModel& model, const FailureModel& failure_model,
                                          const RepairCosts& costs);

struct RPolicyResult {
  int r = 0;
  PolicyEvaluation evaluation;
};

// r = 1 .. n
std::vector<RPolicyResult> evaluate_r_out_of_n_policies(const SystemModel& model, const FailureModel& failure_model,
                                                        const RepairCosts& costs);

}  // namespace reliability
