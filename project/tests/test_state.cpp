// test_state.cpp - state encoding tests.                  (Plan: Day 2; ALG-001, SYS-004)
#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <vector>

#include "reliability/state.hpp"

using namespace reliability;

TEST(State, WorkingCountCountsOneBits) {
  EXPECT_EQ(working_count(0b000000), 0);
  EXPECT_EQ(working_count(0b101010), 3);
  EXPECT_EQ(working_count(0b111111), 6);
}

TEST(State, AllWorkingSetsLowestNBits) {
  EXPECT_EQ(all_working(6), 63U);
  EXPECT_EQ(all_working(1), 1U);
  EXPECT_EQ(all_working(0), 0U);
  EXPECT_EQ(all_working(31), 0x7FFFFFFFU);
}

TEST(State, WithoutClearsOneBit) {
  EXPECT_EQ(without(0b111111, 0), 0b111110U);  // P1 failed -> mask 62
  EXPECT_EQ(without(0b111010, 1), 0b111000U);
  EXPECT_FALSE(is_working(without(0b111111, 3), 3));
  EXPECT_EQ(without(0b101, 1), 0b101U);  // clearing a bit that is already 0 changes nothing
}

// TC-C-01
TEST(State, EnumerationGivesSixtyFourUniqueStates) {
  std::vector<StateMask> all;
  std::vector<std::size_t> group_sizes;
  for (int w = 0; w <= 6; ++w) {
    const std::vector<StateMask> level = masks_with_working(6, w);
    group_sizes.push_back(level.size());
    for (StateMask x : level) {
      EXPECT_EQ(working_count(x), w);  // every mask in the level really has w working bits
    }
    all.insert(all.end(), level.begin(), level.end());
  }
  EXPECT_EQ(all.size(), 64U);
  const std::set<StateMask> unique(all.begin(), all.end());
  EXPECT_EQ(unique.size(), 64U);
  EXPECT_EQ(*unique.begin(), 0U);
  EXPECT_EQ(*unique.rbegin(), 63U);
  EXPECT_EQ(group_sizes, (std::vector<std::size_t>{1, 6, 15, 20, 15, 6, 1}));
}

TEST(State, MasksWithWorkingAreIncreasingAndHandleEdgeCases) {
  EXPECT_EQ(masks_with_working(4, 2), (std::vector<StateMask>{0b0011, 0b0101, 0b0110, 0b1001, 0b1010, 0b1100}));
  EXPECT_EQ(masks_with_working(5, 0), (std::vector<StateMask>{0}));
  EXPECT_EQ(masks_with_working(5, 5), (std::vector<StateMask>{0b11111}));
  EXPECT_TRUE(masks_with_working(5, 6).empty());
  EXPECT_TRUE(masks_with_working(5, -1).empty());
  EXPECT_TRUE(masks_with_working(32, 1).empty());
  // The largest legal n must not overflow while stepping.
  const std::vector<StateMask> top = masks_with_working(31, 30);
  EXPECT_EQ(top.size(), 31U);
  EXPECT_EQ(top.front(), 0x3FFFFFFFU);  // lowest 30 bits set
  EXPECT_EQ(top.back(), 0x7FFFFFFEU);   // only bit 0 cleared
}

TEST(State, SonsOfAState) {
  EXPECT_TRUE(sons(0).empty());
  EXPECT_EQ(sons(0b111111),
            (std::vector<StateMask>{0b111110, 0b111101, 0b111011, 0b110111, 0b101111, 0b011111}));
  for (StateMask x = 0; x < 64; ++x) {
    const std::vector<StateMask> s = sons(x);
    EXPECT_EQ(static_cast<int>(s.size()), working_count(x));
    for (StateMask son : s) {
      EXPECT_EQ(working_count(son), working_count(x) - 1);
      EXPECT_EQ(son & x, son);  // a son is a sub-state of x
    }
  }
}

TEST(State, BinomialMatchesPascalsTriangle) {
  const std::vector<std::uint64_t> row6 = {1, 6, 15, 20, 15, 6, 1};
  for (int k = 0; k <= 6; ++k) {
    EXPECT_EQ(binomial(6, k), row6[static_cast<std::size_t>(k)]);
  }
  EXPECT_EQ(binomial(5, 7), 0U);
  EXPECT_EQ(binomial(5, -1), 0U);
  EXPECT_EQ(binomial(0, 0), 1U);
  EXPECT_EQ(binomial(30, 15), 155117520U);
  EXPECT_EQ(binomial(60, 30), 118264581564861424ULL);
  for (int n = 1; n <= 40; ++n) {
    for (int k = 1; k < n; ++k) {
      EXPECT_EQ(binomial(n, k), binomial(n - 1, k - 1) + binomial(n - 1, k)) << n << "," << k;
    }
  }
  EXPECT_THROW(binomial(61, 3), std::out_of_range);
}

TEST(State, LevelSizesAddUpToTwoToTheN) {
  for (int n = 0; n <= 12; ++n) {
    std::uint64_t total = 0;
    for (int w = 0; w <= n; ++w) {
      total += masks_with_working(n, w).size();
    }
    EXPECT_EQ(total, std::uint64_t{1} << n) << "n = " << n;
  }
}
