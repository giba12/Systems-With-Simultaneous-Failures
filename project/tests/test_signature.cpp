// test_signature.cpp - Algorithm 1 against the reference and the oracles.   (Plan: Days 4-5)
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numeric>
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

// ---- Day 5: tail signature and signature ---------------------------------------------------------

namespace {

void ExpectNear(const std::vector<double>& actual, const std::vector<double>& expected,
                const std::string& label, double tolerance = 1e-15) {
  ASSERT_EQ(actual.size(), expected.size()) << label;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    EXPECT_NEAR(actual[i], expected[i], tolerance) << label << " entry " << i;
  }
}

// Signature of the whole system, computed from the sweep (state = all components working).
std::vector<double> system_signature(const SystemModel& model) {
  const SweepResult r = sweep(model, nullptr, keep_all());
  return signature(r.vectors[all_working(model.n())]);
}

// The classic 5-component bridge network. Components 1,2 are on the source side, 4,5 on the
// terminal side, 3 is the bridge.   Paths: {1,4} {2,5} {1,3,5} {2,3,4}   (bits 0..4 = components 1..5)
SystemModel bridge_network() {
  return SystemModel::from_function(5, [](StateMask x) {
    const bool c1 = is_working(x, 0), c2 = is_working(x, 1), c3 = is_working(x, 2),
               c4 = is_working(x, 3), c5 = is_working(x, 4);
    return (c1 && c4) || (c2 && c5) || (c1 && c3 && c5) || (c2 && c3 && c4);
  });
}

SystemModel k_out_of_n_working(int n, int k) {
  return SystemModel::from_function(n, [k](StateMask x) { return working_count(x) >= k; });
}

}  // namespace

TEST(Signature, TailSignatureOfKnownVectors) {
  ExpectNear(tail_signature({1, 6, 12, 8, 0, 0, 0}), {1, 1, 0.8, 0.4, 0, 0, 0}, "satellite");
  ExpectNear(tail_signature({1, 4, 4, 0, 0, 0}), {1, 0.8, 0.4, 0, 0, 0}, "P1 failed");
  ExpectNear(tail_signature({1, 3, 1, 0}), {1, 1, 1.0 / 3.0, 0}, "paper Fig. 1");
  ExpectNear(tail_signature({0}), {0}, "all failed");
}

// TC-A-03, B-03, C-06 and the paper's Fig. 1
TEST(Signature, SignatureOfKnownSystems) {
  ExpectNear(signature({1, 0, 0, 0}), {1, 0, 0}, "series(3)");
  ExpectNear(signature({1, 2, 0}), {0, 1}, "parallel(2)");
  ExpectNear(signature({1, 6, 12, 8, 0, 0, 0}), {0, 0.2, 0.4, 0.4, 0, 0}, "satellite");
  ExpectNear(signature({1, 4, 4, 0, 0, 0}), {0.2, 0.4, 0.4, 0, 0}, "satellite, P1 failed");
  ExpectNear(signature({1, 2, 0, 0, 0}), {0.5, 0.5, 0, 0}, "satellite, P1 and C1 failed");
  ExpectNear(signature({1, 3, 1, 0}), {0, 2.0 / 3.0, 1.0 / 3.0}, "paper Fig. 1");
}

TEST(Signature, SignatureOfAFailedSystemIsEmpty) {
  EXPECT_TRUE(signature({0}).empty());
  EXPECT_TRUE(signature({0, 0, 0, 0, 0}).empty());  // e.g. P1 and P2 failed
  EXPECT_EQ(signature({1}).size(), 0U);             // a lone state with no working component
}

TEST(Signature, InvalidCountingVectorsAreRejected) {
  EXPECT_THROW(tail_signature({}), std::invalid_argument);
  EXPECT_THROW(signature({}), std::invalid_argument);
  EXPECT_THROW(tail_signature({1, 7, 0}), std::invalid_argument);  // W[1] = 7 > C(2,1) = 2
  EXPECT_THROW(signature({1, 3, 1}), std::invalid_argument);       // W[1] = 3 > C(2,1) = 2
}

// TC-F-02: for every operational state of every test system, the entries are >= 0 and sum to 1
TEST(Signature, EntriesAreNonNegativeAndSumToOne) {
  std::vector<SystemModel> systems;
  systems.push_back(load_reference());
  systems.push_back(paper_figure_one());
  systems.push_back(bridge_network());
  for (int n = 2; n <= 10; ++n) {
    systems.push_back(SystemModel::series(n));
    systems.push_back(SystemModel::parallel(n));
  }
  std::mt19937 rng(31415);
  for (int t = 0; t < 40; ++t) {
    systems.push_back(random_monotone_system(3 + static_cast<int>(rng() % 6U), rng));
  }

  std::size_t checked = 0;
  for (std::size_t idx = 0; idx < systems.size(); ++idx) {
    const SweepResult r = sweep(systems[idx], nullptr, keep_all());
    for (std::size_t x = 0; x < r.vectors.size(); ++x) {
      if (r.vectors[x][0] == 0U) {
        EXPECT_TRUE(signature(r.vectors[x]).empty());
        continue;
      }
      const std::vector<double> s = signature(r.vectors[x]);
      ASSERT_EQ(s.size(), r.vectors[x].size() - 1U);
      double sum = 0.0;
      for (double entry : s) {
        EXPECT_GE(entry, 0.0) << "system " << idx << " state " << x;  // exact for monotone Phi
        sum += entry;
      }
      EXPECT_NEAR(sum, 1.0, 1e-14) << "system " << idx << " state " << x;
      // the tail signature falls from 1 to 0 and never rises (monotone Phi)
      const std::vector<double> tail = tail_signature(r.vectors[x]);
      EXPECT_EQ(tail.front(), 1.0);
      EXPECT_EQ(tail.back(), 0.0);
      EXPECT_TRUE(std::is_sorted(tail.rbegin(), tail.rend())) << "system " << idx << " state " << x;
      ++checked;
    }
  }
  EXPECT_GT(checked, 1000U);
}

