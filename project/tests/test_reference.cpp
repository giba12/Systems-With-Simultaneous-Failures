// test_reference.cpp - the brute-force checker must itself be right.   (Plan: Day 3; ALG-005, gate G2)
// Until these pass, nobody may use the reference to judge the clever algorithm.
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "reliability/errors.hpp"
#include "reliability/failure_model.hpp"
#include "reliability/reference.hpp"
#include "reliability/state.hpp"
#include "reliability/system_model.hpp"

using namespace reliability;

namespace {

const std::string kReferenceConfig = std::string(RELIABILITY_CONFIG_DIR) + "/satellite_support.json";

SystemModel load_reference() { return SystemModel::from_json_file(kReferenceConfig); }

// The paper's Fig. 1 system: n = 3, Phi = max{x3, min{x1, x2}}; bit0 = x1, bit1 = x2, bit2 = x3.
SystemModel paper_figure_one() {
  return SystemModel::from_function(3, [](StateMask x) {
    const bool x1 = is_working(x, 0);
    const bool x2 = is_working(x, 1);
    const bool x3 = is_working(x, 2);
    return x3 || (x1 && x2);
  });
}

using Vec = CountingVector;

}  // namespace

// ORC-1
TEST(Reference, SeriesOfThreeCountingVector) {
  const auto w = brute_force_counting_vectors(SystemModel::series(3));
  EXPECT_EQ(w[0b111], (Vec{1, 0, 0, 0}));
}

// ORC-2
TEST(Reference, OneOutOfTwoCountingVector) {
  const auto w = brute_force_counting_vectors(SystemModel::parallel(2));
  EXPECT_EQ(w[0b11], (Vec{1, 2, 0}));
  EXPECT_EQ(w[0b01], (Vec{1, 0}));
  EXPECT_EQ(w[0b10], (Vec{1, 0}));
  EXPECT_EQ(w[0b00], (Vec{0}));
}

// ORC-3, ORC-4, ORC-5
TEST(Reference, SatelliteSystemCountingVectors) {
  const auto w = brute_force_counting_vectors(load_reference());
  ASSERT_EQ(w.size(), 64U);
  EXPECT_EQ(w[63], (Vec{1, 6, 12, 8, 0, 0, 0}));
  EXPECT_EQ(w[62], (Vec{1, 4, 4, 0, 0, 0}));  // P1 failed
  EXPECT_EQ(w[58], (Vec{1, 2, 0, 0, 0}));     // P1 and C1 failed
  EXPECT_EQ(w[0b111100], (Vec{0, 0, 0, 0, 0}));  // P1 and P2 failed: the system is down
  EXPECT_EQ(w[0], (Vec{0}));
}

// A vector has |x| + 1 entries, W[0] is Phi(x), and a failed system has an all-zero vector.
TEST(Reference, CountingVectorShapeAndBasicProperties) {
  const SystemModel model = load_reference();
  const auto w = brute_force_counting_vectors(model);
  for (StateMask x = 0; x < 64; ++x) {
    const Vec& v = w[x];
    ASSERT_EQ(v.size(), static_cast<std::size_t>(working_count(x)) + 1U) << "x=" << x;
    EXPECT_EQ(v[0], model.phi(x) ? 1U : 0U) << "x=" << x;
    if (!model.phi(x)) {
      EXPECT_TRUE(std::all_of(v.begin(), v.end(), [](auto e) { return e == 0; })) << "x=" << x;
    }
    // W[k] counts k-subsets, so it can never exceed C(|x|, k).
    for (std::size_t k = 0; k < v.size(); ++k) {
      EXPECT_LE(v[k], binomial(working_count(x), static_cast<int>(k))) << "x=" << x << " k=" << k;
    }
  }
}

