// test_mttf.cpp - MTTF against analytical values.     (Plan: Days 6-7; REL-001, REL-002, REL-005)
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "reliability/errors.hpp"
#include "reliability/failure_model.hpp"
#include "reliability/mttf.hpp"
#include "reliability/reference.hpp"
#include "reliability/signature.hpp"
#include "reliability/state.hpp"
#include "reliability/system_model.hpp"

using namespace reliability;

namespace {

const std::string kReferenceConfig = std::string(RELIABILITY_CONFIG_DIR) + "/satellite_support.json";

SystemModel load_reference() { return SystemModel::from_json_file(kReferenceConfig); }

// The paper's Fig. 1 system: n = 3, Phi = max{x3, min{x1, x2}}; bit0 = x1, bit1 = x2, bit2 = x3.
SystemModel paper_figure_one() {
  return SystemModel::from_function(3, [](StateMask x) {
    return is_working(x, 2) || (is_working(x, 0) && is_working(x, 1));
  });
}

SystemModel bridge_network() {
  return SystemModel::from_function(5, [](StateMask x) {
    const bool c1 = is_working(x, 0), c2 = is_working(x, 1), c3 = is_working(x, 2),
               c4 = is_working(x, 3), c5 = is_working(x, 4);
    return (c1 && c4) || (c2 && c5) || (c1 && c3 && c5) || (c2 && c3 && c4);
  });
}

SystemModel random_monotone_system(int n, std::mt19937& rng) {
  const int terms = 1 + static_cast<int>(rng() % 4U);
  std::vector<StateMask> masks;
  for (int t = 0; t < terms; ++t) {
    StateMask m = 0;
    while (m == 0) {
      m = static_cast<StateMask>(rng()) & all_working(n);
    }
    masks.push_back(m);
  }
  return SystemModel::from_function(n, [masks](StateMask x) {
    return std::any_of(masks.begin(), masks.end(), [x](StateMask m) { return (x & m) == m; });
  });
}

void ExpectRelNear(double actual, double expected, double rel, const std::string& label) {
  EXPECT_NEAR(actual, expected, rel * std::max(1.0, std::fabs(expected))) << label;
}

// Independent closed form (no sweep, no Eq. (6)) for the satellite under independent exponential
// lifetimes: the system lives as long as every pair has a working member. With a = e^{-lambda t},
// a pair with both units working survives with probability 2a - a^2, a pair with one unit working
// with probability a, and a pair with none kills the system. So for a state with p2 healthy pairs
// and p1 degraded pairs the MTTF is the integral of a^p1 * (2a - a^2)^p2, i.e.
//     sum_j C(p2,j) 2^(p2-j) (-1)^j / (lambda * (p1 + p2 + j)).
double satellite_closed_form(StateMask x, double lambda) {
  int healthy = 0;
  int degraded = 0;
  for (int pair = 0; pair < 3; ++pair) {
    const int alive = working_count((x >> (2 * pair)) & 3U);
    if (alive == 0) {
      return 0.0;
    }
    (alive == 2 ? healthy : degraded) += 1;
  }
  double total = 0.0;
  for (int j = 0; j <= healthy; ++j) {
    total += static_cast<double>(binomial(healthy, j)) * std::pow(2.0, healthy - j) *
             (j % 2 == 0 ? 1.0 : -1.0) / (lambda * (degraded + healthy + j));
  }
  return total;
}

// Independent closed form (no sweep, no kernel, no Eq. (6)) for the satellite under the lethal-shock
// model with beta = theta. A state survives while no shock has struck AND every pair still has a working
// unit; units fail alone at rate r = 1 - theta. With a = e^{-r t}, p2 healthy pairs and p1 degraded pairs:
//     MTTF = integral of e^{-theta t} a^p1 (2a - a^2)^p2 dt
//          = sum_j C(p2,j) 2^(p2-j) (-1)^j / (theta + r (p1 + p2 + j)).
double satellite_lethal_shock_closed_form(StateMask x, double theta) {
  const double r = 1.0 - theta;
  int healthy = 0;
  int degraded = 0;
  for (int pair = 0; pair < 3; ++pair) {
    const int alive = working_count((x >> (2 * pair)) & 3U);
    if (alive == 0) {
      return 0.0;
    }
    (alive == 2 ? healthy : degraded) += 1;
  }
  long double total = 0.0L;
  for (int j = 0; j <= healthy; ++j) {
    total += static_cast<long double>(binomial(healthy, j)) * std::pow(2.0L, healthy - j) *
             (j % 2 == 0 ? 1.0L : -1.0L) / (theta + r * (degraded + healthy + j));
  }
  return static_cast<double>(total);
}

// parallel(n) under [LSh]: the system outlives its components or the shock, whichever comes first:
//     MTTF = sum_{j=1..n} (-1)^(j+1) C(n,j) / (theta + j (1 - theta)).
double parallel_lethal_shock_closed_form(int n, double theta) {
  long double total = 0.0L;
  for (int j = 1; j <= n; ++j) {
    total += (j % 2 == 1 ? 1.0L : -1.0L) * static_cast<long double>(binomial(n, j)) / (theta + j * (1.0 - theta));
  }
  return static_cast<double>(total);
}

}  // namespace

