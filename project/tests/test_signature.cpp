// test_signature.cpp - Algorithm 1 against the reference and the oracles.   (Plan: Days 4-5)
#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include "reliability/errors.hpp"
#include "reliability/reference.hpp"
#include "reliability/signature.hpp"
#include "reliability/state.hpp"
#include "reliability/system_model.hpp"

using namespace reliability;

namespace {

using Vec = CountingVector;

const std::string kReferenceConfig = std::string(RELIABILITY_CONFIG_DIR) + "/satellite_support.json";

SystemModel load_reference() { return SystemModel::from_json_file(kReferenceConfig); }

// The paper's Fig. 1 system: n = 3, Phi = max{x3, min{x1, x2}}; bit0 = x1, bit1 = x2, bit2 = x3.
SystemModel paper_figure_one() {
  return SystemModel::from_function(3, [](StateMask x) {
    return is_working(x, 2) || (is_working(x, 0) && is_working(x, 1));
  });
}

SweepOptions keep_all() {
  SweepOptions options;
  options.keep_all_vectors = true;
  return options;
}

// Any Boolean function at all (not necessarily monotone), from a random truth table.
SystemModel random_function_system(int n, std::mt19937& rng) {
  std::vector<std::uint8_t> table(std::size_t{1} << n);
  for (auto& bit : table) {
    bit = static_cast<std::uint8_t>(rng() & 1U);
  }
  return SystemModel::from_function(n, [table](StateMask x) { return table[x] != 0; });
}

// A random monotone, non-trivial system: Phi = OR of a few random non-empty AND-terms.
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

void ExpectSweepEqualsBruteForce(const SystemModel& model, const std::string& label) {
  const SweepResult result = sweep(model, nullptr, keep_all());
  const std::vector<CountingVector> reference = brute_force_counting_vectors(model);
  ASSERT_EQ(result.vectors.size(), reference.size()) << label;
  for (std::size_t x = 0; x < reference.size(); ++x) {
    ASSERT_EQ(result.vectors[x], reference[x]) << label << ", state " << x;
  }
}

}  // namespace

// TC-C-03, VER-001: the main safety net. Recursive result == brute force, state by state.
TEST(Signature, SweepMatchesBruteForceOnEveryStateOfTheReferenceSystem) {
  const SystemModel model = load_reference();
  const SweepResult result = sweep(model, nullptr, keep_all());
  const auto reference = brute_force_counting_vectors(model);
  ASSERT_EQ(result.vectors.size(), 64U);
  for (StateMask x = 0; x < 64; ++x) {
    EXPECT_EQ(result.vectors[x], reference[x]) << "state " << x;
  }
}

// TC-C-09 (Theorem 2): Phi is asked exactly once per state, and memory stays at two levels
TEST(Signature, SweepEvaluatesPhiExactlyOncePerState) {
  const SystemModel model = load_reference();
  const SweepResult result = sweep(model, nullptr, {});
  EXPECT_EQ(result.phi_calls, 64U);
  EXPECT_EQ(model.phi_calls(), 64U);
  // two largest neighbouring levels of n = 6: C(6,3) + C(6,2) = 20 + 15 (never all 64 at once)
  EXPECT_LE(result.peak_vectors_held, 35U);
  EXPECT_GT(result.peak_vectors_held, 0U);

  for (int n = 1; n <= 12; ++n) {
    const SystemModel parallel = SystemModel::parallel(n);
    const SweepResult r = sweep(parallel, nullptr, {});
    EXPECT_EQ(r.phi_calls, std::uint64_t{1} << n) << "n = " << n;
    // the peak is the largest sum of two adjacent binomial coefficients
    std::uint64_t expected_peak = 0;
    for (int w = 0; w <= n; ++w) {
      expected_peak = std::max(expected_peak, binomial(n, w) + binomial(n, w - 1));
    }
    EXPECT_EQ(r.peak_vectors_held, expected_peak) << "n = " << n;
  }
}

TEST(Signature, SweepResetsTheCallCounterItself) {
  const SystemModel model = load_reference();
  model.phi(1);
  model.phi(2);  // some earlier calls
  const SweepResult result = sweep(model, nullptr, {});
  EXPECT_EQ(result.phi_calls, 64U);  // not 66
}

