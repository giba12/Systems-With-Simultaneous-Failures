// test_properties.cpp - things the paper PROVES, checked on our code.   (Plan: Day 6; VER-004)
#include <gtest/gtest.h>

#include "reliability/failure_model.hpp"
#include "reliability/mttf.hpp"
#include "reliability/signature.hpp"
#include "reliability/system_model.hpp"

using namespace reliability;

// TC-I-02, paper Lemma 4
TEST(Properties, MttfNeverIncreasesWhenMoreComponentsFail) {
  GTEST_SKIP() << "needs compute_all_mttf";
  // PSEUDO-CODE
  //   for model in {exponential(1), lethal_shock(0.2), lethal_shock(0.5)}:
  //       m = compute_all_mttf(reference system, model)
  //       for all x, y with y a subset of x (y & x == y): expect m[y] <= m[x] + 1e-12
}

// TC-I-01, paper Theorem 5
TEST(Properties, SignatureDominanceImpliesMttfDominance) {
  GTEST_SKIP() << "needs sweep and compute_all_mttf";
  // PSEUDO-CODE
  //   for each pair x, y with the same number of working components w:
  //       if tail_signature(W[x])[k] >= tail_signature(W[y])[k] for every k:
  //            expect m[x] >= m[y]
  //            if the tail signatures differ and all increments > 0: expect m[x] > m[y]
  //   repeat on random monotone systems (n <= 8, fixed seed), for [Exp] and [LSh]
}

// TC-I-04 (the converse is false)
TEST(Properties, EqualMttfDoesNotMeanEqualSignature) {
  GTEST_SKIP() << "optional: find a pair of states with crossing tail signatures";
  // PSEUDO-CODE
  //   search random systems for x, y, same w, with neither tail signature dominating
  //   document the example; do not assert any ordering
}

// TC-I-03
TEST(Properties, MinimalPathSetCriterion) {
  GTEST_SKIP() << "needs sweep";
  // PSEUDO-CODE
  //   states flagged by the sweep (W0 == 1 and W1 == 0) are exactly the states x with
  //   Phi(x) = 1 whose every son has Phi = 0
}

// TC-G-05 groundwork, Experiment 3 (same number of failures, different reliability)
TEST(Properties, SameFailureCountDifferentMttf) {
  GTEST_SKIP() << "needs compute_all_mttf";
  // PSEUDO-CODE
  //   m = compute_all_mttf(reference system, exp(1))
  //   expect m[0b111010] (P1 and C1 failed) ~ 5/12   and   m[0b111100] (P1 and P2 failed) == 0
}