TEST(Mttf, FromCountingVectorHandCases) {
  const IndependentExponential fm(1.0);
  EXPECT_NEAR(mttf_from_counting_vector({1, 0, 0, 0}, fm), 1.0 / 3.0, 1e-15);   // series of 3
  EXPECT_NEAR(mttf_from_counting_vector({1, 2, 0}, fm), 1.5, 1e-15);            // 1-out-of-2
  EXPECT_NEAR(mttf_from_counting_vector({1, 6, 12, 8, 0, 0, 0}, fm), 0.7, 1e-15);
  EXPECT_NEAR(mttf_from_counting_vector({1, 3, 1, 0}, fm), 7.0 / 6.0, 1e-15);   // paper Fig. 1
  EXPECT_EQ(mttf_from_counting_vector({0, 0, 0}, fm), 0.0);                    // failed system
  EXPECT_EQ(mttf_from_counting_vector({0}, fm), 0.0);
  EXPECT_THROW(mttf_from_counting_vector({}, fm), std::invalid_argument);
  EXPECT_THROW(mttf_from_counting_vector({1, 9, 0}, fm), std::invalid_argument);
}

// TC-A-04
TEST(Mttf, SeriesSystemMttfIsOneOverNLambda) {
  for (int n = 2; n <= 6; ++n) {
    for (double lambda : {0.5, 1.0, 3.0}) {
      const std::vector<double> m = compute_all_mttf(SystemModel::series(n), IndependentExponential(lambda));
      ExpectRelNear(m[all_working(n)], 1.0 / (n * lambda), 1e-12,
                    "series n=" + std::to_string(n) + " lambda=" + std::to_string(lambda));
    }
  }
}

// TC-B-04, B-05
TEST(Mttf, OneOutOfTwoMttf) {
  for (double lambda : {0.5, 1.0, 3.0}) {
    const std::vector<double> m = compute_all_mttf(SystemModel::parallel(2), IndependentExponential(lambda));
    ASSERT_EQ(m.size(), 4U);
    ExpectRelNear(m[0b11], 1.5 / lambda, 1e-12, "both working");
    ExpectRelNear(m[0b01], 1.0 / lambda, 1e-12, "one working");
    ExpectRelNear(m[0b10], 1.0 / lambda, 1e-12, "other one working");
    EXPECT_EQ(m[0b00], 0.0);
  }
}

