// signature.cpp - Algorithm 1 (the level sweep) and signature formulas.   (Plan: Days 4-5)
#include "reliability/signature.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

#include "reliability/errors.hpp"

namespace reliability {

namespace {

// Pascal's triangle up to 31 (a state mask holds at most 31 components); every entry fits in 64 bits.
struct BinomialTable {
  std::array<std::array<std::uint64_t, 32>, 32> c{};
  BinomialTable() {
    for (int a = 0; a < 32; ++a) {
      c[static_cast<std::size_t>(a)][0] = 1;
      for (int b = 1; b <= a; ++b) {
        c[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)] =
            c[static_cast<std::size_t>(a - 1)][static_cast<std::size_t>(b - 1)] +
            (b <= a - 1 ? c[static_cast<std::size_t>(a - 1)][static_cast<std::size_t>(b)] : 0U);
      }
    }
  }
  std::uint64_t operator()(int a, int b) const {
    return (b < 0 || b > a) ? 0U : c[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)];
  }
};

// Position of x among all states with the same number of working components, listed in increasing
// numeric order. That order is "colexicographic", and the rank of the set {c1 < c2 < ... < cw} of
// working bit positions is  C(c1,1) + C(c2,2) + ... + C(cw,w)  (combinatorial number system).
// It lets us find a son's vector in the previous level without a hash map.
std::uint64_t rank_in_level(StateMask x, const BinomialTable& binom) {
  std::uint64_t rank = 0;
  int j = 0;
  for (int bit = 0; bit < 32; ++bit) {
    if (is_working(x, bit)) {
      ++j;
      rank += binom(bit, j);
    }
  }
  return rank;
}

}  // namespace

// ALGORITHM   (paper Algorithm 1, Lemma 1, Theorem 2)
//   levels go from "no component works" (w = 0) up to "all work" (w = n), i.e. failures n .. 0
//   each level is one flat matrix: one row of w+1 numbers per state, rows in increasing mask order
//   only two levels exist at any moment: the previous one (w-1 working) and the current one (w)
//   for each state x of level w:
//       W[0] = phi(x)                          // the ONLY place Phi is asked: once per state
//       for each working bit i: rank of the son  x without i  in the previous level
//       for k = 1 .. w:  W[k] = (1/k) * sum over sons of  previous[son][k-1]
//       tell the observer; record x if W[0] == 1 and W[1] == 0 (minimal path set)
//   Lemma 1 divides exactly. A remainder means a bug (the check costs almost nothing).
// Nothing here needs Phi to be monotone: Lemma 1 is pure counting, so the sweep is also correct
// for an arbitrary Boolean function (the tests check that). Monotonicity is only needed for the
// meaning of "minimal path set" and for the theorems built on top of the vectors.
SweepResult sweep(const SystemModel& model, const StateObserver& observer,
                  const SweepOptions& options) {
  const int n = model.n();
  if (n < 1 || n > kMaxComponents) {
    throw ModelError("sweep needs between 1 and " + std::to_string(kMaxComponents) +
                     " components, got " + std::to_string(n));
  }

  static const BinomialTable binom;
  SweepResult result;
  if (options.keep_all_vectors) {
    result.vectors.resize(std::size_t{1} << n);
  }

  model.reset_phi_calls();

  // Entries stay below C(n, n/2) <= 300 million for n <= 31, so 32 bits are enough and halve the
  // memory of the two live levels.
  std::vector<std::uint32_t> previous;  // level w-1: row r has w entries
  std::vector<std::uint32_t> current;   // level w:   row r has w+1 entries
  CountingVector row;                   // reused, handed to the observer
  std::vector<std::uint64_t> son_rank;  // reused, ranks of the sons of the current state

  for (int w = 0; w <= n; ++w) {
    const std::vector<StateMask> states = masks_with_working(n, w);
    const std::size_t stride = static_cast<std::size_t>(w) + 1;
    current.assign(states.size() * stride, 0U);
    row.assign(stride, 0U);
    result.peak_vectors_held =
        std::max(result.peak_vectors_held, previous.size() / std::max<std::size_t>(w, 1) + states.size());

    for (std::size_t r = 0; r < states.size(); ++r) {
      const StateMask x = states[r];

      // sons of x in the previous level
      son_rank.clear();
      for (int i = 0; i < n; ++i) {
        if (is_working(x, i)) {
          son_rank.push_back(rank_in_level(without(x, i), binom));
        }
      }

      row[0] = model.phi(x) ? 1U : 0U;
      for (int k = 1; k <= w; ++k) {
        std::uint64_t total = 0;
        for (std::uint64_t sr : son_rank) {
          total += previous[sr * static_cast<std::uint64_t>(w) + static_cast<std::uint64_t>(k - 1)];
        }
        if (total % static_cast<std::uint64_t>(k) != 0U) {
          throw std::logic_error("Lemma 1 violated at state " + std::to_string(x) + ", k = " +
                                 std::to_string(k) + " (sum " + std::to_string(total) +
                                 " is not divisible by " + std::to_string(k) + ")");
        }
        row[static_cast<std::size_t>(k)] = total / static_cast<std::uint64_t>(k);
      }

      for (std::size_t k = 0; k < stride; ++k) {
        current[r * stride + k] = static_cast<std::uint32_t>(row[k]);
      }
      if (observer) {
        observer(x, row);
      }
      if (w >= 1 && row[0] == 1U && row[1] == 0U) {
        result.minimal_path_sets.push_back(x);
      }
      if (options.keep_all_vectors) {
        result.vectors[x] = row;
      }
    }
    previous.swap(current);  // the older level is dropped: only two levels are ever alive
  }

  result.phi_calls = model.phi_calls();
  if (result.phi_calls != std::uint64_t{1} << n) {
    throw std::logic_error("the sweep must evaluate Phi exactly 2^n times, but did " +
                           std::to_string(result.phi_calls) + " for n = " + std::to_string(n));
  }
  return result;
}

// PSEUDO-CODE   (Day 5)
//   w = size(W) - 1
//   return [ W[k] / binomial(w, k)  for k in 0..w ]       // as doubles
std::vector<double> tail_signature(const CountingVector& /*w*/) {
  not_implemented("tail_signature");
}

// PSEUDO-CODE   (Day 5)
//   if W[0] == 0: return empty list                        // system already failed
//   S = tail_signature(W)
//   return [ S[k-1] - S[k]  for k in 1..w ]                // each entry >= 0, total = 1
std::vector<double> signature(const CountingVector& /*w*/) { not_implemented("signature"); }

}  // namespace reliability
