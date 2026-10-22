// test_structure_function.cpp - the system model and Phi.   (Plan: Day 2; SYS-001/003/004/005)
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

#include "reliability/errors.hpp"
#include "reliability/state.hpp"
#include "reliability/system_model.hpp"

using namespace reliability;

namespace {

const std::string kReferenceConfig = std::string(RELIABILITY_CONFIG_DIR) + "/satellite_support.json";

SystemModel load_reference() { return SystemModel::from_json_file(kReferenceConfig); }

// Runs f and checks that it throws ModelError whose message contains `expected`.
void ExpectModelError(const std::function<void()>& f, const std::string& expected) {
  try {
    f();
  } catch (const ModelError& e) {
    EXPECT_NE(std::string(e.what()).find(expected), std::string::npos)
        << "expected the message to contain \"" << expected << "\" but it was: " << e.what();
    return;
  } catch (const std::exception& e) {
    ADD_FAILURE() << "expected ModelError, got a different exception: " << e.what();
    return;
  }
  ADD_FAILURE() << "expected ModelError containing \"" << expected << "\" but nothing was thrown";
}

// Writes `text` to a temporary file and returns its path.
std::string write_temp(const std::string& name, const std::string& text) {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / ("reliability_test_" + name + ".json");
  std::ofstream(path) << text;
  return path.string();
}

// A valid 3-component config to which individual tests add one defect.
std::string config_with(const std::string& components, const std::string& structure) {
  return R"({"components": [)" + components + R"(], "structure": )" + structure + "}";
}

const std::string kThree =
    R"({"id":"A","bit":0},{"id":"B","bit":1},{"id":"C","bit":2})";
const std::string kThreeSeries = R"({"type":"series"})";

}  // namespace

// TC-C-02
TEST(StructureFunction, ReferenceSystemHasTwentySevenWorkingStates) {
  const SystemModel model = load_reference();
  int working = 0;
  for (StateMask x = 0; x < 64; ++x) {
    if (model.phi(x)) {
      ++working;
    }
  }
  EXPECT_EQ(working, 27);  // 3 working choices per pair: 3 * 3 * 3
  EXPECT_TRUE(model.phi(63));
  EXPECT_FALSE(model.phi(0));
  EXPECT_FALSE(model.phi(0b110011));  // both communications units failed (bits 2 and 3)
  EXPECT_FALSE(model.phi(0b111100));  // both power units failed (bits 0 and 1)
  EXPECT_TRUE(model.phi(0b111110));   // P1 failed, P2 still works
}

// TC-A-01, TC-B-01
TEST(StructureFunction, SeriesAndParallelTruthTables) {
  for (int n = 2; n <= 6; ++n) {
    const SystemModel series = SystemModel::series(n);
    const SystemModel parallel = SystemModel::parallel(n);
    EXPECT_NO_THROW(series.validate());
    EXPECT_NO_THROW(parallel.validate());
    const StateMask all = all_working(n);
    for (StateMask x = 0; x <= all; ++x) {
      EXPECT_EQ(series.phi(x), x == all) << "series n=" << n << " x=" << x;
      EXPECT_EQ(parallel.phi(x), x != 0U) << "parallel n=" << n << " x=" << x;
    }
  }
  // 1-out-of-2: three working states out of four (TC-B-01)
  const SystemModel pair = SystemModel::parallel(2);
  int working = 0;
  for (StateMask x = 0; x < 4; ++x) {
    working += pair.phi(x) ? 1 : 0;
  }
  EXPECT_EQ(working, 3);
}

// TC-C-11
TEST(StructureFunction, ReferenceSystemIsMonotone) {
  const SystemModel model = load_reference();
  EXPECT_NO_THROW(model.validate());
  // Independent check, not using validate(): y <= x must imply Phi(y) <= Phi(x).
  for (StateMask x = 0; x < 64; ++x) {
    for (StateMask y = 0; y < 64; ++y) {
      if ((y & x) == y && model.phi(y)) {
        EXPECT_TRUE(model.phi(x)) << "y=" << y << " x=" << x;
      }
    }
  }
}

// TC-C-12
TEST(StructureFunction, ConfigLoadsSixNamedComponents) {
  const SystemModel model = load_reference();
  ASSERT_EQ(model.n(), 6);
  const std::vector<std::string> ids = {"P1", "P2", "C1", "C2", "K1", "K2"};
  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(model.components()[static_cast<std::size_t>(i)].id, ids[static_cast<std::size_t>(i)]);
    EXPECT_EQ(model.components()[static_cast<std::size_t>(i)].bit, i);
  }
  EXPECT_EQ(model.components()[0].name, "Power unit A");
  EXPECT_EQ(model.components()[5].role, "Backup processing");
}

TEST(StructureFunction, ComponentsAreSortedByBitWhateverTheFileOrder) {
  const std::string path = write_temp(
      "shuffled", config_with(R"({"id":"C","bit":2},{"id":"A","bit":0},{"id":"B","bit":1})",
                              kThreeSeries));
  const SystemModel model = SystemModel::from_json_file(path);
  EXPECT_EQ(model.components()[0].id, "A");
  EXPECT_EQ(model.components()[1].id, "B");
  EXPECT_EQ(model.components()[2].id, "C");
}

