// test_signature.cpp - Algorithm 1 against the reference and the oracles.   (Plan: Days 4-5)
#include <gtest/gtest.h>

#include "reliability/reference.hpp"
#include "reliability/signature.hpp"
#include "reliability/system_model.hpp"

using namespace reliability;

// TC-C-03, VER-001: the main safety net. Recursive result == brute force, state by state.
TEST(Signature, SweepMatchesBruteForceOnEveryStateOfTheReferenceSystem) {
  GTEST_SKIP() << "needs sweep";
  // PSEUDO-CODE
  //   model = reference system
  //   result = sweep(model, observer = none, options{keep_all_vectors = true})
  //   ref = brute_force_counting_vectors(model)
  //   for x in 0..63: expect result.vectors[x] == ref[x]       (print x and k on any difference)
}

// TC-C-09 (Theorem 2): Phi is asked exactly once per state, and memory stays at two levels
TEST(Signature, SweepEvaluatesPhiExactlyOncePerState) {
  GTEST_SKIP() << "needs sweep";
  // PSEUDO-CODE
  //   result = sweep(reference system, none, {})
  //   expect result.phi_calls == 64 and model.phi_calls() == 64
  //   expect result.peak_vectors_held <= C(6,3) + C(6,2)   (two largest neighbouring levels: 20 + 15)
}

// TC-C-04, C-05, C-14 (paper Fig. 1)
TEST(Signature, KnownCountingVectors) {
  GTEST_SKIP() << "needs sweep";
  // PSEUDO-CODE
  //   reference system:  W[63] = {1,6,12,8,0,0,0};  W[62] = {1,4,4,0,0,0};  W[58] = {1,2,0,0,0}
  //   paper Fig. 1 system (n=3):  W[7] = {1,3,1,0};  W[5] = {1,1,0};  W[4] = {1,0};  W[3] = {1,0,0}
}

// TC-A-02/03, B-02/03, C-06
TEST(Signature, SignatureOfKnownSystems) {
  GTEST_SKIP() << "needs signature";
  // PSEUDO-CODE
  //   series(3):            signature({1,0,0,0})             == (1, 0, 0)
  //   parallel(2):          signature({1,2,0})               == (0, 1)
  //   reference system:     signature({1,6,12,8,0,0,0})      == (0, 0.2, 0.4, 0.4, 0, 0)
  //   P1 failed:            signature({1,4,4,0,0,0})         == (0.2, 0.4, 0.4, 0, 0)
  //   paper Fig. 1:         signature({1,3,1,0})             == (0, 2/3, 1/3)
}

// TC-F-02
TEST(Signature, EntriesAreNonNegativeAndSumToOne) {
  GTEST_SKIP() << "needs sweep and signature";
  // PSEUDO-CODE
  //   for every operational state x of every test system:
  //       s = signature(W[x]);  expect every s[k] >= -1e-12 and |sum(s) - 1| < 1e-12
}

// TC-C-10, TC-I-03
TEST(Signature, MinimalPathSetsComeFromTheSweep) {
  GTEST_SKIP() << "needs sweep";
  // PSEUDO-CODE
  //   result = sweep(reference system, none, {})
  //   expect sorted(result.minimal_path_sets) == sorted(brute_force_minimal_path_sets(model))
  //   expect 8 sets, each of size 3
}

// TC-F-05
TEST(Signature, RandomMonotoneSystemsAgreeWithBruteForce) {
  GTEST_SKIP() << "needs sweep and a random monotone system generator";
  // PSEUDO-CODE
  //   rng = fixed seed
  //   repeat 50 times:
  //       n = random in 3..8
  //       Phi = random monotone function (e.g. OR over a few random AND-terms, then validate())
  //       expect sweep vectors == brute-force vectors for all states
}

// TC-E-12
TEST(Signature, IrrelevantComponentIsNeverInAMinimalPathSet) {
  GTEST_SKIP() << "needs sweep";
  // PSEUDO-CODE
  //   model = system whose Phi ignores component 3
  //   expect no minimal path set contains bit 3, and sweep == brute force
}