// TC-C-07, TC-C-08  (oracles ORC-3, ORC-4, ORC-5)
TEST(Mttf, SatelliteSystemMttfValues) {
  const SystemModel model = load_reference();
  const std::vector<double> m = compute_all_mttf(model, IndependentExponential(1.0));
  ASSERT_EQ(m.size(), 64U);
  EXPECT_NEAR(m[63], 0.7, 1e-12);
  EXPECT_NEAR(m[62], 8.0 / 15.0, 1e-12);
  EXPECT_NEAR(m[58], 5.0 / 12.0, 1e-12);
  for (StateMask x = 0; x < 64; ++x) {
    if (!model.phi(x)) {
      EXPECT_EQ(m[x], 0.0) << "state " << x;  // TC-C-08: a failed system has MTTF 0
    } else {
      EXPECT_GT(m[x], 0.0) << "state " << x;
    }
  }
}

// All 64 satellite states against the independent integral formula, for several rates.
// Traceability: TC-C-20
TEST(Mttf, SatelliteMatchesIndependentClosedFormOnEveryState) {
  const SystemModel model = load_reference();
  for (double lambda : {0.25, 1.0, 7.0}) {
    const std::vector<double> m = compute_all_mttf(model, IndependentExponential(lambda));
    for (StateMask x = 0; x < 64; ++x) {
      ExpectRelNear(m[x], satellite_closed_form(x, lambda), 1e-12,
                    "state " + std::to_string(x) + " lambda=" + std::to_string(lambda));
    }
  }
}

// TC-C-14 (paper Fig. 1: m(1) = 7/6 and the "battery" ratios)
TEST(Mttf, PaperFigureOneMttf) {
  const std::vector<double> m = compute_all_mttf(paper_figure_one(), IndependentExponential(1.0));
  EXPECT_NEAR(m[0b111], 7.0 / 6.0, 1e-12);
  EXPECT_NEAR(m[0b101] / m[0b111], 6.0 / 7.0, 1e-12);  // (1,0,1)  ~ 86 %
  EXPECT_NEAR(m[0b110] / m[0b111], 6.0 / 7.0, 1e-12);  // (0,1,1)  ~ 86 %
  EXPECT_NEAR(m[0b100] / m[0b111], 6.0 / 7.0, 1e-12);  // (0,0,1)  ~ 86 %
  EXPECT_NEAR(m[0b011] / m[0b111], 3.0 / 7.0, 1e-12);  // (1,1,0)  ~ 43 %
  EXPECT_EQ(m[0b001], 0.0);
  EXPECT_EQ(m[0b010], 0.0);
  EXPECT_EQ(m[0b000], 0.0);
  // the figure prints these percentages
  EXPECT_NEAR(100.0 * m[0b101] / m[0b111], 85.7, 0.05);
  EXPECT_NEAR(100.0 * m[0b011] / m[0b111], 42.9, 0.05);
}

// k-out-of-n:G has the closed form  MTTF = sum_{j=k}^{w} 1/(j*lambda)  in every state with w >= k
// working components (and 0 otherwise). This checks ALL states of many systems, independently.
// Traceability: TC-C-21
TEST(Mttf, KOutOfNMatchesClosedFormOnEveryState) {
  const double lambda = 1.7;
  for (int n = 2; n <= 9; ++n) {
    for (int k = 1; k <= n; ++k) {
      const SystemModel model =
          SystemModel::from_function(n, [k](StateMask x) { return working_count(x) >= k; });
      const std::vector<double> m = compute_all_mttf(model, IndependentExponential(lambda));
      for (StateMask x = 0; x <= all_working(n); ++x) {
        const int w = working_count(x);
        double expected = 0.0;
        for (int j = k; j <= w; ++j) {
          expected += 1.0 / (j * lambda);
        }
        ExpectRelNear(m[x], expected, 1e-12,
                      "n=" + std::to_string(n) + " k=" + std::to_string(k) + " x=" + std::to_string(x));
      }
    }
  }
}