TEST(StructureFunction, JsonSeriesAndParallelTypesWork) {
  const SystemModel series = SystemModel::from_json_file(
      write_temp("series", config_with(kThree, kThreeSeries)));
  const SystemModel parallel = SystemModel::from_json_file(
      write_temp("parallel", config_with(kThree, R"({"type":"parallel"})")));
  for (StateMask x = 0; x < 8; ++x) {
    EXPECT_EQ(series.phi(x), x == 7U);
    EXPECT_EQ(parallel.phi(x), x != 0U);
  }
}

// TC-E-01, E-04, E-06, E-10, E-11 (invalid models) and E-12 (irrelevant components allowed)
TEST(StructureFunction, ValidateRejectsBadSystemsButAcceptsIrrelevantComponents) {
  ExpectModelError([] { SystemModel::from_function(0, [](StateMask) { return false; }).validate(); },
                   "between 2 and 24");
  ExpectModelError([] { SystemModel::from_function(1, [](StateMask x) { return x != 0U; }).validate(); },
                   "got 1");

  // constant functions are trivial systems
  ExpectModelError([] { SystemModel::from_function(3, [](StateMask) { return true; }).validate(); },
                   "Phi(0) must be 0");
  ExpectModelError([] { SystemModel::from_function(3, [](StateMask) { return false; }).validate(); },
                   "Phi(1) must be 1");

  // XOR of two components: Phi(0) = 0 but Phi(1) = 0, so it is rejected as trivial-at-the-top
  EXPECT_THROW(SystemModel::from_function(2, [](StateMask x) { return ((x & 1U) ^ ((x >> 1) & 1U)) != 0U; })
                   .validate(),
               ModelError);

  // Phi(0) = 0, Phi(1) = 1, but component 1 failing REPAIRS state 011 -> not monotone
  ExpectModelError(
      [] {
        SystemModel::from_function(3, [](StateMask x) { return x == 0b001U || x == 0b111U; }).validate();
      },
      "not monotone");

  // more components than the supported maximum
  ExpectModelError([] { SystemModel::parallel(25).validate(); }, "between 2 and 24");
  EXPECT_THROW(SystemModel::parallel(32), ModelError);  // cannot even be represented in a mask

  // TC-E-12: component 3 (bit 2) is irrelevant, which the paper allows
  const SystemModel irrelevant =
      SystemModel::from_function(3, [](StateMask x) { return (x & 0b011U) == 0b011U; });
  EXPECT_NO_THROW(irrelevant.validate());
}

// TC-E-02, E-03, E-09 (bad configuration files)
TEST(StructureFunction, BadConfigFilesGiveHelpfulErrors) {
  ExpectModelError([] { SystemModel::from_json_file("/no/such/file.json"); }, "cannot open");

  ExpectModelError(
      [] {
        SystemModel::from_json_file(write_temp(
            "dup_id", config_with(R"({"id":"A","bit":0},{"id":"A","bit":1})", kThreeSeries)));
      },
      "duplicate component id 'A'");
  ExpectModelError(
      [] {
        SystemModel::from_json_file(write_temp(
            "dup_bit", config_with(R"({"id":"A","bit":0},{"id":"B","bit":0})", kThreeSeries)));
      },
      "both use bit 0");
  ExpectModelError(
      [] {
        SystemModel::from_json_file(write_temp(
            "bit_range", config_with(R"({"id":"A","bit":0},{"id":"B","bit":5})", kThreeSeries)));
      },
      "component 'B' uses bit 5");
  ExpectModelError(
      [] {
        SystemModel::from_json_file(write_temp(
            "unknown_id",
            config_with(kThree, R"({"type":"and_of_or_groups","groups":[["A","ZZ"],["B","C"]]})")));
      },
      "unknown component id 'ZZ'");
  ExpectModelError(
      [] {
        SystemModel::from_json_file(
            write_temp("bad_type", config_with(kThree, R"({"type":"triangle"})")));
      },
      "unknown structure type 'triangle'");
  ExpectModelError([] { SystemModel::from_json_file(write_temp("malformed", R"({"components": [)")); },
                   "malformed JSON");
  ExpectModelError([] { SystemModel::from_json_file(write_temp("no_components", R"({"structure":{}})")); },
                   "missing required field 'components'");
  ExpectModelError(
      [] {
        SystemModel::from_json_file(
            write_temp("no_structure", R"({"components":[{"id":"A","bit":0},{"id":"B","bit":1}]})"));
      },
      "missing required field 'structure'");
  ExpectModelError(
      [] {
        SystemModel::from_json_file(write_temp(
            "no_id", config_with(R"({"bit":0},{"id":"B","bit":1})", kThreeSeries)));
      },
      "missing required field 'id' in components[0]");
  ExpectModelError(
      [] {
        SystemModel::from_json_file(write_temp(
            "bit_type", config_with(R"({"id":"A","bit":"zero"},{"id":"B","bit":1})", kThreeSeries)));
      },
      "'bit' in components[0] must be an integer");
  // a single component fails the paper's n > 1 rule even in a well-formed file
  ExpectModelError(
      [] {
        SystemModel::from_json_file(
            write_temp("single", config_with(R"({"id":"A","bit":0})", kThreeSeries)));
      },
      "got 1");
}

