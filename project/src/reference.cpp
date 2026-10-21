// reference.cpp - brute-force checker. MUST NOT use sweep() or the Lemma 1 recurrence.
//                                                                   (Plan: Day 3)
#include "reliability/reference.hpp"

#include "reliability/errors.hpp"

namespace reliability {

// PSEUDO-CODE
//   result = vector of 2^n counting vectors
//   for x in 0 .. 2^n-1:
//       bits = positions of the 1-bits of x
//       result[x] = vector of size(bits)+1 zeros
//       for subset_mask in 0 .. 2^size(bits)-1:               // every way to fail some of them
//           k = number of 1-bits in subset_mask
//           y = x with the chosen bits cleared
//           result[x][k] += phi(y)
//   return result
// NOTE: enumerating subsets by a counter keeps this obviously correct and clearly
//       different from the paper's recurrence.
std::vector<CountingVector> brute_force_counting_vectors(const SystemModel& /*model*/) {
  not_implemented("brute_force_counting_vectors");
}

// PSEUDO-CODE
//   result = []
//   for x in 1 .. 2^n-1:
//       if not phi(x): continue
//       minimal = true
//       for each 1-bit i of x:
//           if phi(without(x, i)): minimal = false; break
//       if minimal: result.append(x)
//   return result
std::vector<StateMask> brute_force_minimal_path_sets(const SystemModel& /*model*/) {
  not_implemented("brute_force_minimal_path_sets");
}

// PSEUDO-CODE   (paper Eq. (6); see reference.hpp)
//   m = vector<double>(2^n, 0)
//   for w = 1 .. n:
//       for x in masks_with_working(n, w):
//           if not phi(x): m[x] = 0; continue
//           acc = 1 / fm.exit_rate(w)
//           for w2 = 0 .. w-1:
//               p = fm.transition_prob(w, w2)
//               if p == 0: continue
//               s = sum of m[y] over every y whose bits are a subset of x and |y| = w2
//               acc += p / binomial(w, w - w2) * s
//           m[x] = acc
//   return m
std::vector<double> solve_mttf_direct(const SystemModel& /*model*/, const FailureModel& /*fm*/) {
  not_implemented("solve_mttf_direct");
}

}  // namespace reliability
