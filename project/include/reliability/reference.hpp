// reference.hpp - the INDEPENDENT second opinion.           (Plan: Day 3; ALG-005, REL-005)
//
// IDEA
//   If the clever algorithm and its checker share the same idea, they can share the same
//   bug. So this module computes the same answers the slow, obvious way and must never
//   call the recurrence from signature.cpp. Tests compare the two on every state.
//
// PSEUDO-CODE
//   brute_force_counting_vectors(model):
//       for every state x (0 .. 2^n - 1):
//           working = list of bit positions that are 1 in x
//           for k = 0 .. len(working):
//               count = 0
//               for every k-element subset S of `working`:
//                   y = x with every bit in S cleared
//                   count += model.phi(y)
//               W[x][k] = count
//       return W
//       (cost ~ 3^n; only for small n)
//
//   brute_force_minimal_path_sets(model):
//       for every state x with Phi(x) = 1:
//           x is minimal  iff  Phi(y) == 0 for EVERY proper sub-state y of x
//           (not just the sons: this is the paper's definition, so it does not rely on
//            monotonicity or on the sweep's W0 = 1 and W1 = 0 shortcut)
//       return the list
//
//   solve_mttf_direct(model, failure_model):      // paper Eq. (6)   (Day 6: needs the failure model)
//       m[0] = 0
//       for states in increasing order of working count w:
//           if Phi(x) == 0: m[x] = 0
//           else:
//               m[x] = 1 / nu(w)
//                      + sum over w2 = 0 .. w-1 of
//                          P(w, w2) / C(w, w - w2) * sum of m[y] over states y <= x with |y| = w2
//       return m
//       (a completely different route to the same MTTF: used to cross-check Eq. (7))
#pragma once

#include <vector>

#include "reliability/failure_model.hpp"
#include "reliability/signature.hpp"
#include "reliability/state.hpp"
#include "reliability/system_model.hpp"

namespace reliability {

// The brute-force routines do about 3^n calls to Phi (every state x, every subset of its working
// components). 3^16 = 43 million is still fast; much beyond that is not, so larger systems are
// refused with a ModelError rather than silently running for hours.
inline constexpr int kMaxBruteForceComponents = 16;

// Note: these functions call model.phi(), so they advance the model's phi_calls() counter.
std::vector<CountingVector> brute_force_counting_vectors(const SystemModel& model);
std::vector<StateMask> brute_force_minimal_path_sets(const SystemModel& model);
std::vector<double> solve_mttf_direct(const SystemModel& model, const FailureModel& fm);

}  // namespace reliability
