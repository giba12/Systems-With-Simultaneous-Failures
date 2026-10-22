# Space System Reliability Analyzer
#
# PSEUDO-CODE (what this build file does)
#   1. Require C++20 and turn on strict warnings.
#   2. Build the engine as a static library "reliability" from src/*.cpp
#      (everything except main.cpp).
#   3. Build the command-line tool "analyze" from src/main.cpp, linked to the library.
#   4. Download nlohmann/json (config parsing).
#   5. If BUILD_TESTING is on: download GoogleTest, build one test program
#      "reliability_tests" from tests/*.cpp, and register each test with CTest.
#   (Decision pending, see docs/requirements-spec.md open item 4: GoogleTest vs Catch2.
#    GoogleTest is the recommended default in docs/project-plan.md.)

cmake_minimum_required(VERSION 3.20)
project(space_system_reliability VERSION 0.8.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)  # recorded in the verification report (NFR-005)
endif()

option(BUILD_TESTING "Build the unit tests" ON)

# JSON parsing for configs/*.json. Decision made on Day 2: nlohmann/json 3.11.3 (MIT licence,
# header-only). It is downloaded at configure time, like GoogleTest below.
include(FetchContent)
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(nlohmann_json)

add_library(reliability STATIC
  src/state.cpp
  src/system_model.cpp
  src/signature.cpp
  src/failure_model.cpp
  src/mttf.cpp
  src/reference.cpp
)
target_include_directories(reliability PUBLIC include)
target_link_libraries(reliability PRIVATE nlohmann_json::nlohmann_json)
if(MSVC)
  target_compile_options(reliability PRIVATE /W4)
else()
  target_compile_options(reliability PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(analyze src/main.cpp)
target_link_libraries(analyze PRIVATE reliability nlohmann_json::nlohmann_json)
target_compile_definitions(analyze PRIVATE RELIABILITY_VERSION="${PROJECT_VERSION}")
if(NOT MSVC)
  target_compile_options(analyze PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  include(CTest)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_executable(reliability_tests
    tests/test_state.cpp
    tests/test_structure_function.cpp
    tests/test_reference.cpp
    tests/test_signature.cpp
    tests/test_failure_models.cpp
    tests/test_mttf.cpp
    tests/test_properties.cpp
  )
  target_link_libraries(reliability_tests PRIVATE reliability GTest::gtest_main)
  # Tests load the reference system from here, whatever the working directory is.
  target_compile_definitions(reliability_tests PRIVATE
    RELIABILITY_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/configs")

  include(GoogleTest)
  gtest_discover_tests(reliability_tests)

  # End-to-end tests of the command line tool and the experiment scripts (standard library only;
  # tests that need pandas / matplotlib skip themselves when those are missing).
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME python_end_to_end
      COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/python/tests -v
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python)
    set_tests_properties(python_end_to_end PROPERTIES
      ENVIRONMENT "ANALYZE_BIN=$<TARGET_FILE:analyze>;PROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
endif()