// Closed forms that need no brute force at all, so a bug in the reference cannot hide behind
// the same bug in the expectation: series -> [1,0,...,0]; parallel -> [C(n,0), ..., C(n,n-1), 0].
TEST(Reference, SeriesAndParallelMatchClosedForms) {
  for (int n = 2; n <= 8; ++n) {
    const StateMask all = all_working(n);
    const auto series = brute_force_counting_vectors(SystemModel::series(n));
    Vec expected_series(static_cast<std::size_t>(n) + 1, 0);
    expected_series[0] = 1;
    EXPECT_EQ(series[all], expected_series) << "series n=" << n;

    const auto parallel = brute_force_counting_vectors(SystemModel::parallel(n));
    Vec expected_parallel(static_cast<std::size_t>(n) + 1, 0);
    for (int k = 0; k < n; ++k) {
      expected_parallel[static_cast<std::size_t>(k)] = binomial(n, k);
    }
    EXPECT_EQ(parallel[all], expected_parallel) << "parallel n=" << n;
  }
}

// A redundant pair repeated m times: the full-state vector has the closed form
//   W[k] = number of ways to remove k components without emptying a pair
//        = sum over j (pairs with both... ) -> here: coefficient of z^k in (1 + 2z)^m
TEST(Reference, RepeatedPairsMatchBinomialExpansion) {
  for (int pairs = 1; pairs <= 4; ++pairs) {
    std::vector<Component> components;
    std::vector<std::vector<int>> groups;
    for (int i = 0; i < 2 * pairs; ++i) {
      components.push_back({"U" + std::to_string(i), "", "", i});
    }
    for (int p = 0; p < pairs; ++p) {
      groups.push_back({2 * p, 2 * p + 1});
    }
    const SystemModel model = SystemModel::and_of_or_groups(components, groups);
    const auto w = brute_force_counting_vectors(model);
    const Vec& full = w[all_working(2 * pairs)];
    ASSERT_EQ(full.size(), static_cast<std::size_t>(2 * pairs) + 1U);
    for (int k = 0; k <= 2 * pairs; ++k) {
      // remove k components, at most one from each pair: choose k pairs, then one of 2 in each
      const std::uint64_t expected = k <= pairs ? binomial(pairs, k) * (std::uint64_t{1} << k) : 0U;
      EXPECT_EQ(full[static_cast<std::size_t>(k)], expected) << "pairs=" << pairs << " k=" << k;
    }
  }
}

// The enumeration performs exactly sum_x 2^|x| = 3^n calls to Phi.
TEST(Reference, CountingVectorsCallPhiThreeToTheNTimes) {
  const SystemModel model = SystemModel::parallel(4);
  model.reset_phi_calls();
  brute_force_counting_vectors(model);
  EXPECT_EQ(model.phi_calls(), 81U);  // 3^4
}

// TC-C-10 (reference side)
TEST(Reference, SatelliteMinimalPathSets) {
  const auto sets = brute_force_minimal_path_sets(load_reference());
  ASSERT_EQ(sets.size(), 8U);
  for (StateMask s : sets) {
    EXPECT_EQ(working_count(s), 3) << "mask " << s;
    // exactly one working component in each redundant pair (bits 0-1, 2-3, 4-5)
    EXPECT_EQ(working_count(s & 0b000011U), 1) << "mask " << s;
    EXPECT_EQ(working_count(s & 0b001100U), 1) << "mask " << s;
    EXPECT_EQ(working_count(s & 0b110000U), 1) << "mask " << s;
  }
  EXPECT_TRUE(std::is_sorted(sets.begin(), sets.end()));
}

TEST(Reference, MinimalPathSetsOfSimpleSystems) {
  // series(3): the only path set is everything; parallel(3): each single component
  EXPECT_EQ(brute_force_minimal_path_sets(SystemModel::series(3)), (std::vector<StateMask>{0b111}));
  EXPECT_EQ(brute_force_minimal_path_sets(SystemModel::parallel(3)),
            (std::vector<StateMask>{0b001, 0b010, 0b100}));
}

// ORC-6: the paper's own Fig. 1 system
TEST(Reference, PaperFigureOneCountingVectors) {
  const auto w = brute_force_counting_vectors(paper_figure_one());
  EXPECT_EQ(w[0b111], (Vec{1, 3, 1, 0}));
  // the batteries drawn in the paper's figure, one per state
  EXPECT_EQ(w[0b101], (Vec{1, 1, 0}));  // x = (1,0,1)
}