// TC-C-13 (REL-005): Eq. (7) via the sweep == Eq. (6) solved directly
TEST(Mttf, SweepMatchesDirectSolveUnderExponential) {
  std::vector<SystemModel> systems;
  systems.push_back(load_reference());
  systems.push_back(paper_figure_one());
  systems.push_back(bridge_network());
  for (int n = 2; n <= 8; ++n) {
    systems.push_back(SystemModel::series(n));
    systems.push_back(SystemModel::parallel(n));
  }
  std::mt19937 rng(8675309);
  for (int t = 0; t < 40; ++t) {
    systems.push_back(random_monotone_system(3 + static_cast<int>(rng() % 6U), rng));
  }
  for (std::size_t idx = 0; idx < systems.size(); ++idx) {
    for (double lambda : {0.3, 1.0, 5.0}) {
      const IndependentExponential fm(lambda);
      const std::vector<double> sweep_m = compute_all_mttf(systems[idx], fm);
      const std::vector<double> direct_m = solve_mttf_direct(systems[idx], fm);
      ASSERT_EQ(sweep_m.size(), direct_m.size());
      for (std::size_t x = 0; x < sweep_m.size(); ++x) {
        ExpectRelNear(sweep_m[x], direct_m[x], 1e-12,
                      "system " + std::to_string(idx) + " state " + std::to_string(x));
      }
    }
  }
}

// TC-F-03, F-04, F-08
TEST(Mttf, NumericalSanityAndScalingInLambda) {
  const SystemModel model = load_reference();
  const std::vector<double> base = compute_all_mttf(model, IndependentExponential(1.0));
  for (double lambda : {1e-6, 1e-3, 0.1, 10.0, 1e3}) {
    const std::vector<double> m = compute_all_mttf(model, IndependentExponential(lambda));
    for (StateMask x = 0; x < 64; ++x) {
      EXPECT_TRUE(std::isfinite(m[x])) << "lambda=" << lambda << " state " << x;
      EXPECT_GE(m[x], 0.0);
      // MTTF scales as 1/lambda: m(lambda) * lambda is the same number for every lambda
      ExpectRelNear(m[x] * lambda, base[x], 1e-12, "lambda=" + std::to_string(lambda));
    }
  }
}

TEST(Mttf, ComputeAllMttfSizeAndSystemsOfEveryShape) {
  for (int n = 1; n <= 10; ++n) {
    const std::vector<double> m = compute_all_mttf(SystemModel::parallel(n), IndependentExponential(1.0));
    EXPECT_EQ(m.size(), std::size_t{1} << n);
    // parallel(n): MTTF of the full system is the harmonic number H_n
    double harmonic = 0.0;
    for (int j = 1; j <= n; ++j) {
      harmonic += 1.0 / j;
    }
    ExpectRelNear(m[all_working(n)], harmonic, 1e-12, "parallel n=" + std::to_string(n));
  }
}

// ---- Day 7: simultaneous failures -----------------------------------------------------------------------

// TC-D-05 (ORC-8): the closed form at beta = 0.05, 0.2, 0.5, and the exact value 32/35 at beta = 0.5
TEST(Mttf, LethalShockMatchesClosedFormAndDirectSolve) {
  const SystemModel model = load_reference();
  for (double beta : {0.05, 0.2, 0.5}) {
    const double r = 1.0 - beta;
    const double closed = 8.0 / (beta + 3 * r) - 12.0 / (beta + 4 * r) + 6.0 / (beta + 5 * r) - 1.0 / (beta + 6 * r);
    const std::unique_ptr<FailureModel> fm = make_lethal_shock(beta, 6);
    const std::vector<double> m = compute_all_mttf(model, *fm);
    ExpectRelNear(m[63], closed, 1e-12, "beta=" + std::to_string(beta));
    // TC-D-06: Eq. (7) through the sweep equals Eq. (6) solved directly, on every state
    const std::vector<double> direct = solve_mttf_direct(model, *fm);
    for (StateMask x = 0; x < 64; ++x) {
      ExpectRelNear(m[x], direct[x], 1e-12, "beta=" + std::to_string(beta) + " state " + std::to_string(x));
    }
  }
  ExpectRelNear(compute_all_mttf(model, *make_lethal_shock(0.5, 6))[63], 32.0 / 35.0, 1e-12, "32/35");
  // the other published values of the plan: 0.71782, 0.77608, 0.91429 and the P1-failed state at beta = 0.5
  ExpectRelNear(compute_all_mttf(model, *make_lethal_shock(0.05, 6))[63], 147467.0 / 205436.0, 1e-12, "beta=0.05");
  ExpectRelNear(compute_all_mttf(model, *make_lethal_shock(0.2, 6))[63], 6003.0 / 7735.0, 1e-12, "beta=0.2");
  ExpectRelNear(compute_all_mttf(model, *make_lethal_shock(0.5, 6))[62], 11.0 / 15.0, 1e-12, "P1 failed, beta=0.5");
  ExpectRelNear(compute_all_mttf(model, *make_lethal_shock(0.5, 6))[58], 3.0 / 5.0, 1e-12, "P1,C1 failed, beta=0.5");
}

