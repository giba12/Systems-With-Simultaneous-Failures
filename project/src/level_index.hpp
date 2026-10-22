// level_index.hpp - where a state sits among the states with the same number of working components.
//                                                         (private header of the library; Plan: Days 4 and 10+)
//
// IDEA
//   The level sweep (signature.cpp) and the level-sum passes (policy.cpp) both walk the states one level at a time,
//   where a level holds all states with w working components, and look up a state's sons (one component fewer)
//   in the previous level. A flat array per level is indexed by the state's RANK in its level.
//
// ALGORITHM of rank_in_level
//   the states of a level in increasing numeric order are in "colexicographic" order, and the rank of the set
//   {c1 < c2 < ... < cw} of working bit positions is  C(c1,1) + C(c2,2) + ... + C(cw,w)
//   (the combinatorial number system), so no hash map is needed.
#pragma once

#include <array>
#include <cstdint>

#include "reliability/state.hpp"

namespace reliability::detail {

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

inline std::uint64_t rank_in_level(StateMask x, const BinomialTable& binom) {
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

}  // namespace reliability::detail