TEST(Reference, PaperFigureOneEveryState) {
  // Bit order: bit0 = x1, bit1 = x2, bit2 = x3. Mask 0b011 is x = (1,1,0); 0b110 is x = (0,1,1).
  const auto w = brute_force_counting_vectors(paper_figure_one());
  EXPECT_EQ(w[0b000], (Vec{0}));
  EXPECT_EQ(w[0b001], (Vec{0, 0}));        // (1,0,0)
  EXPECT_EQ(w[0b010], (Vec{0, 0}));        // (0,1,0)
  EXPECT_EQ(w[0b100], (Vec{1, 0}));        // (0,0,1)
  EXPECT_EQ(w[0b011], (Vec{1, 0, 0}));     // (1,1,0): works, but losing either component breaks it
  EXPECT_EQ(w[0b101], (Vec{1, 1, 0}));     // (1,0,1)
  EXPECT_EQ(w[0b110], (Vec{1, 1, 0}));     // (0,1,1)
  EXPECT_EQ(w[0b111], (Vec{1, 3, 1, 0}));  // (1,1,1)
}

TEST(Reference, PaperFigureOneMinimalPathSets) {
  // Phi = x3 or (x1 and x2): the minimal path sets are {3} = 0b100 and {1,2} = 0b011
  EXPECT_EQ(brute_force_minimal_path_sets(paper_figure_one()), (std::vector<StateMask>{0b011, 0b100}));
}

// TC-E-12 (reference side): an irrelevant component is in no minimal path set
TEST(Reference, IrrelevantComponentIsInNoMinimalPathSet) {
  const SystemModel model =
      SystemModel::from_function(3, [](StateMask x) { return (x & 0b011U) == 0b011U; });
  ASSERT_NO_THROW(model.validate());
  const auto sets = brute_force_minimal_path_sets(model);
  EXPECT_EQ(sets, (std::vector<StateMask>{0b011}));
  const auto w = brute_force_counting_vectors(model);
  // component 3 can fail freely: with everything working, removing it never matters
  EXPECT_EQ(w[0b111], (Vec{1, 1, 0, 0}));
}

TEST(Reference, RefusesSystemsTooLargeForBruteForce) {
  const SystemModel big = SystemModel::parallel(kMaxBruteForceComponents + 1);
  EXPECT_THROW(brute_force_counting_vectors(big), ModelError);
  EXPECT_THROW(brute_force_minimal_path_sets(big), ModelError);
}

// REL-005: Eq. (6), solved directly state by state, gives the known MTTF values.
TEST(Reference, DirectMttfSolveMatchesKnownValues) {
  const IndependentExponential fm(1.0);
  const std::vector<double> m = solve_mttf_direct(load_reference(), fm);
  ASSERT_EQ(m.size(), 64U);
  EXPECT_NEAR(m[63], 0.7, 1e-12);
  EXPECT_NEAR(m[62], 8.0 / 15.0, 1e-12);
  EXPECT_NEAR(m[58], 5.0 / 12.0, 1e-12);
  EXPECT_EQ(m[0b111100], 0.0);
  EXPECT_EQ(m[0], 0.0);

  // the paper's Fig. 1 value and the textbook cases
  EXPECT_NEAR(solve_mttf_direct(paper_figure_one(), fm)[0b111], 7.0 / 6.0, 1e-12);
  EXPECT_NEAR(solve_mttf_direct(SystemModel::series(4), fm)[0b1111], 0.25, 1e-12);
  EXPECT_NEAR(solve_mttf_direct(SystemModel::parallel(2), fm)[0b11], 1.5, 1e-12);
}

TEST(Reference, DirectMttfSolveScalesWithLambdaAndRefusesLargeSystems) {
  const SystemModel model = load_reference();
  const std::vector<double> base = solve_mttf_direct(model, IndependentExponential(1.0));
  const std::vector<double> fast = solve_mttf_direct(model, IndependentExponential(4.0));
  for (std::size_t x = 0; x < 64; ++x) {
    EXPECT_NEAR(fast[x] * 4.0, base[x], 1e-12);
  }
  EXPECT_THROW(solve_mttf_direct(SystemModel::parallel(kMaxBruteForceComponents + 1), IndependentExponential(1.0)),
               ModelError);
}
