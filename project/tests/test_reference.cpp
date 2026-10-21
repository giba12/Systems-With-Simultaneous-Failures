// test_reference.cpp - the brute-force checker must itself be right.   (Plan: Day 3; ALG-005, gate G2)
// Until these pass, nobody may use the reference to judge the clever algorithm.
#include <gtest/gtest.h>

#include "reliability/reference.hpp"
#include "reliability/system_model.hpp"

using namespace reliability;

// ORC-1
TEST(Reference, SeriesOfThreeCountingVector) {
  GTEST_SKIP() << "needs brute_force_counting_vectors";
  // PSEUDO-CODE
  //   W = brute_force_counting_vectors(series(3))
  //   expect W[0b111] == {1, 0, 0, 0}
}

// ORC-2
TEST(Reference, OneOutOfTwoCountingVector) {
  GTEST_SKIP() << "needs brute_force_counting_vectors";
  // PSEUDO-CODE
  //   W = brute_force_counting_vectors(parallel(2))
  //   expect W[0b11] == {1, 2, 0}
}

// ORC-3, ORC-4, ORC-5
TEST(Reference, SatelliteSystemCountingVectors) {
  GTEST_SKIP() << "needs brute_force_counting_vectors";
  // PSEUDO-CODE
  //   W = brute_force_counting_vectors(reference system)
  //   expect W[63] == {1, 6, 12, 8, 0, 0, 0}
  //   expect W[62] == {1, 4, 4, 0, 0, 0}            (P1 failed)
  //   expect W[58] == {1, 2, 0, 0, 0}               (P1 and C1 failed)
  //   expect W[0b111100] == all zeros               (P1 and P2 failed: system is down)
}

// TC-C-10 (reference side)
TEST(Reference, SatelliteMinimalPathSets) {
  GTEST_SKIP() << "needs brute_force_minimal_path_sets";
  // PSEUDO-CODE
  //   sets = brute_force_minimal_path_sets(reference system)
  //   expect size(sets) == 8 and every set has exactly 3 working components (one per pair)
}

// ORC-6: the paper's own Fig. 1 system, n = 3, Phi = max{x3, min{x1, x2}}
TEST(Reference, PaperFigureOneCountingVectors) {
  GTEST_SKIP() << "needs brute_force_counting_vectors / from_function";
  // PSEUDO-CODE
  //   model = from_function(3, x -> x3 or (x1 and x2))        bit0=x1, bit1=x2, bit2=x3
  //   W = brute_force_counting_vectors(model)
  //   expect W[0b111] == {1, 3, 1, 0}
  //   expect W[0b101] == {1, 1, 0}  and W[0b110] == {1, 0, 0}  and W[0b011] == {1, 1, 0}
}

// REL-005 groundwork (needs the failure model too)
TEST(Reference, DirectMttfSolveMatchesKnownValues) {
  GTEST_SKIP() << "needs solve_mttf_direct and IndependentExponential";
  // PSEUDO-CODE
  //   m = solve_mttf_direct(reference system, exponential(lambda=1))
  //   expect m[63] ~ 0.7,  m[62] ~ 8/15,  m[58] ~ 5/12,  m[0b111100] == 0     (tolerance 1e-9)
}
