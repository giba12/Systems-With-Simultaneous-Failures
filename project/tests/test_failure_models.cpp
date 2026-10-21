// test_failure_models.cpp - [Exp] and Levy-frailty models.   (Plan: Days 6-7; REL-003, REL-004)
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include "reliability/errors.hpp"
#include "reliability/failure_model.hpp"
#include "reliability/state.hpp"

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

namespace {

// What the paper PRINTS for Eq. (21): lower limit i = w-k+1. It is a typo (see the validation report,
// section 6); this helper exists only so a test can document how wrong it is.
double printed_eq21(const LevyFrailty& model, int w, int k) {
  long double total = 0.0L;
  for (int i = w - k + 1; i <= w; ++i) {
    const long double sign = ((i - w + k) % 2 == 0) ? 1.0L : -1.0L;
    total += sign * static_cast<long double>(binomial(w, k)) * static_cast<long double>(binomial(k, w - i)) /
             static_cast<long double>(model.exit_rate(i));
  }
  return static_cast<double>(total);
}

}  // namespace

// TC-D-08 (REL-004)
TEST(FailureModels, LethalShockCalibration) {
  const std::unique_ptr<FailureModel> fm = make_lethal_shock(0.2, 8);
  EXPECT_NEAR(fm->exit_rate(1), 1.0, 1e-15);        // Psi(1) = 1: unit mean lifetime
  EXPECT_NEAR(2.0 - fm->exit_rate(2), 0.2, 1e-15);  // beta = 2 - Psi(2)  (Eq. (22))
  EXPECT_NEAR(fm->exit_rate(5), 0.8 * 5 + 0.2, 1e-15);
  EXPECT_EQ(fm->exit_rate(0), 0.0);

  // beta = 0 is exactly the independent exponential model with lambda = 1 (TC-D-01)
  const std::unique_ptr<FailureModel> independent = make_lethal_shock(0.0, 8);
  const IndependentExponential exponential(1.0);
  for (int w = 1; w <= 8; ++w) {
    EXPECT_NEAR(independent->exit_rate(w), exponential.exit_rate(w), 1e-15);
    for (int k = 0; k < w; ++k) {
      EXPECT_NEAR(independent->increment(w, k), exponential.increment(w, k), 1e-13) << w << "," << k;
    }
    for (int w2 = 0; w2 < w; ++w2) {
      EXPECT_NEAR(independent->transition_prob(w, w2), exponential.transition_prob(w, w2), 1e-13);
    }
  }
}

TEST(FailureModels, BinomialFailureRateCalibration) {
  for (double beta : {0.05, 0.2, 0.5, 1.0}) {
    const std::unique_ptr<FailureModel> fm = make_binomial_failure_rate(beta, 8);
    EXPECT_NEAR(fm->exit_rate(1), 1.0, 1e-14) << beta;
    EXPECT_NEAR(2.0 - fm->exit_rate(2), beta, 1e-14) << beta;  // beta = 2 - Psi(2)
  }
}

// TC-E-08
TEST(FailureModels, InvalidParametersAreRejected) {
  const double bad_beta[] = {-0.1, 1.1, std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity()};
  for (double beta : bad_beta) {
    EXPECT_THROW(make_lethal_shock(beta, 6), ModelError) << beta;
    EXPECT_THROW(make_binomial_failure_rate(beta, 6), ModelError) << beta;
  }
  EXPECT_THROW(make_binomial_failure_rate(0.0, 6), ModelError);  // needs beta > 0
  EXPECT_NO_THROW(make_lethal_shock(0.0, 6));
  EXPECT_NO_THROW(make_lethal_shock(1.0, 6));
  EXPECT_NO_THROW(make_binomial_failure_rate(1.0, 6));
  // sizes
  EXPECT_THROW(make_lethal_shock(0.2, 0), ModelError);
  EXPECT_THROW(make_lethal_shock(0.2, kMaxLevyFrailtyComponents + 1), ModelError);
  EXPECT_NO_THROW(make_lethal_shock(0.2, kMaxLevyFrailtyComponents));
}

TEST(FailureModels, InvalidLaplaceExponentsAreRejected) {
  EXPECT_THROW(LevyFrailty("empty", nullptr, 4), ModelError);
  EXPECT_THROW(LevyFrailty("not calibrated", [](int x) { return 2.0 * x; }, 4), ModelError);       // Psi(1) = 2
  EXPECT_THROW(LevyFrailty("psi(0) != 0", [](int x) { return x + 1.0; }, 4), ModelError);
  EXPECT_THROW(LevyFrailty("decreasing", [](int x) { return x <= 1 ? 1.0 : 0.5; }, 4), ModelError);
  EXPECT_THROW(LevyFrailty("zero", [](int x) { return x == 1 ? 1.0 : 0.0; }, 4), ModelError);
  EXPECT_THROW(LevyFrailty("nan", [](int x) { return x == 3 ? std::nan("") : static_cast<double>(x); }, 4),
               ModelError);
  // Psi(x) = x^2 is calibrated and increasing but is not a Laplace exponent (not concave): its
  // "transition probabilities" would be negative, and the constructor must notice.
  EXPECT_THROW(LevyFrailty("convex", [](int x) { return static_cast<double>(x) * x; }, 6), ModelError);
}