// All 64 states against the independent integral formula, several beta values.
// Traceability: TC-D-13
TEST(Mttf, SatelliteLethalShockMatchesIndependentClosedFormOnEveryState) {
  const SystemModel model = load_reference();
  for (double beta : {0.01, 0.05, 0.2, 0.5, 0.9}) {
    const std::vector<double> m = compute_all_mttf(model, *make_lethal_shock(beta, 6));
    for (StateMask x = 0; x < 64; ++x) {
      ExpectRelNear(m[x], satellite_lethal_shock_closed_form(x, beta), 1e-11,
                    "state " + std::to_string(x) + " beta=" + std::to_string(beta));
    }
  }
}

// TC-D-01: beta = 0 reproduces the independent model on every state of the satellite
TEST(Mttf, ZeroBetaReproducesTheIndependentModel) {
  const SystemModel model = load_reference();
  const std::vector<double> independent = compute_all_mttf(model, IndependentExponential(1.0));
  const std::vector<double> lethal = compute_all_mttf(model, *make_lethal_shock(0.0, 6));
  const std::vector<double> bfr_limit = compute_all_mttf(model, *make_binomial_failure_rate(1e-9, 6));
  for (StateMask x = 0; x < 64; ++x) {
    ExpectRelNear(lethal[x], independent[x], 1e-12, "lethal shock, state " + std::to_string(x));
    ExpectRelNear(bfr_limit[x], independent[x], 1e-6, "binomial failure rate -> independent, state " + std::to_string(x));
  }
}

// Series and parallel systems have closed forms under [LSh] too: series fails at the first event,
// 1/Psi(n); parallel by inclusion-exclusion.
// Traceability: TC-D-13
TEST(Mttf, SeriesAndParallelLethalShockClosedForms) {
  for (double beta : {0.05, 0.2, 0.5, 0.9}) {
    for (int n = 2; n <= 12; ++n) {
      const std::unique_ptr<FailureModel> fm = make_lethal_shock(beta, n);
      ExpectRelNear(compute_all_mttf(SystemModel::series(n), *fm)[all_working(n)], 1.0 / ((1 - beta) * n + beta),
                    1e-12, "series n=" + std::to_string(n) + " beta=" + std::to_string(beta));
      ExpectRelNear(compute_all_mttf(SystemModel::parallel(n), *fm)[all_working(n)],
                    parallel_lethal_shock_closed_form(n, beta), 1e-9,
                    "parallel n=" + std::to_string(n) + " beta=" + std::to_string(beta));
    }
  }
}

// TC-D-04 (CORRECTED expectation - see docs/paper-validation-report.md E1). With every component
// calibrated to unit mean lifetime, ABSOLUTE MTTF rises with beta (failures are bunched together), while
// the benefit of redundancy relative to a series system built from the same components FALLS.
TEST(Mttf, RedundancyBenefitFallsAsBetaGrows) {
  const SystemModel model = load_reference();
  const double betas[] = {0.0, 0.05, 0.2, 0.5};
  const double expected_absolute[] = {0.7, 0.717824529, 0.776082741, 0.914285714};
  const double expected_ratio[] = {4.2, 4.1275, 3.8804, 3.2};
  double previous_ratio = 1e9;
  double previous_absolute = 0.0;
  for (int i = 0; i < 4; ++i) {
    const std::unique_ptr<FailureModel> fm = make_lethal_shock(betas[i], 6);
    const double absolute = compute_all_mttf(model, *fm)[63];
    const double series_of_six = 1.0 / fm->exit_rate(6);  // the six components in series: fails at the first event
    const double ratio = absolute / series_of_six;
    EXPECT_NEAR(absolute, expected_absolute[i], 1e-8) << "beta=" << betas[i];
    EXPECT_NEAR(ratio, expected_ratio[i], 1e-3) << "beta=" << betas[i];
    EXPECT_LT(ratio, previous_ratio) << "the redundancy ratio must fall as beta grows";
    EXPECT_GT(absolute, previous_absolute) << "absolute MTTF rises with beta under this calibration";
    previous_ratio = ratio;
    previous_absolute = absolute;
  }
}

