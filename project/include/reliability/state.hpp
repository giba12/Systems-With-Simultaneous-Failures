// state.hpp - how a system state is represented.        (Plan: Day 2; SYS-004, ALG-001)
//
// IDEA
//   A state is one whole number whose binary digits say which components work.
//   Bit i = 1 means component i works, bit i = 0 means it has failed
//   (the paper writes this as x in {0,1}^n, x_i = 1 when component i works).
//   With 6 components there are 2^6 = 64 states, numbered 0..63.
//   Mask 63 (0b111111) = everything works.  Mask 0 = everything has failed.
//
// PSEUDO-CODE for the helpers in this file
//   working_count(x)      = number of 1-bits in x                       (the paper's |x|)
//   all_working(n)        = a number with the lowest n bits set to 1     (the paper's vector 1)
//   is_working(x, i)      = is bit i of x equal to 1?
//   without(x, i)         = x with bit i switched to 0                   (one more failure)
//   sons(x)               = for every 1-bit i in x: without(x, i)
//                           -> the states reached by failing ONE working component.
//                           x has |x| sons.  (paper: Sons(x), used by Lemma 1)
//   masks_with_working(n, w)
//                         = every n-bit number that has exactly w 1-bits, in increasing order.
//                           Used to visit the states one "level" at a time.
//                           (Gosper's hack steps from one such number to the next.)
//   binomial(n, k)        = "n choose k" as an exact integer
//                           (build row by row with Pascal's rule or multiply/divide in order).
#pragma once

#include <bit>
#include <cstdint>
#include <vector>

namespace reliability {

using StateMask = std::uint32_t;  // bit i = 1  <=>  component i works

// Largest n a system may have (checked by SystemModel::validate). Decision made on Day 2:
// keep 24 for now. Validation and the brute-force reference cost about 2^n to 3^n work, and
// the simultaneous-failure model loses precision in plain double above about n = 12 (see
// docs/paper-validation-report.md section 4), so that model will apply its own tighter limit.
// A state mask is 32 bits wide, so no system can ever have more than 31 components.
inline constexpr int kMaxComponents = 24;

// --- one-liners: real code ---------------------------------------------------
constexpr int working_count(StateMask x) { return std::popcount(x); }
constexpr StateMask all_working(int n) { return (StateMask{1} << n) - 1U; }
constexpr bool is_working(StateMask x, int i) { return ((x >> i) & 1U) != 0U; }
constexpr StateMask without(StateMask x, int i) { return x & ~(StateMask{1} << i); }

// --- stubs (see state.cpp) ----------------------------------------------------
std::vector<StateMask> sons(StateMask x);
std::vector<StateMask> masks_with_working(int n, int w);
std::uint64_t binomial(int n, int k);

}  // namespace reliability