TEST(FailureModels, ArgumentChecksOnQueries) {
  const std::unique_ptr<FailureModel> fm = make_lethal_shock(0.2, 6);
  EXPECT_THROW(fm->exit_rate(-1), ModelError);
  EXPECT_THROW(fm->exit_rate(7), ModelError);
  EXPECT_THROW(fm->transition_prob(7, 0), ModelError);
  EXPECT_THROW(fm->transition_prob(3, -1), ModelError);
  EXPECT_THROW(fm->increment(0, 0), ModelError);
  EXPECT_THROW(fm->increment(3, 3), ModelError);
  EXPECT_THROW(fm->increment(7, 0), ModelError);
  EXPECT_EQ(fm->transition_prob(0, 0), 0.0);
  EXPECT_EQ(fm->transition_prob(3, 3), 0.0);  // "and zero otherwise"
  EXPECT_EQ(fm->transition_prob(3, 5), 0.0);
}

// TC-D-07: each row of P is a probability distribution, and the mean number of components failing per
// event is w Psi(1) / Psi(w) (paper Remark 1).
TEST(FailureModels, TransitionRowsAreProbabilities) {
  for (double beta : {0.05, 0.2, 0.5}) {
    for (int family = 0; family < 2; ++family) {
      const std::unique_ptr<FailureModel> fm =
          family == 0 ? make_lethal_shock(beta, 16) : make_binomial_failure_rate(beta, 16);
      for (int w = 1; w <= 16; ++w) {
        const double tolerance = w <= 12 ? 1e-11 : 1e-9;
        double sum = 0.0;
        double mean_failures = 0.0;
        for (int w2 = 0; w2 < w; ++w2) {
          const double p = fm->transition_prob(w, w2);
          EXPECT_GE(p, 0.0) << beta << " w=" << w << " w2=" << w2;
          sum += p;
          mean_failures += (w - w2) * p;
        }
        EXPECT_NEAR(sum, 1.0, tolerance) << "family " << family << " beta " << beta << " w=" << w;
        EXPECT_NEAR(mean_failures, w * fm->exit_rate(1) / fm->exit_rate(w), 100 * tolerance)
            << "family " << family << " beta " << beta << " w=" << w;
      }
    }
  }
}

// Independent closed form for [LSh]: a component fails alone (probability (1-beta)w / Psi(w)) or a lethal
// shock fails all of them (probability beta / Psi(w)); nothing in between. This checks Eq. (20) against
// numbers that were never produced by an alternating sum.
TEST(FailureModels, LethalShockKernelMatchesClosedForm) {
  for (double beta : {0.05, 0.2, 0.5, 0.9}) {
    const std::unique_ptr<FailureModel> fm = make_lethal_shock(beta, 16);
    for (int w = 1; w <= 16; ++w) {
      const double nu = (1.0 - beta) * w + beta;
      const double tolerance = w <= 12 ? 1e-11 : 1e-9;
      for (int w2 = 0; w2 < w; ++w2) {
        double expected = 0.0;
        if (w2 == w - 1) {
          expected += (1.0 - beta) * w / nu;
        }
        if (w2 == 0) {
          expected += beta / nu;
        }
        EXPECT_NEAR(fm->transition_prob(w, w2), expected, tolerance) << "beta=" << beta << " w=" << w << " w2=" << w2;
      }
    }
  }
}

// TC-D-02: a single event can fail several components
TEST(FailureModels, LethalShockAllowsSimultaneousFailures) {
  const std::unique_ptr<FailureModel> fm = make_lethal_shock(0.5, 6);
  EXPECT_GT(fm->transition_prob(6, 0), 0.0);          // a shock can kill all six at once
  EXPECT_GT(fm->transition_prob(6, 5), 0.0);          // a single component can still fail alone
  EXPECT_NEAR(fm->transition_prob(6, 3), 0.0, 1e-12);  // [LSh] only fails ONE component or ALL of them
  EXPECT_NEAR(fm->transition_prob(6, 0), 0.5 / 3.5, 1e-12);
}

