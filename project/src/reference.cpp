// reference.cpp - brute-force checker. MUST NOT use sweep() or the Lemma 1 recurrence.
//                                                                   (Plan: Day 3)
#include "reliability/reference.hpp"

#include <string>

#include "reliability/errors.hpp"

namespace reliability {

namespace {

void require_small_enough(const SystemModel& model, const char* what) {
  if (model.n() > kMaxBruteForceComponents) {
    throw ModelError(std::string(what) + " does about 3^n work and is limited to " +
                     std::to_string(kMaxBruteForceComponents) + " components, got " +
                     std::to_string(model.n()));
  }
}

}  // namespace

// ALGORITHM
//   result = vector of 2^n counting vectors
//   for x in 0 .. 2^n-1:
//       bits = positions of the 1-bits of x
//       result[x] = vector of size(bits)+1 zeros
//       for subset_mask in 0 .. 2^size(bits)-1:               // every way to fail some of them
//           k = number of 1-bits in subset_mask
//           y = x with the chosen bits cleared
//           result[x][k] += phi(y)
//   return result
// NOTE: enumerating subsets by a counter keeps this obviously correct and clearly different
//       from the paper's recurrence. It uses none of sons(), masks_with_working() or binomial().
std::vector<CountingVector> brute_force_counting_vectors(const SystemModel& model) {
  require_small_enough(model, "brute_force_counting_vectors");
  const std::uint64_t states = std::uint64_t{1} << model.n();
  std::vector<CountingVector> result(static_cast<std::size_t>(states));
  for (std::uint64_t xi = 0; xi < states; ++xi) {
    const StateMask x = static_cast<StateMask>(xi);
    std::vector<int> bits;
    for (int i = 0; i < model.n(); ++i) {
      if (is_working(x, i)) {
        bits.push_back(i);
      }
    }
    CountingVector w(bits.size() + 1, 0);
    const std::uint64_t subsets = std::uint64_t{1} << bits.size();
    for (std::uint64_t subset = 0; subset < subsets; ++subset) {
      std::size_t failed = 0;
      StateMask y = x;
      for (std::size_t j = 0; j < bits.size(); ++j) {
        if (((subset >> j) & 1U) != 0U) {
          y = without(y, bits[j]);
          ++failed;
        }
      }
      if (model.phi(y)) {
        ++w[failed];
      }
    }
    result[static_cast<std::size_t>(xi)] = std::move(w);
  }
  return result;
}

// ALGORITHM
//   result = []
//   for x in 1 .. 2^n-1:
//       if not phi(x): continue
//       minimal = true
//       for every proper sub-state y of x (all sub-masks except x itself, including 0):
//           if phi(y): minimal = false; break
//       if minimal: result.append(x)
//   return result
// This is the paper's definition (Phi(1_A) = 1 and Phi(x') = 0 for every x' <= 1_A, x' != 1_A).
// It deliberately checks ALL proper sub-states, not only the sons, so it stays correct even for
// a non-monotone Phi and is independent of the sweep's "W0 = 1 and W1 = 0" shortcut.
std::vector<StateMask> brute_force_minimal_path_sets(const SystemModel& model) {
  require_small_enough(model, "brute_force_minimal_path_sets");
  const std::uint64_t states = std::uint64_t{1} << model.n();
  std::vector<StateMask> result;
  for (std::uint64_t xi = 1; xi < states; ++xi) {
    const StateMask x = static_cast<StateMask>(xi);
    if (!model.phi(x)) {
      continue;
    }
    bool minimal = true;
    // enumerate the proper sub-masks of x, from the largest down to 0
    for (StateMask y = (x - 1U) & x;; y = (y - 1U) & x) {
      if (model.phi(y)) {
        minimal = false;
        break;
      }
      if (y == 0U) {
        break;
      }
    }
    if (minimal) {
      result.push_back(x);
    }
  }
  return result;
}

// ALGORITHM   (paper Eq. (6); see reference.hpp)
//   works[x] = phi(x) for every state
//   m[0] = 0
//   for x in 1 .. 2^n-1, in numeric order        // every proper sub-state y of x has y < x, so it is ready
//       if not works[x]: m[x] = 0; continue
//       w = |x|
//       acc = 1 / nu_w
//       for every proper sub-state y of x (all sub-masks except x itself, including 0):
//           p = P(w, |y|)
//           if p != 0: acc += p / C(w, w - |y|) * m[y]
//       m[x] = acc
//   return m
// This never looks at counting vectors or signatures: it is the paper's defining linear system
// for the MTTF, solved state by state. Agreement with Eq. (7) is evidence that both are right.
std::vector<double> solve_mttf_direct(const SystemModel& model, const FailureModel& fm) {
  require_small_enough(model, "solve_mttf_direct");
  const std::uint64_t states = std::uint64_t{1} << model.n();
  std::vector<std::uint8_t> works(static_cast<std::size_t>(states));
  for (std::uint64_t x = 0; x < states; ++x) {
    works[static_cast<std::size_t>(x)] = model.phi(static_cast<StateMask>(x)) ? 1 : 0;
  }

  std::vector<double> m(static_cast<std::size_t>(states), 0.0);
  for (std::uint64_t xi = 1; xi < states; ++xi) {
    if (works[static_cast<std::size_t>(xi)] == 0) {
      continue;
    }
    const StateMask x = static_cast<StateMask>(xi);
    const int w = working_count(x);
    const double nu = fm.exit_rate(w);
    if (!(nu > 0.0)) {
      throw ModelError("state " + std::to_string(x) + " works but has exit rate " +
                       std::to_string(nu) + " (a working system needs at least one working component)");
    }
    double acc = 1.0 / nu;
    for (StateMask y = (x - 1U) & x;; y = (y - 1U) & x) {
      const int wy = working_count(y);
      const double p = fm.transition_prob(w, wy);
      if (p != 0.0) {
        acc += p / static_cast<double>(binomial(w, w - wy)) * m[y];
      }
      if (y == 0U) {
        break;
      }
    }
    m[x] = acc;
  }
  return m;
}

}  // namespace reliability
