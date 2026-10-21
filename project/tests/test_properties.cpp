// test_properties.cpp - things the paper PROVES, checked on our code.   (Plan: Day 6; VER-004)
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <set>
#include <string>
#include <vector>

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

// A fixed collection of monotone systems used by the property tests.
std::vector<SystemModel> test_systems() {
  std::vector<SystemModel> systems;
  systems.push_back(load_reference());
  systems.push_back(paper_figure_one());
  systems.push_back(bridge_network());
  std::mt19937 rng(1234567);
  for (int t = 0; t < 40; ++t) {
    systems.push_back(random_monotone_system(3 + static_cast<int>(rng() % 6U), rng));
  }
  return systems;
}

// x "signature-dominates" y (same number of working components): S_x[k] >= S_y[k] for all k.
// Both vectors have the same length, and C(w,k) is the same for both, so comparing the integer
// counting vectors W entry by entry is the same test, and exact.
bool dominates(const CountingVector& x, const CountingVector& y) {
  for (std::size_t k = 0; k < x.size(); ++k) {
    if (x[k] < y[k]) {
      return false;
    }
  }
  return true;
}

}  // namespace

// TC-I-02, paper Lemma 4
TEST(Properties, MttfNeverIncreasesWhenMoreComponentsFail) {
  const IndependentExponential fm(1.0);
  std::size_t pairs = 0;
  for (const SystemModel& model : test_systems()) {
    const std::vector<double> m = compute_all_mttf(model, fm);
    const std::uint64_t states = std::uint64_t{1} << model.n();
    for (std::uint64_t xi = 1; xi < states; ++xi) {
      const StateMask x = static_cast<StateMask>(xi);
      for (StateMask y = (x - 1U) & x;; y = (y - 1U) & x) {  // every proper sub-state y of x
        EXPECT_LE(m[y], m[x] + 1e-12) << "y=" << y << " x=" << x;
        ++pairs;
        if (y == 0U) {
          break;
        }
      }
    }
  }
  EXPECT_GT(pairs, 10000U);
}

// TC-I-01, paper Theorem 5: if x dominates y (same number of working components) then m(x) >= m(y),
// and strictly when the vectors differ and every increment is positive (true for [Exp]).
TEST(Properties, SignatureDominanceImpliesMttfDominance) {
  const IndependentExponential fm(1.0);
  std::size_t dominated_pairs = 0;
  std::size_t strict_pairs = 0;
  for (const SystemModel& model : test_systems()) {
    const SweepResult r = sweep(model, nullptr, {true});
    const std::vector<double> m = compute_all_mttf(model, fm);
    const std::uint64_t states = std::uint64_t{1} << model.n();
    for (std::uint64_t xi = 0; xi < states; ++xi) {
      for (std::uint64_t yi = 0; yi < states; ++yi) {
        const StateMask x = static_cast<StateMask>(xi);
        const StateMask y = static_cast<StateMask>(yi);
        if (xi == yi || working_count(x) != working_count(y) || !dominates(r.vectors[x], r.vectors[y])) {
          continue;
        }
        ++dominated_pairs;
        EXPECT_GE(m[x], m[y] - 1e-12) << "x=" << x << " y=" << y;
        if (r.vectors[x] != r.vectors[y]) {
          ++strict_pairs;
          EXPECT_GT(m[x], m[y] + 1e-12) << "x=" << x << " y=" << y;
        }
      }
    }
  }
  // make sure the test is not vacuous
  EXPECT_GT(dominated_pairs, 1000U);
  EXPECT_GT(strict_pairs, 100U);
}

