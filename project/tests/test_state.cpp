// test_state.cpp - state encoding tests.                  (Plan: Day 2; ALG-001, SYS-004)
// Every test is SKIPPED until its feature exists. To "turn one on": write the real check
// from the pseudo-code and delete the GTEST_SKIP line.
#include <gtest/gtest.h>

#include "reliability/state.hpp"

using namespace reliability;

// These three helpers are already real code, so their tests are real too.
TEST(State, WorkingCountCountsOneBits) {
  EXPECT_EQ(working_count(0b000000), 0);
  EXPECT_EQ(working_count(0b101010), 3);
  EXPECT_EQ(working_count(0b111111), 6);
}

TEST(State, AllWorkingSetsLowestNBits) {
  EXPECT_EQ(all_working(6), 63U);
  EXPECT_EQ(all_working(1), 1U);
}

TEST(State, WithoutClearsOneBit) {
  EXPECT_EQ(without(0b111111, 0), 0b111110U);  // P1 failed -> mask 62
  EXPECT_EQ(without(0b111010, 1), 0b111000U);
  EXPECT_FALSE(is_working(without(0b111111, 3), 3));
}

// TC-C-01
TEST(State, EnumerationGivesSixtyFourUniqueStates) {
  GTEST_SKIP() << "needs masks_with_working";
  // PSEUDO-CODE
  //   all = []
  //   for w in 0..6: all += masks_with_working(6, w)
  //   expect size(all) == 64 and all values distinct and each < 64
  //   expect the group sizes to be 1, 6, 15, 20, 15, 6, 1
}

TEST(State, SonsOfAState) {
  GTEST_SKIP() << "needs sons";
  // PSEUDO-CODE
  //   expect sons(0) is empty
  //   expect sons(0b111111) == {0b111110, 0b111101, 0b111011, 0b110111, 0b101111, 0b011111}
  //   for every x: size(sons(x)) == working_count(x) and every son is x with one bit cleared
}

TEST(State, BinomialMatchesPascalsTriangle) {
  GTEST_SKIP() << "needs binomial";
  // PSEUDO-CODE
  //   expect binomial(6, 0..6) == 1, 6, 15, 20, 15, 6, 1
  //   expect binomial(5, 7) == 0
  //   expect binomial(30, 15) == 155117520
  //   for n in 0..20, k in 0..n: binomial(n,k) == binomial(n-1,k-1) + binomial(n-1,k)
}