TEST(StructureFunction, AndOfOrGroupsRejectsBadGroups) {
  std::vector<Component> two = {{"A", "", "", 0}, {"B", "", "", 1}};
  ExpectModelError([&] { SystemModel::and_of_or_groups(two, {{0, 7}}); }, "refers to bit 7");
  ExpectModelError([&] { SystemModel::and_of_or_groups(two, {{0}, {}}); }, "group 1 is empty");
  // a component outside every group is irrelevant, which is allowed
  EXPECT_NO_THROW(SystemModel::and_of_or_groups(two, {{0}}).validate());
}

// TC-E-05
TEST(StructureFunction, PhiRejectsOutOfRangeMask) {
  const SystemModel model = load_reference();
  EXPECT_NO_THROW(model.phi(63));
  ExpectModelError([&] { model.phi(64); }, "out of range");
  ExpectModelError([&] { model.phi(0xFFFFFFFFU); }, "out of range");
}

// Groundwork for TC-C-09: the call counter that the sweep will rely on
TEST(StructureFunction, PhiCallsAreCountedAndValidationDoesNotPolluteTheCount) {
  const SystemModel model = load_reference();  // loading validates, which must leave the counter at 0
  EXPECT_EQ(model.phi_calls(), 0U);
  model.phi(1);
  model.phi(2);
  model.phi(3);
  EXPECT_EQ(model.phi_calls(), 3U);
  model.validate();
  EXPECT_EQ(model.phi_calls(), 0U);  // validate() resets the counter when it finishes
  model.phi(1);
  model.reset_phi_calls();
  EXPECT_EQ(model.phi_calls(), 0U);
  // a failed validation also resets it
  const SystemModel bad = SystemModel::from_function(3, [](StateMask) { return true; });
  EXPECT_THROW(bad.validate(), ModelError);
  EXPECT_EQ(bad.phi_calls(), 0U);
}

// Traceability: TC-E-13
// The path-set form (OR of ANDs): the bridge network written as its four minimal path sets, from code and from JSON.
TEST(StructureFunction, PathSetStructuresWorkFromCodeAndJson) {
  std::vector<Component> five;
  for (int i = 0; i < 5; ++i) {
    five.push_back({"C" + std::to_string(i + 1), "", "", i});
  }
  const SystemModel bridge = SystemModel::or_of_and_groups(five, {{0, 3}, {1, 4}, {0, 2, 4}, {1, 2, 3}});
  ASSERT_NO_THROW(bridge.validate());
  EXPECT_TRUE(bridge.phi(0b01001));   // {1,4}
  EXPECT_TRUE(bridge.phi(0b10101));   // {1,3,5}
  EXPECT_FALSE(bridge.phi(0b00011));  // {1,2}: a cut
  EXPECT_FALSE(bridge.phi(0b11000));  // {4,5}: a cut
  EXPECT_TRUE(bridge.phi(0b11111));

  const std::string path = write_temp(
      "bridge",
      R"({"components": [{"id":"a","bit":0},{"id":"b","bit":1},{"id":"c","bit":2},{"id":"d","bit":3},{"id":"e","bit":4}],
          "structure": {"type":"path_sets","paths":[["a","d"],["b","e"],["a","c","e"],["b","c","d"]]}})");
  const SystemModel from_json = SystemModel::from_json_file(path);
  for (StateMask x = 0; x < 32; ++x) {
    EXPECT_EQ(from_json.phi(x), bridge.phi(x)) << x;
  }
}

// Traceability: TC-E-13
TEST(StructureFunction, PathSetStructuresRejectBadInput) {
  std::vector<Component> two = {{"A", "", "", 0}, {"B", "", "", 1}};
  ExpectModelError([&] { SystemModel::or_of_and_groups(two, {{0}, {}}); }, "path set 1 is empty");
  ExpectModelError([&] { SystemModel::or_of_and_groups(two, {{0, 9}}); }, "refers to bit 9");
  ExpectModelError(
      [&] {
        SystemModel::from_json_file(write_temp(
            "paths_unknown", config_with(R"({"id":"A","bit":0},{"id":"B","bit":1})",
                                         R"({"type":"path_sets","paths":[["A","Z"]]})")));
      },
      "unknown component id 'Z'");
  ExpectModelError(
      [&] {
        SystemModel::from_json_file(write_temp(
            "paths_missing", config_with(R"({"id":"A","bit":0},{"id":"B","bit":1})", R"({"type":"path_sets"})")));
      },
      "missing required field 'paths'");
  // no path sets at all: the system can never work, which validation rejects
  ExpectModelError([&] { SystemModel::or_of_and_groups(two, {}).validate(); }, "Phi(1) must be 1");
}