// TC-I-04: the converse of Theorem 5 is false. A concrete counterexample (found by random search, then
// fixed here so the test is deterministic): a 6-component system whose minimal path sets are
// {0,5}, {3,4}, {1,4}, {3} and {0,1,3,5}. Take the 4-component states
//     x = {0,1,2,3} (mask 15)   W = [1, 3, 3, 1, 0]   tail (1, 3/4, 1/2, 1/4, 0)
//     y = {0,1,4,5} (mask 51)   W = [1, 4, 2, 0, 0]   tail (1, 1,   1/3, 0,   0)
// x is better after two or three more failures, y after exactly one, so their tail signatures cross and
// neither dominates. Yet m(x) = 1/4 + 1/4 + 1/4 + 1/4 = 1 > m(y) = 1/4 + 1/3 + 1/6 = 3/4.
TEST(Properties, MttfOrderDoesNotImplySignatureDominance) {
  const std::vector<StateMask> terms = {33, 24, 18, 8, 43};
  const SystemModel model = SystemModel::from_function(6, [terms](StateMask s) {
    return std::any_of(terms.begin(), terms.end(), [s](StateMask m) { return (s & m) == m; });
  });
  ASSERT_NO_THROW(model.validate());
  const SweepResult r = sweep(model, nullptr, {true});
  const StateMask x = 15;
  const StateMask y = 51;
  EXPECT_EQ(r.vectors[x], (CountingVector{1, 3, 3, 1, 0}));
  EXPECT_EQ(r.vectors[y], (CountingVector{1, 4, 2, 0, 0}));
  EXPECT_FALSE(dominates(r.vectors[x], r.vectors[y]));
  EXPECT_FALSE(dominates(r.vectors[y], r.vectors[x]));

  const std::vector<double> m = compute_all_mttf(model, IndependentExponential(1.0));
  EXPECT_NEAR(m[x], 1.0, 1e-12);
  EXPECT_NEAR(m[y], 0.75, 1e-12);
  EXPECT_GT(m[x], m[y]);  // larger MTTF, yet x does not dominate y: the converse fails
}

// TC-I-03: a state is a minimal path set exactly when it works and every son fails
TEST(Properties, MinimalPathSetCriterion) {
  for (const SystemModel& model : test_systems()) {
    const SweepResult r = sweep(model, nullptr, {true});
    const std::uint64_t states = std::uint64_t{1} << model.n();
    for (std::uint64_t xi = 1; xi < states; ++xi) {
      const StateMask x = static_cast<StateMask>(xi);
      bool all_sons_fail = true;
      for (StateMask son : sons(x)) {
        all_sons_fail = all_sons_fail && !model.phi(son);
      }
      const bool by_definition = model.phi(x) && all_sons_fail;
      const bool by_vector = r.vectors[x][0] == 1U && r.vectors[x][1] == 0U;
      EXPECT_EQ(by_vector, by_definition) << "x=" << x;
    }
  }
}

// Experiment 3 groundwork: two states with the same number of failed components can differ a lot.
TEST(Properties, SameFailureCountDifferentMttf) {
  const SystemModel model = load_reference();
  const std::vector<double> m = compute_all_mttf(model, IndependentExponential(1.0));
  EXPECT_NEAR(m[0b111010], 5.0 / 12.0, 1e-12);  // P1 and C1 failed: one unit left in two pairs
  EXPECT_EQ(m[0b111100], 0.0);                   // P1 and P2 failed: a whole pair is gone

  // among states with exactly two failures: MTTF is 0 when both failed units are partners, else 5/12
  std::set<double> two_failed;
  std::set<double> one_failed;
  for (StateMask x = 0; x < 64; ++x) {
    if (working_count(x) == 4) {
      two_failed.insert(std::round(m[x] * 1e9) / 1e9);
    }
    if (working_count(x) == 5) {
      one_failed.insert(std::round(m[x] * 1e9) / 1e9);
    }
  }
  EXPECT_EQ(two_failed.size(), 2U);
  EXPECT_EQ(*two_failed.begin(), 0.0);
  EXPECT_NEAR(*two_failed.rbegin(), 5.0 / 12.0, 1e-9);
  EXPECT_EQ(one_failed.size(), 1U);  // by symmetry, any single failure leaves the same MTTF
  EXPECT_NEAR(*one_failed.begin(), 8.0 / 15.0, 1e-9);
}
