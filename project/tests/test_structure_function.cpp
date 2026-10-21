// test_structure_function.cpp - the system model and Phi.   (Plan: Day 2; SYS-001/003/004/005)
#include <gtest/gtest.h>

#include "reliability/errors.hpp"
#include "reliability/system_model.hpp"

using namespace reliability;

// TC-C-02
TEST(StructureFunction, ReferenceSystemHasTwentySevenWorkingStates) {
  GTEST_SKIP() << "needs SystemModel::from_json_file / and_of_or_groups";
  // PSEUDO-CODE
  //   model = from_json_file("configs/satellite_support.json")
  //   count = number of x in 0..63 with model.phi(x)
  //   expect count == 27                       (3 choices per pair, 3*3*3)
  //   expect model.phi(63) and not model.phi(0)
  //   expect not model.phi(0b110011)           (both communications units failed -> bits 2,3 = 0)
}

// TC-A-01, TC-B-01
TEST(StructureFunction, SeriesAndParallelTruthTables) {
  GTEST_SKIP() << "needs series / parallel";
  // PSEUDO-CODE
  //   for n in 2..6: series(n) works only for the all-ones mask; parallel(n) fails only for mask 0
}

// TC-C-11
TEST(StructureFunction, ReferenceSystemIsMonotone) {
  GTEST_SKIP() << "needs phi";
  // PSEUDO-CODE
  //   for every x and every working bit i: expect phi(without(x,i)) <= phi(x)
}

// TC-C-12
TEST(StructureFunction, ConfigLoadsSixNamedComponents) {
  GTEST_SKIP() << "needs from_json_file";
  // PSEUDO-CODE
  //   components = model.components()
  //   expect ids == [P1,P2,C1,C2,K1,K2] and bits == [0,1,2,3,4,5]
}

// TC-E-01, E-10, E-11, E-12 (invalid models)
TEST(StructureFunction, ValidateRejectsBadSystemsButAcceptsIrrelevantComponents) {
  GTEST_SKIP() << "needs validate";
  // PSEUDO-CODE
  //   expect ModelError for: n = 0, n = 1, constant-true Phi, constant-false Phi
  //   expect ModelError for XOR of two components                              (E-04, not monotone)
  //   expect NO error for a 3-component system whose Phi ignores component 3   (E-12, semi-coherent)
}

// TC-E-02, E-03, E-09 (bad configuration files)
TEST(StructureFunction, BadConfigFilesGiveHelpfulErrors) {
  GTEST_SKIP() << "needs from_json_file";
  // PSEUDO-CODE
  //   duplicate id          -> ModelError whose message contains the duplicated id
  //   unknown id in group   -> ModelError whose message contains the unknown id
  //   malformed JSON        -> ModelError (message names the problem)
  //   missing "components"  -> ModelError naming the missing field
}

// TC-E-05
TEST(StructureFunction, PhiRejectsOutOfRangeMask) {
  GTEST_SKIP() << "needs phi";
  // PSEUDO-CODE
  //   expect ModelError for model.phi(64) on the 6-component system
}
