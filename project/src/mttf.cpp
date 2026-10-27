// mttf.cpp - per-state MTTF from counting vectors.                 (Plan: Day 6)
#include "reliability/mttf.hpp"

#include <stdexcept>

#include "reliability/errors.hpp"

namespace reliability {

// ALGORITHM   (paper Eq. (7))
//   if W is empty: throw std::invalid_argument
//   if W[0] == 0: return 0.0                                      // system already failed
//   w = size(W) - 1
//   total = 0.0
//   for k = 0 .. w-1:
//       total += (W[k] / binomial(w, k)) * fm.increment(w, k)     // every term >= 0
//   return total
// W[k] and binomial are exact integers, so the only rounding is in the division and in the failure
// model's increments. All terms are non-negative, so nothing cancels.
double mttf_from_counting_vector(const CountingVector& w, const FailureModel& fm) {
  if (w.empty()) {
    throw std::invalid_argument("a counting vector needs at least one entry (W[0])");
  }
  if (w[0] == 0U) {
    return 0.0;
  }
  const int working = static_cast<int>(w.size()) - 1;
  const std::vector<double> tail = tail_signature(w);  // also validates W[k] <= C(w,k)
  double total = 0.0;
  for (int k = 0; k < working; ++k) {
    total += tail[static_cast<std::size_t>(k)] * fm.increment(working, k);
  }
  return total;
}

// ALGORITHM
//   m = vector<double>(2^n, 0.0)
//   sweep(model, observer = [&](x, W) { m[x] = mttf_from_counting_vector(W, fm); })
//   return m
// MTTF is computed inside the sweep, one state at a time, as in the paper's Algorithm 1.
// CHECKS (tests, not production code): m[x] >= 0, finite; m[x] == 0 where Phi(x) == 0;
//   m monotone: y <= x implies m[y] <= m[x]   (paper Lemma 4, holds for [Exp] and the six common-cause models of Table I).
std::vector<double> compute_all_mttf(const SystemModel& model, const FailureModel& fm) {
  std::vector<double> m(std::size_t{1} << model.n(), 0.0);
  sweep(model,
        [&](StateMask x, const CountingVector& w) { m[x] = mttf_from_counting_vector(w, fm); }, {});
  return m;
}

}  // namespace reliability
