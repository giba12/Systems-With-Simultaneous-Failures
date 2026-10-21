// test_failure_models.cpp - [Exp] and Levy-frailty models.   (Plan: Days 6-7; REL-003, REL-004)
#include <gtest/gtest.h>

#include "reliability/errors.hpp"
#include "reliability/failure_model.hpp"

using namespace reliability;

// REL-003
TEST(FailureModels, ExponentialIncrementsAreOneOverRemainingRate) {
  GTEST_SKIP() << "needs IndependentExponential";
  // PSEUDO-CODE
  //   fm = exponential(lambda = 2)
  //   expect fm.increment(w, k) == 1 / (2 * (w - k)) for all 0 <= k < w <= 8
  //   expect fm.exit_rate(w) == 2 * w;  transition_prob(w, w-1) == 1;  transition_prob(w, w-2) == 0
}

// TC-E-07
TEST(FailureModels, InvalidExponentialRateIsRejected) {
  GTEST_SKIP() << "needs IndependentExponential";
  // PSEUDO-CODE
  //   expect ModelError for lambda in {0, -1, NaN, +infinity}
}

// TC-D-08 (REL-004)
TEST(FailureModels, LethalShockCalibration) {
  GTEST_SKIP() << "needs LevyFrailty";
  // PSEUDO-CODE
  //   fm = lethal_shock(beta = 0.2)
  //   expect exit_rate(1) == 1                         (Psi(1) = 1: unit mean lifetime)
  //   expect 2 - exit_rate(2) == 0.2                   (beta = 2 - Psi(2))
  //   expect lethal_shock(0).increment(w,k) == exponential(1).increment(w,k)   (beta 0 = [Exp])
}

// TC-E-08
TEST(FailureModels, InvalidBetaIsRejected) {
  GTEST_SKIP() << "needs make_lethal_shock";
  // PSEUDO-CODE
  //   expect ModelError for beta = -0.1 and beta = 1.1
}

// TC-D-07
TEST(FailureModels, TransitionRowsAreProbabilities) {
  GTEST_SKIP() << "needs LevyFrailty";
  // PSEUDO-CODE
  //   for beta in {0.05, 0.2, 0.5}, w in 1..6:
  //       expect every transition_prob(w, w2) >= -1e-12 for w2 < w
  //       expect sum over w2 < w of transition_prob(w, w2) == 1   (tolerance 1e-12)
  //       expect mean failures per event, sum (w-w2)*P, == w * Psi(1) / Psi(w)       (paper Remark 1)
}

// TC-D-02 (a single event can fail several components)
TEST(FailureModels, LethalShockAllowsSimultaneousFailures) {
  GTEST_SKIP() << "needs LevyFrailty";
  // PSEUDO-CODE
  //   fm = lethal_shock(0.5)
  //   expect transition_prob(6, 0) > 0                 (a shock can kill all six at once)
  //   expect transition_prob(6, 5) > 0                 (a single component can still fail alone)
  //   expect transition_prob(6, 3) == 0                 ([LSh] only fails ONE component or ALL of them)
}

// TC-D-06 / Eq. (21) cross-check, TC-F-07 precision
TEST(FailureModels, TwoWaysToGetIncrementsAgree) {
  GTEST_SKIP() << "Day 7: needs LevyFrailty (Eq. (21) uses lower limit w-k, NOT the printed w-k+1)";
  // PSEUDO-CODE
  //   for beta in {0.05, 0.2, 0.5}, n in {6, 12}:
  //       expect relative difference between increment(w,k) and increment_eq21(w,k) < 1e-9
  //   expect every increment(w,k) > 0
  //   note: at n = 20 plain double is expected to drift ~1e-8 (see validation report)
}
