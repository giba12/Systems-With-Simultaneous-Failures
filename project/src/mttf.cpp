// mttf.cpp - per-state MTTF from counting vectors.                 (Plan: Day 6)
#include "reliability/mttf.hpp"

#include "reliability/errors.hpp"

namespace reliability {

// PSEUDO-CODE
//   if W is empty: throw std::invalid_argument
//   if W[0] == 0: return 0.0
//   w = size(W) - 1
//   total = 0.0
//   for k = 0 .. w-1:
//       total += (W[k] / binomial(w, k)) * fm.increment(w, k)     // every term >= 0
//   return total
// NOTES: use double; W[k] and binomial are exact integers, so the only rounding is in the
//        division and the failure model's increments.
double mttf_from_counting_vector(const CountingVector& /*w*/, const FailureModel& /*fm*/) {
  not_implemented("mttf_from_counting_vector");
}

// PSEUDO-CODE
//   m = vector<double>(2^n, 0.0)
//   sweep(model,
//         observer = [&](x, W) { m[x] = mttf_from_counting_vector(W, fm); })
//   return m
// CHECKS (tests, not production code): m[x] >= 0, finite; m[x] == 0 where Phi(x) == 0;
//   m monotone: y <= x implies m[y] <= m[x]   (paper Lemma 4, holds for [Exp] and [LSh]).
std::vector<double> compute_all_mttf(const SystemModel& /*model*/, const FailureModel& /*fm*/) {
  not_implemented("compute_all_mttf");
}

}  // namespace reliability
