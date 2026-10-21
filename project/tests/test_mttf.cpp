// test_mttf.cpp - MTTF against analytical values.     (Plan: Days 6-7; REL-001, REL-002, REL-005)
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
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

// Day 7 (needs the Levy-frailty model)
TEST(Mttf, LethalShockMatchesClosedFormAndDirectSolve) {
  GTEST_SKIP() << "Day 7: needs LevyFrailty";
  // PSEUDO-CODE
  //   for beta in {0.05, 0.2, 0.5}:   r = 1 - beta
  //       closed = 8/(beta+3r) - 12/(beta+4r) + 6/(beta+5r) - 1/(beta+6r)
  //       expect compute_all_mttf(reference, lethal_shock(beta))[63] == closed
  //       expect sweep == solve_mttf_direct for all 64 states
  //   expect beta = 0.5 gives 32/35
}

// TC-D-04 (CORRECTED expectation - see docs/paper-validation-report.md E1)
TEST(Mttf, RedundancyBenefitFallsAsBetaGrows) {
  GTEST_SKIP() << "Day 7: needs LevyFrailty";
  // PSEUDO-CODE
  //   for beta in {0, 0.05, 0.2, 0.5}:
  //       ratio = m_reference[63] / (1 / Psi(6))          // series-of-6 MTTF under the same model
  //   expect ratios ~ 4.20, 4.13, 3.88, 3.20 and strictly decreasing
  //   do NOT assert that absolute MTTF decreases: it INCREASES (0.700 -> 0.914)
}