// beta = 1: every event is a lethal shock, so every working system lives exactly one unit of time on average.
TEST(Mttf, FullyCommonCauseGivesMttfOneForEveryWorkingState) {
  const SystemModel model = load_reference();
  const std::vector<double> m = compute_all_mttf(model, *make_lethal_shock(1.0, 6));
  for (StateMask x = 0; x < 64; ++x) {
    EXPECT_NEAR(m[x], model.phi(x) ? 1.0 : 0.0, 1e-12) << "state " << x;
  }
}

// Eq. (7) = Eq. (6) under every simultaneous-failure family, on many systems.
// Traceability: TC-D-14
TEST(Mttf, SweepMatchesDirectSolveUnderSimultaneousFailures) {
  std::vector<SystemModel> systems;
  systems.push_back(load_reference());
  systems.push_back(paper_figure_one());
  systems.push_back(bridge_network());
  std::mt19937 rng(2718281);
  for (int t = 0; t < 25; ++t) {
    systems.push_back(random_monotone_system(3 + static_cast<int>(rng() % 6U), rng));
  }
  std::vector<std::unique_ptr<FailureModel>> models;
  models.push_back(make_lethal_shock(0.2, 8));
  models.push_back(make_lethal_shock(0.7, 8));
  models.push_back(make_binomial_failure_rate(0.15, 8));
  models.push_back(make_binomial_failure_rate(0.6, 8));
  models.push_back(std::make_unique<LevyFrailty>("stable(0.6)", [](int x) { return std::pow(static_cast<double>(x), 0.6); }, 8));
  for (std::size_t idx = 0; idx < systems.size(); ++idx) {
    for (const auto& fm : models) {
      const std::vector<double> sweep_m = compute_all_mttf(systems[idx], *fm);
      const std::vector<double> direct_m = solve_mttf_direct(systems[idx], *fm);
      for (std::size_t x = 0; x < sweep_m.size(); ++x) {
        ExpectRelNear(sweep_m[x], direct_m[x], 1e-10,
                      fm->name() + ", system " + std::to_string(idx) + ", state " + std::to_string(x));
      }
    }
  }
}

// TC-F-07: accuracy at larger n, against exact rational arithmetic (Python Fractions), for the parallel
// system under [LSh] with beta = 1/5. The error grows by roughly a factor 10 per two components; the model
// refuses to exist beyond n = 16.
TEST(Mttf, LethalShockAccuracyAtLargerSizes) {
  struct Case {
    int n;
    double exact;
    double tolerance;
  };
  const Case cases[] = {
      {6, 2.1757810816634344, 1e-13},
      {12, 2.595877122891562, 1e-10},
      {16, 2.7556771083577862, 1e-9},
  };
  for (const Case& c : cases) {
    const std::unique_ptr<FailureModel> fm = make_lethal_shock(0.2, c.n);
    CountingVector w(static_cast<std::size_t>(c.n) + 1, 0);
    for (int k = 0; k < c.n; ++k) {
      w[static_cast<std::size_t>(k)] = binomial(c.n, k);
    }
    const double m = mttf_from_counting_vector(w, *fm);  // parallel(n): only the all-failed case fails
    EXPECT_NEAR(m, c.exact, c.tolerance * c.exact) << "n = " << c.n;
  }
  EXPECT_THROW(make_lethal_shock(0.2, 17), ModelError);
}

