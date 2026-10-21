// signature.hpp - the paper's Algorithm 1: counting vectors and signatures at EVERY state.
//                                       (Plan: Days 4-5; ALG-002, ALG-003, ALG-004, SYS-002)
//
// IDEA
//   For a state x with w working components, the "counting vector" W^x has w+1 entries:
//       W^x[k] = how many ways are there to switch off exactly k of the working components
//                and still have a system that works?
//   Example (reference satellite, everything working): W = [1, 6, 12, 8, 0, 0, 0]
//       k=0: 1 way (switch off nothing).      k=1: 6 (any one unit may fail).
//       k=2: 12 (15 pairs minus the 3 pairs that are a whole redundant pair).
//       k=3: 8 (one from each pair).          k>=4: 0 (a pair would be gone).
//   The trick (Lemma 1): W^x[k] = (1/k) * sum over the "sons" of x of W^son[k-1].
//   A son is x with one working component failed. Each case is seen by exactly k sons,
//   hence the division by k. So we never need to recount from scratch.
//
//   Tail signature     Sbar^x[k] = W^x[k] / C(|x|, k)     = fraction of k-failure cases that survive
//   Signature          s^x[k]    = Sbar^x[k-1] - Sbar^x[k]  = chance the system dies at failure number k
//
// PSEUDO-CODE for sweep(model, observer, options)   (paper Algorithm 1)
//   n = model.n();   model.reset_phi_calls();
//   previous_level = empty map  (state -> counting vector)
//   for failures f = n down to 0:                       // start at "all failed", end at "all working"
//       w = n - f                                       // working components at this level
//       current_level = empty map
//       for each state x in masks_with_working(n, w):
//           W = vector of w+1 numbers
//           W[0] = model.phi(x)                         // the ONLY place Phi is asked: once per state
//           for k = 1 .. w:
//               total = 0
//               for each son s of x:  total += previous_level[s][k-1]
//               assert total % k == 0                   // must divide exactly: cheap bug detector
//               W[k] = total / k
//           observer(x, W)                              // let the MTTF engine compute m(x) now
//           if w >= 1 and W[0] == 1 and W[1] == 0:      // working, but every son has failed
//               record x as a minimal path set          // free by-product (paper Section IV)
//           if options.keep_all_vectors: store W in the result
//           current_level[x] = W
//       previous_level = current_level                  // forget the older level: only 2 levels in memory
//       track the largest number of vectors held at once
//   assert model.phi_calls() == 2^n
//   return result
//
//   NOTE: the paper's Algorithm 1 sets W^0 = [0] for the all-failed state without calling
//   Phi, while Theorem 2.1 says "exactly 2^n evaluations". We call Phi(0) too (and check it
//   is 0), so the count is exactly 2^n. Confirm this reading on Day 4.
//
// PSEUDO-CODE for tail_signature(W) and signature(W)
//   w = len(W) - 1
//   Sbar[k] = W[k] / binomial(w, k)   for k = 0..w
//   s[k]    = Sbar[k-1] - Sbar[k]     for k = 1..w
//   (if Phi(x) == 0 then W is all zeros and the signature is all zeros / undefined: return empty)
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "reliability/state.hpp"
#include "reliability/system_model.hpp"

namespace reliability {

using CountingVector = std::vector<std::uint64_t>;  // W^x[0..|x|]

// Called once per state, in sweep order, as soon as that state's vector is known.
using StateObserver = std::function<void(StateMask, const CountingVector&)>;

struct SweepOptions {
  bool keep_all_vectors = false;  // tests only; the real sweep keeps two levels
};

struct SweepResult {
  std::vector<CountingVector> vectors;       // indexed by mask; filled only if keep_all_vectors
  std::vector<StateMask> minimal_path_sets;  // states with W0 == 1 and W1 == 0
  std::uint64_t phi_calls = 0;               // must equal 2^n
  std::size_t peak_vectors_held = 0;         // evidence for the two-level memory claim
};

SweepResult sweep(const SystemModel& model, const StateObserver& observer,
                  const SweepOptions& options = {});

std::vector<double> tail_signature(const CountingVector& w);
std::vector<double> signature(const CountingVector& w);

}  // namespace reliability