// TC-C-04, C-05, C-14 (paper Fig. 1)
TEST(Signature, KnownCountingVectors) {
  const SweepResult sat = sweep(load_reference(), nullptr, keep_all());
  EXPECT_EQ(sat.vectors[63], (Vec{1, 6, 12, 8, 0, 0, 0}));
  EXPECT_EQ(sat.vectors[62], (Vec{1, 4, 4, 0, 0, 0}));
  EXPECT_EQ(sat.vectors[58], (Vec{1, 2, 0, 0, 0}));
  EXPECT_EQ(sat.vectors[0b111100], (Vec{0, 0, 0, 0, 0}));

  const SweepResult fig = sweep(paper_figure_one(), nullptr, keep_all());
  EXPECT_EQ(fig.vectors[0b111], (Vec{1, 3, 1, 0}));
  EXPECT_EQ(fig.vectors[0b101], (Vec{1, 1, 0}));
  EXPECT_EQ(fig.vectors[0b110], (Vec{1, 1, 0}));
  EXPECT_EQ(fig.vectors[0b100], (Vec{1, 0}));
  EXPECT_EQ(fig.vectors[0b011], (Vec{1, 0, 0}));
  EXPECT_EQ(fig.vectors[0b000], (Vec{0}));
}

// The observer sees every state exactly once, level by level, with the final vector.
TEST(Signature, ObserverSeesEveryStateOnceInSweepOrder) {
  const SystemModel model = load_reference();
  std::vector<StateMask> order;
  std::vector<CountingVector> seen;
  const SweepResult result = sweep(
      model,
      [&](StateMask x, const CountingVector& w) {
        order.push_back(x);
        seen.push_back(w);
        EXPECT_EQ(w.size(), static_cast<std::size_t>(working_count(x)) + 1U);
      },
      keep_all());
  ASSERT_EQ(order.size(), 64U);
  // levels ascend by number of working components; inside a level masks increase
  for (std::size_t i = 1; i < order.size(); ++i) {
    const int prev_w = working_count(order[i - 1]);
    const int w = working_count(order[i]);
    EXPECT_TRUE(prev_w < w || (prev_w == w && order[i - 1] < order[i])) << "position " << i;
  }
  std::vector<StateMask> sorted = order;
  std::sort(sorted.begin(), sorted.end());
  for (StateMask x = 0; x < 64; ++x) {
    EXPECT_EQ(sorted[x], x);  // every mask exactly once
  }
  for (std::size_t i = 0; i < order.size(); ++i) {
    EXPECT_EQ(seen[i], result.vectors[order[i]]);
  }
}

TEST(Signature, VectorsAreOnlyKeptWhenAskedFor) {
  const SweepResult lean = sweep(load_reference(), nullptr, {});
  EXPECT_TRUE(lean.vectors.empty());
  const SweepResult full = sweep(load_reference(), nullptr, keep_all());
  EXPECT_EQ(full.vectors.size(), 64U);
}

// Series and parallel have closed forms (no brute force involved); sweep must agree for larger n too.
TEST(Signature, SeriesAndParallelMatchClosedFormsUpToSixteenComponents) {
  for (int n = 2; n <= 16; ++n) {
    const StateMask all = all_working(n);
    const SweepResult series = sweep(SystemModel::series(n), nullptr, keep_all());
    Vec expected_series(static_cast<std::size_t>(n) + 1, 0);
    expected_series[0] = 1;
    EXPECT_EQ(series.vectors[all], expected_series) << "series n=" << n;

    const SweepResult parallel = sweep(SystemModel::parallel(n), nullptr, keep_all());
    Vec expected_parallel(static_cast<std::size_t>(n) + 1, 0);
    for (int k = 0; k < n; ++k) {
      expected_parallel[static_cast<std::size_t>(k)] = binomial(n, k);
    }
    EXPECT_EQ(parallel.vectors[all], expected_parallel) << "parallel n=" << n;
  }
}

// TC-F-05: random monotone systems, fixed seed
TEST(Signature, RandomMonotoneSystemsAgreeWithBruteForce) {
  std::mt19937 rng(20261021);
  for (int trial = 0; trial < 60; ++trial) {
    const int n = 3 + static_cast<int>(rng() % 6U);  // 3..8
    const SystemModel model = random_monotone_system(n, rng);
    ASSERT_NO_THROW(model.validate()) << "trial " << trial;
    ExpectSweepEqualsBruteForce(model, "monotone trial " + std::to_string(trial) + " n=" + std::to_string(n));
  }
}