// REL-002 acceptance: the model genuinely produces simultaneous failures, and ignoring them would be wrong.
// Treating the shock-driven failures as if they came one at a time (an independent model with the same
// per-component rate) predicts a different MTTF than the lethal-shock model with the same marginals.
TEST(Mttf, SimultaneousFailuresChangeTheAnswerComparedWithIndependentFailures) {
  const SystemModel model = load_reference();
  const double independent = compute_all_mttf(model, IndependentExponential(1.0))[63];
  const double shocked = compute_all_mttf(model, *make_lethal_shock(0.5, 6))[63];
  EXPECT_GT(std::fabs(shocked - independent), 0.1);
}

// Traceability: TC-D-19
// The paper calibrates all six models to agree on every PAIR of components (Section VII-B). So a single redundant pair has
// exactly the same MTTF under all of them, while the satellite, which has several pairs, does not.
TEST(Mttf, AllSixModelsAgreeOnOnePairButNotOnTheSatellite) {
  const SystemModel pair = SystemModel::parallel(2);
  const SystemModel model = load_reference();
  const std::vector<std::pair<std::string, std::function<std::unique_ptr<FailureModel>(double, int)>>> makers = {
      {"lethal shock", make_lethal_shock}, {"binomial failure rate", make_binomial_failure_rate},
      {"compound Poisson", make_compound_poisson}, {"gamma process", make_gamma_process},
      {"inverse Gaussian", make_inverse_gaussian}, {"stable subordinator", make_stable_subordinator}};
  for (double beta : {0.05, 0.2, 0.5}) {
    const double reference_pair = compute_all_mttf(pair, *makers[0].second(beta, 2))[3];
    double lowest = 1e9, highest = 0.0;
    for (const auto& [name, make] : makers) {
      const double pair_mttf = compute_all_mttf(pair, *make(beta, 2))[3];
      ExpectRelNear(pair_mttf, reference_pair, 1e-10, name + " one pair, beta=" + std::to_string(beta));
      const double satellite = compute_all_mttf(model, *make(beta, 6))[63];
      lowest = std::min(lowest, satellite);
      highest = std::max(highest, satellite);
    }
    EXPECT_GT(highest - lowest, 1e-3) << "the models must differ on a system with several pairs, beta=" << beta;
  }
}

// Traceability: TC-D-20
// Eq. (7) (through the sweep) equals Eq. (6) (solved directly) under every one of the six models, on many systems.
TEST(Mttf, SweepMatchesDirectSolveUnderAllSixCommonCauseModels) {
  std::vector<SystemModel> systems;
  systems.push_back(load_reference());
  systems.push_back(paper_figure_one());
  systems.push_back(bridge_network());
  std::mt19937 rng(161803);
  for (int t = 0; t < 12; ++t) {
    systems.push_back(random_monotone_system(3 + static_cast<int>(rng() % 6U), rng));
  }
  std::vector<std::unique_ptr<FailureModel>> models;
  for (double beta : {0.1, 0.45}) {
    models.push_back(make_lethal_shock(beta, 8));
    models.push_back(make_binomial_failure_rate(beta, 8));
    models.push_back(make_compound_poisson(beta, 8));
    models.push_back(make_gamma_process(beta, 8));
    models.push_back(make_inverse_gaussian(beta, 8));
    models.push_back(make_stable_subordinator(beta, 8));
  }
  for (std::size_t idx = 0; idx < systems.size(); ++idx) {
    for (const auto& fm : models) {
      const std::vector<double> sweep_m = compute_all_mttf(systems[idx], *fm);
      const std::vector<double> direct_m = solve_mttf_direct(systems[idx], *fm);
      for (std::size_t x = 0; x < sweep_m.size(); ++x) {
        ExpectRelNear(sweep_m[x], direct_m[x], 1e-10,
                      fm->name() + ", system " + std::to_string(idx) + ", state " + std::to_string(x));
      }
    }
  }
}
