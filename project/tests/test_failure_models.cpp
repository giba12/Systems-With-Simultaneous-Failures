// test_failure_models.cpp - [Exp] and Levy-frailty models.   (Plan: Days 6-7; REL-003, REL-004)
#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <string>

#include "reliability/errors.hpp"
#include "reliability/failure_model.hpp"

using namespace reliability;

// REL-003
TEST(FailureModels, ExponentialIncrementsAreOneOverRemainingRate) {
  const IndependentExponential fm(2.0);
  for (int w = 1; w <= 8; ++w) {
    for (int k = 0; k < w; ++k) {
      EXPECT_DOUBLE_EQ(fm.increment(w, k), 1.0 / (2.0 * (w - k))) << "w=" << w << " k=" << k;
    }
    EXPECT_DOUBLE_EQ(fm.exit_rate(w), 2.0 * w);
    EXPECT_EQ(fm.transition_prob(w, w - 1), 1.0);
    for (int w2 = 0; w2 < w - 1; ++w2) {
      EXPECT_EQ(fm.transition_prob(w, w2), 0.0);  // never two failures at once
    }
    EXPECT_EQ(fm.transition_prob(w, w), 0.0);      // the count never stays or rises
  }
  EXPECT_EQ(fm.exit_rate(0), 0.0);
  EXPECT_EQ(fm.transition_prob(0, 0), 0.0);
}

// The increments of an exponential model add up to the harmonic expectation of the maximum:
// E T_{w:w} = (1/lambda) * (1 + 1/2 + ... + 1/w).
TEST(FailureModels, ExponentialIncrementsSumToHarmonicNumbers) {
  const double lambda = 0.5;
  const IndependentExponential fm(lambda);
  for (int w = 1; w <= 12; ++w) {
    double sum = 0.0;
    double harmonic = 0.0;
    for (int k = 0; k < w; ++k) {
      sum += fm.increment(w, k);
    }
    for (int j = 1; j <= w; ++j) {
      harmonic += 1.0 / j;
    }
    EXPECT_NEAR(sum, harmonic / lambda, 1e-12) << "w=" << w;
  }
}

TEST(FailureModels, ExponentialRejectsBadArguments) {
  const IndependentExponential fm(1.0);
  EXPECT_THROW(fm.increment(3, 3), ModelError);   // k must be < w
  EXPECT_THROW(fm.increment(3, -1), ModelError);
  EXPECT_THROW(fm.increment(0, 0), ModelError);
  EXPECT_THROW(fm.exit_rate(-1), ModelError);
  EXPECT_THROW(fm.transition_prob(-1, 0), ModelError);
}

// TC-E-07
TEST(FailureModels, InvalidExponentialRateIsRejected) {
  const double bad[] = {0.0, -1.0, -1e-300, std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::infinity(),
                        -std::numeric_limits<double>::infinity()};
  for (double lambda : bad) {
    EXPECT_THROW(IndependentExponential{lambda}, ModelError) << "lambda = " << lambda;
    EXPECT_THROW(make_exponential(lambda), ModelError) << "lambda = " << lambda;
  }
  EXPECT_NO_THROW(IndependentExponential{1e-9});
  EXPECT_NO_THROW(IndependentExponential{1e9});
}

TEST(FailureModels, ExponentialFactoryAndName) {
  const std::unique_ptr<FailureModel> fm = make_exponential(2.5);
  ASSERT_NE(fm, nullptr);
  EXPECT_NE(fm->name().find("independent_exponential"), std::string::npos);
  EXPECT_NE(fm->name().find("2.5"), std::string::npos);
  EXPECT_DOUBLE_EQ(fm->increment(4, 1), 1.0 / (2.5 * 3));
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