// Independent closed form for [BFR]: an event fails each of the w working components independently with
// probability p = beta, and events that fail nobody do not change the state, so the number of components
// that fail is Binomial(w, p) conditioned on being at least 1. This tests the whole kernel of Eq. (20).
TEST(FailureModels, BinomialFailureRateKernelMatchesClosedForm) {
  for (double beta : {0.05, 0.2, 0.5, 0.9}) {
    const std::unique_ptr<FailureModel> fm = make_binomial_failure_rate(beta, 16);
    for (int w = 1; w <= 16; ++w) {
      const double tolerance = w <= 12 ? 1e-11 : 1e-9;
      const double at_least_one = 1.0 - std::pow(1.0 - beta, w);
      for (int failing = 1; failing <= w; ++failing) {
        const double expected = static_cast<double>(binomial(w, failing)) * std::pow(beta, failing) *
                                std::pow(1.0 - beta, w - failing) / at_least_one;
        EXPECT_NEAR(fm->transition_prob(w, w - failing), expected, tolerance)
            << "beta=" << beta << " w=" << w << " failing=" << failing;
      }
    }
  }
}

// TC-D-06 / Eq. (21) cross-check, TC-F-07 precision.
// The two routes agree to about 1e-13 (w <= 12) in ABSOLUTE terms. For the tiny late-stage increments
// (large k, large beta) that is a larger relative error, so the tolerance is taken relative to the scale
// of the increments of that w, namely E T_{w:w} = sum_k delta(w,k). Checked against exact rational
// arithmetic: both routes are within about 5e-10 relative of the exact value even for the tiniest ones.
TEST(FailureModels, TwoWaysToGetIncrementsAgree) {
  for (double beta : {0.05, 0.2, 0.5, 0.9}) {
    const LevyFrailty model("lethal", [beta](int x) { return x == 0 ? 0.0 : (1 - beta) * x + beta; }, 16);
    for (int w = 1; w <= 16; ++w) {
      const double tolerance = w <= 12 ? 1e-11 : 1e-9;
      double scale = 0.0;
      for (int k = 0; k < w; ++k) {
        scale += model.increment(w, k);
      }
      for (int k = 0; k < w; ++k) {
        const double recursion = model.increment(w, k);
        const double eq21 = model.increment_eq21(w, k);
        EXPECT_GT(recursion, 0.0) << "beta=" << beta << " w=" << w << " k=" << k;
        EXPECT_NEAR(eq21, recursion, tolerance * scale) << "beta=" << beta << " w=" << w << " k=" << k;
      }
    }
  }
}

// Documents the typo in the paper's printed Eq. (21): with lower limit w-k+1 the sum is empty for k = 0
// (so it returns 0, although E T_{1:w} = 1/Psi(w) > 0) and it is wrong for larger k as well.
TEST(FailureModels, PrintedEquation21LowerLimitIsATypo) {
  const LevyFrailty model("lethal", [](int x) { return x == 0 ? 0.0 : 0.8 * x + 0.2; }, 8);
  for (int w = 2; w <= 8; ++w) {
    EXPECT_EQ(printed_eq21(model, w, 0), 0.0);
    EXPECT_GT(model.increment(w, 0), 0.0);
    EXPECT_NEAR(model.increment_eq21(w, 0), 1.0 / model.exit_rate(w), 1e-15);  // the corrected form
    for (int k = 1; k < w; ++k) {
      EXPECT_GT(std::fabs(printed_eq21(model, w, k) - model.increment(w, k)), 1e-3) << w << "," << k;
    }
  }
}

// A third family, beyond [LSh] and [BFR]: the stable subordinator Psi(x) = x^alpha, alpha in (0,1)
// (model [LFMO-S] of the paper). It is a valid Laplace exponent, so the kernel must be a distribution.
TEST(FailureModels, StableSubordinatorGivesAValidKernel) {
  for (double alpha : {0.3, 0.5, 0.7, 0.95}) {
    const LevyFrailty model("stable", [alpha](int x) { return std::pow(static_cast<double>(x), alpha); }, 12);
    for (int w = 1; w <= 12; ++w) {
      double sum = 0.0;
      for (int w2 = 0; w2 < w; ++w2) {
        EXPECT_GE(model.transition_prob(w, w2), 0.0);
        sum += model.transition_prob(w, w2);
      }
      EXPECT_NEAR(sum, 1.0, 1e-11) << "alpha=" << alpha << " w=" << w;
    }
    EXPECT_NEAR(2.0 - model.exit_rate(2), 2.0 - std::pow(2.0, alpha), 1e-15);  // beta = 2 - 2^alpha
  }
}

// beta = 1: every event is a lethal shock, so all components fail at the same instant.
TEST(FailureModels, FullyCommonCauseFailsEverythingAtOnce) {
  const std::unique_ptr<FailureModel> fm = make_lethal_shock(1.0, 8);
  for (int w = 1; w <= 8; ++w) {
    EXPECT_EQ(fm->exit_rate(w), 1.0);
    EXPECT_NEAR(fm->transition_prob(w, 0), 1.0, 1e-12);
    EXPECT_NEAR(fm->increment(w, 0), 1.0, 1e-12);     // one event, rate 1
    for (int k = 1; k < w; ++k) {
      EXPECT_NEAR(fm->increment(w, k), 0.0, 1e-12);  // no time passes between "failures": they coincide
    }
  }
}
