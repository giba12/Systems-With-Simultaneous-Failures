// mttf.hpp - mean time to failure at every state.            (Plan: Day 6; REL-001, REL-005)
//
// IDEA
//   MTTF = how long, on average, the system keeps working starting from state x.
//   The paper turns the counting vector into a time (Eq. (7)):
//       m(x) = sum over k = 0 .. |x|-1 of   [ W^x[k] / C(|x|, k) ]  *  increment(|x|, k)
//   Read it as: "for each failure number k, take the chance the system is still alive
//   after k failures (the tail signature), and multiply by the average time that the
//   (k+1)-th failure takes to arrive."  A state where the system is already down has m = 0.
//
// ALGORITHM
//   mttf_from_counting_vector(W, failure_model):
//       w = len(W) - 1
//       if W[0] == 0: return 0                         // system already failed
//       total = 0
//       for k = 0 .. w-1:
//           total += (W[k] / binomial(w, k)) * failure_model.increment(w, k)
//       return total
//
//   compute_all_mttf(model, failure_model):
//       m = array of 2^n zeros
//       sweep(model, observer = (x, W) -> m[x] = mttf_from_counting_vector(W, failure_model))
//       return m                                       // m[63] is the "new system" MTTF
//
//   Sanity values (independent exponential, lambda = 1):
//       series of n units      -> 1/n
//       one-out-of-two         -> 1.5
//       reference satellite    -> 0.7   (state 63);  0.5333 with P1 failed (state 62)
#pragma once

#include <vector>

#include "reliability/failure_model.hpp"
#include "reliability/signature.hpp"
#include "reliability/system_model.hpp"

namespace reliability {

double mttf_from_counting_vector(const CountingVector& w, const FailureModel& fm);

// m[x] for every state mask x, computed with the level sweep.
std::vector<double> compute_all_mttf(const SystemModel& model, const FailureModel& fm);

}  // namespace reliability
