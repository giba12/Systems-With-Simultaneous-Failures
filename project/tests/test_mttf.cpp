// test_mttf.cpp - MTTF against analytical values.     (Plan: Days 6-7; REL-001, REL-002, REL-005)
#include <gtest/gtest.h>

#include "reliability/failure_model.hpp"
#include "reliability/mttf.hpp"
#include "reliability/reference.hpp"
#include "reliability/system_model.hpp"

using namespace reliability;

// TC-A-04
TEST(Mttf, SeriesSystemMttfIsOneOverNLambda) {
  GTEST_SKIP() << "needs compute_all_mttf";
  // PSEUDO-CODE
  //   for n in 2..6 and lambda in {0.5, 1, 3}:
  //       expect compute_all_mttf(series(n), exp(lambda))[all ones] == 1 / (n * lambda)
}

// TC-B-04, B-05
TEST(Mttf, OneOutOfTwoMttf) {
  GTEST_SKIP() << "needs compute_all_mttf";
  // PSEUDO-CODE
  //   m = compute_all_mttf(parallel(2), exp(1))
  //   expect m[0b11] == 1.5;  m[0b01] == 1.0;  m[0b10] == 1.0;  m[0b00] == 0
}

// TC-C-07, TC-C-08  (oracles ORC-3, ORC-4, ORC-5)
TEST(Mttf, SatelliteSystemMttfValues) {
  GTEST_SKIP() << "needs compute_all_mttf";
  // PSEUDO-CODE
  //   m = compute_all_mttf(reference system, exp(1))
  //   expect m[63] ~ 0.7;  m[62] ~ 8/15;  m[58] ~ 5/12                 (tolerance 1e-9)
  //   expect m[x] == 0 for every x where Phi(x) == 0                  (e.g. 0b111100)
}

// TC-C-14 (paper Fig. 1: m(1) = 7/6 and the "battery" ratios)
TEST(Mttf, PaperFigureOneMttf) {
  GTEST_SKIP() << "needs compute_all_mttf";
  // PSEUDO-CODE
  //   m = compute_all_mttf(fig1 system, exp(1))
  //   expect m[7] == 7/6
  //   expect m[0b101]/m[7] == 6/7;  m[0b011]/m[7] == 6/7;  m[0b100]/m[7] == 6/7
  //   expect m[0b110]/m[7] == 3/7;  m[0b001] == m[0b010] == m[0] == 0
}

// TC-C-13 (REL-005): Eq. (7) via the sweep == Eq. (6) solved directly
TEST(Mttf, SweepMatchesDirectSolveUnderExponential) {
  GTEST_SKIP() << "needs compute_all_mttf and solve_mttf_direct";
  // PSEUDO-CODE
  //   for x in 0..63: expect |m_sweep[x] - m_direct[x]| < 1e-9 (relative)
}

// TC-D-05, TC-D-06, ORC-8
TEST(Mttf, LethalShockMatchesClosedFormAndDirectSolve) {
  GTEST_SKIP() << "needs LevyFrailty";
  // PSEUDO-CODE
  //   for beta in {0.05, 0.2, 0.5}:   r = 1 - beta
  //       closed = 8/(beta+3r) - 12/(beta+4r) + 6/(beta+5r) - 1/(beta+6r)
  //       expect compute_all_mttf(reference, lethal_shock(beta))[63] == closed
  //       expect sweep == solve_mttf_direct for all 64 states
  //   expect beta = 0.5 gives 32/35
}

// TC-D-04 (CORRECTED expectation - see docs/paper-validation-report.md E1)
TEST(Mttf, RedundancyBenefitFallsAsBetaGrows) {
  GTEST_SKIP() << "needs LevyFrailty";
  // PSEUDO-CODE
  //   for beta in {0, 0.05, 0.2, 0.5}:
  //       ratio = m_reference[63] / (1 / Psi(6))          // series-of-6 MTTF under the same model
  //   expect ratios ~ 4.20, 4.13, 3.88, 3.20 and strictly decreasing
  //   do NOT assert that absolute MTTF decreases: it INCREASES (0.700 -> 0.914)
}

// TC-F-03, F-04, F-08
TEST(Mttf, NumericalSanity) {
  GTEST_SKIP() << "needs compute_all_mttf";
  // PSEUDO-CODE
  //   every m[x] finite and >= 0
  //   scaling: m(lambda) * lambda is the same for lambda in 1e-6 .. 1e3 (relative 1e-9)
}