// The paper: "no assumption whatsoever is made on Phi". Lemma 1 is pure counting, so the sweep must
// also be right for arbitrary (even non-monotone, even trivial) Boolean functions.
TEST(Signature, ArbitraryBooleanFunctionsAlsoAgreeWithBruteForce) {
  std::mt19937 rng(424242);
  for (int trial = 0; trial < 60; ++trial) {
    const int n = 1 + static_cast<int>(rng() % 8U);  // 1..8
    ExpectSweepEqualsBruteForce(random_function_system(n, rng),
                                "arbitrary trial " + std::to_string(trial) + " n=" + std::to_string(n));
  }
}

// TC-E-12
TEST(Signature, IrrelevantComponentIsNeverInAMinimalPathSet) {
  const SystemModel model =
      SystemModel::from_function(3, [](StateMask x) { return (x & 0b011U) == 0b011U; });
  ASSERT_NO_THROW(model.validate());
  ExpectSweepEqualsBruteForce(model, "irrelevant component");
  const SweepResult result = sweep(model, nullptr, {});
  EXPECT_EQ(result.minimal_path_sets, (std::vector<StateMask>{0b011}));
}

// TC-C-10, TC-I-03
TEST(Signature, MinimalPathSetsComeFromTheSweep) {
  const SystemModel model = load_reference();
  const SweepResult result = sweep(model, nullptr, {});
  std::vector<StateMask> sets = result.minimal_path_sets;
  std::sort(sets.begin(), sets.end());
  EXPECT_EQ(sets, brute_force_minimal_path_sets(model));
  ASSERT_EQ(sets.size(), 8U);
  for (StateMask s : sets) {
    EXPECT_EQ(working_count(s), 3);
  }
  // Fig. 1 system: {x3} and {x1, x2}
  std::vector<StateMask> fig = sweep(paper_figure_one(), nullptr, {}).minimal_path_sets;
  std::sort(fig.begin(), fig.end());
  EXPECT_EQ(fig, (std::vector<StateMask>{0b011, 0b100}));
}

// For random MONOTONE systems the sweep's shortcut (W0 = 1 and W1 = 0) equals the definition.
TEST(Signature, MinimalPathSetShortcutEqualsDefinitionOnRandomMonotoneSystems) {
  std::mt19937 rng(777);
  for (int trial = 0; trial < 40; ++trial) {
    const int n = 3 + static_cast<int>(rng() % 6U);
    const SystemModel model = random_monotone_system(n, rng);
    std::vector<StateMask> sets = sweep(model, nullptr, {}).minimal_path_sets;
    std::sort(sets.begin(), sets.end());
    EXPECT_EQ(sets, brute_force_minimal_path_sets(model)) << "trial " << trial << " n=" << n;
  }
}

TEST(Signature, SweepRejectsSystemsOutsideTheSupportedSize) {
  const SystemModel empty = SystemModel::from_function(0, [](StateMask) { return false; });
  EXPECT_THROW(sweep(empty, nullptr, {}), ModelError);
  EXPECT_THROW(sweep(SystemModel::parallel(kMaxComponents + 1), nullptr, {}), ModelError);
  // a single component is allowed by the sweep (the paper's n > 1 rule belongs to validate())
  EXPECT_NO_THROW(sweep(SystemModel::parallel(1), nullptr, {}));
}

// ---- Day 5: signature formulas (still skeleton) -------------------------------------------------

// TC-A-03/03, B-03, C-06
TEST(Signature, SignatureOfKnownSystems) {
  GTEST_SKIP() << "Day 5: needs signature";
  // PSEUDO-CODE
  //   series(3):            signature({1,0,0,0})             == (1, 0, 0)
  //   parallel(2):          signature({1,2,0})               == (0, 1)
  //   reference system:     signature({1,6,12,8,0,0,0})      == (0, 0.2, 0.4, 0.4, 0, 0)
  //   P1 failed:            signature({1,4,4,0,0,0})         == (0.2, 0.4, 0.4, 0, 0)
  //   paper Fig. 1:         signature({1,3,1,0})             == (0, 2/3, 1/3)
}

// TC-F-02
TEST(Signature, EntriesAreNonNegativeAndSumToOne) {
  GTEST_SKIP() << "Day 5: needs signature";
  // PSEUDO-CODE
  //   for every operational state x of every test system:
  //       s = signature(W[x]);  expect every s[k] >= -1e-12 and |sum(s) - 1| < 1e-12
}