// Mixed topology, independent of our own machinery: the 5-component bridge network has the
// well-known signature (0, 1/5, 3/5, 1/5, 0). Hand check: the minimal cuts of size 2 are {1,2} and
// {4,5}, so 2 of the C(5,2) = 10 failure pairs kill it (s2 = 1/5); after three failures only the two
// size-2 paths {1,4} and {2,5} survive, 2 of 10 (S3 = 1/5, so s3 = 4/5 - 1/5 = 3/5); no single
// component is a path, so s4 = 1/5 and s5 = 0.
TEST(Signature, BridgeNetworkHasTheClassicSignature) {
  const SystemModel bridge = bridge_network();
  ASSERT_NO_THROW(bridge.validate());
  ExpectNear(system_signature(bridge), {0, 0.2, 0.6, 0.2, 0}, "bridge");
  const SweepResult r = sweep(bridge, nullptr, keep_all());
  EXPECT_EQ(r.vectors[0b11111], (Vec{1, 5, 8, 2, 0, 0}));
  std::vector<StateMask> paths = r.minimal_path_sets;
  EXPECT_EQ(paths.size(), 4U);  // {1,4} {2,5} {1,3,5} {2,3,4}
  std::sort(paths.begin(), paths.end(), [](StateMask a, StateMask b) {
    return working_count(a) != working_count(b) ? working_count(a) < working_count(b) : a < b;
  });
  // sorted by size, then mask value: {1,4}=9  {2,5}=18  {2,3,4}=14  {1,3,5}=21
  EXPECT_EQ(paths, (std::vector<StateMask>{0b01001, 0b10010, 0b01110, 0b10101}));
}

// k-out-of-n:G (works while at least k components work) fails exactly at the (n-k+1)-th failure,
// so its signature is a single 1 at that position, and W[j] = C(n, j) for j <= n-k, else 0.
TEST(Signature, KOutOfNSystemsHaveAPointMassSignature) {
  for (int n = 2; n <= 9; ++n) {
    for (int k = 1; k <= n; ++k) {
      const SystemModel model = k_out_of_n_working(n, k);
      ASSERT_NO_THROW(model.validate()) << n << "," << k;
      std::vector<double> expected(static_cast<std::size_t>(n), 0.0);
      expected[static_cast<std::size_t>(n - k)] = 1.0;  // position n-k+1, zero-based n-k
      ExpectNear(system_signature(model), expected, "k=" + std::to_string(k) + " of n=" + std::to_string(n));

      const SweepResult r = sweep(model, nullptr, keep_all());
      const Vec& w = r.vectors[all_working(n)];
      for (int j = 0; j <= n; ++j) {
        EXPECT_EQ(w[static_cast<std::size_t>(j)], j <= n - k ? binomial(n, j) : 0U)
            << "n=" << n << " k=" << k << " j=" << j;
      }
    }
  }
}

// (3 components in parallel) in series with 1 component, written U0,U1,U2 | U3.
// The system fails at the earlier of two events: U3 fails, or the LAST of U0..U2 fails.
// With U3 at failure position p (uniform on 1..4): p = 1 or 2 -> fails at p; p = 3 -> the block's last
// member is 4th, so it fails at 3; p = 4 -> the block's last member is 3rd, so it fails at 3.
// Hence the signature is (1/4, 1/4, 1/2, 0). Counting agrees: W = [1, 3, 3, 0, 0].
TEST(Signature, SeriesOfParallelBlocksMatchesHandCount) {
  std::vector<Component> components;
  for (int i = 0; i < 4; ++i) {
    components.push_back({"U" + std::to_string(i), "", "", i});
  }
  const SystemModel model = SystemModel::and_of_or_groups(components, {{0, 1, 2}, {3}});
  const SweepResult r = sweep(model, nullptr, keep_all());
  EXPECT_EQ(r.vectors[0b1111], (Vec{1, 3, 3, 0, 0}));
  ExpectNear(signature(r.vectors[0b1111]), {0.25, 0.25, 0.5, 0}, "(3-parallel) in series with 1");
}

// The signature is a distribution over the ORDER of failures, so it cannot depend on how the
// components are labelled: permuting the bits of any system leaves its signature unchanged.
TEST(Signature, SignatureIsInvariantUnderRelabellingComponents) {
  const SystemModel original = bridge_network();
  const std::vector<int> permutation = {2, 0, 4, 1, 3};  // component i is renamed permutation[i]
  const SystemModel relabelled = SystemModel::from_function(5, [&](StateMask x) {
    StateMask y = 0;
    for (int i = 0; i < 5; ++i) {
      if (is_working(x, i)) {
        y |= StateMask{1} << permutation[static_cast<std::size_t>(i)];
      }
    }
    return original.phi(y);
  });
  ExpectNear(system_signature(relabelled), system_signature(original), "relabelled bridge");
  ExpectNear(system_signature(relabelled), {0, 0.2, 0.6, 0.2, 0}, "relabelled bridge, absolute");
}
