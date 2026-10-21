// state.cpp - the non-trivial state helpers.                      (Plan: Day 2)
#include "reliability/state.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace reliability {

// ALGORITHM
//   result = empty list
//   for each bit i from 0 up to 31:
//       if bit i of x is 1: append without(x, i)
//   return result            // exactly working_count(x) states; empty for x == 0
std::vector<StateMask> sons(StateMask x) {
  std::vector<StateMask> result;
  result.reserve(static_cast<std::size_t>(working_count(x)));
  for (int i = 0; i < 32; ++i) {
    if (is_working(x, i)) {
      result.push_back(without(x, i));
    }
  }
  return result;
}

// ALGORITHM   (Gosper's hack: next number with the same count of 1-bits)
//   if n < 0 or n > 31 or w < 0 or w > n: return empty list
//   if w == 0: return [0]
//   x = lowest w bits set                      // smallest n-bit number with w ones
//   limit = 2^n
//   while x < limit:
//       append x
//       c = x & -x                             // lowest set bit
//       r = x + c                              // ripple the lowest block of ones upward
//       x = (((r ^ x) >> 2) / c) | r           // refill the low bits with the leftover ones
//   return list                                // C(n, w) numbers, increasing
// Arithmetic is done in 64 bits so that "x + c" cannot overflow when n is 31.
std::vector<StateMask> masks_with_working(int n, int w) {
  std::vector<StateMask> result;
  if (n < 0 || n > 31 || w < 0 || w > n) {
    return result;
  }
  if (w == 0) {
    result.push_back(0);
    return result;
  }
  result.reserve(static_cast<std::size_t>(binomial(n, w)));
  std::uint64_t x = (std::uint64_t{1} << w) - 1;
  const std::uint64_t limit = std::uint64_t{1} << n;
  while (x < limit) {
    result.push_back(static_cast<StateMask>(x));
    const std::uint64_t c = x & (~x + 1);
    const std::uint64_t r = x + c;
    x = (((r ^ x) >> 2) / c) | r;
  }
  return result;
}

// ALGORITHM
//   if k < 0 or k > n: return 0
//   k = min(k, n - k)
//   result = 1
//   for i = 1 .. k:
//       result = result * (n - k + i) / i      // stays an exact integer at every step
//   return result
// After step i, result = C(n - k + i, i), so the division is always exact. For n <= 60 the
// largest intermediate product is below 2^63, so nothing overflows; larger n is refused.
std::uint64_t binomial(int n, int k) {
  if (n < 0 || k < 0 || k > n) {
    return 0;
  }
  if (n > 60) {
    throw std::out_of_range("binomial: n = " + std::to_string(n) +
                            " is too large for an exact 64-bit result (maximum 60)");
  }
  k = std::min(k, n - k);
  std::uint64_t result = 1;
  for (int i = 1; i <= k; ++i) {
    result = result * static_cast<std::uint64_t>(n - k + i) / static_cast<std::uint64_t>(i);
  }
  return result;
}

}  // namespace reliability
