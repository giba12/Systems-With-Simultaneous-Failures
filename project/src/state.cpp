// state.cpp - the non-trivial state helpers.                      (Plan: Day 2)
#include "reliability/state.hpp"

#include "reliability/errors.hpp"

namespace reliability {

// PSEUDO-CODE
//   result = empty list
//   for each bit i from 0 up to 31:
//       if bit i of x is 1: append without(x, i)
//   return result            // exactly working_count(x) states; empty for x == 0
std::vector<StateMask> sons(StateMask /*x*/) { not_implemented("sons"); }

// PSEUDO-CODE   (Gosper's hack: next number with the same count of 1-bits)
//   if w < 0 or w > n: return empty list
//   if w == 0: return [0]
//   x = lowest w bits set                      // smallest n-bit number with w ones
//   limit = 2^n
//   while x < limit:
//       append x
//       c = x & -x                             // lowest set bit
//       r = x + c                              // ripple the lowest block of ones upward
//       x = (((r ^ x) >> 2) / c) | r           // refill the low bits with the leftover ones
//   return list                                // C(n, w) numbers, increasing
std::vector<StateMask> masks_with_working(int /*n*/, int /*w*/) {
  not_implemented("masks_with_working");
}

// PSEUDO-CODE
//   if k < 0 or k > n: return 0
//   k = min(k, n - k)
//   result = 1
//   for i = 1 .. k:
//       result = result * (n - k + i) / i      // stays an exact integer at every step
//   return result
std::uint64_t binomial(int /*n*/, int /*k*/) { not_implemented("binomial"); }

}  // namespace reliability
