# Space System Reliability Analyzer
#
# PSEUDO-CODE (what this build file does)
#   1. Require C++20 and turn on strict warnings.
#   2. Build the engine as a static library "reliability" from src/*.cpp
#      (everything except main.cpp).
#   3. Build the command-line tool "analyze" from src/main.cpp, linked to the library.
#   4. If BUILD_TESTING is on: download GoogleTest, build one test program
#      "reliability_tests" from tests/*.cpp, and register each test with CTest.
#   (Decision pending, see docs/requirements-spec.md open item 4: GoogleTest vs Catch2.
#    GoogleTest is the recommended default in docs/project-plan.md.)

cmake_minimum_required(VERSION 3.20)
project(space_system_reliability VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)  # recorded in the verification report (NFR-005)
endif()

option(BUILD_TESTING "Build the unit tests" ON)

add_library(reliability STATIC
  src/state.cpp
  src/system_model.cpp
  src/signature.cpp
  src/failure_model.cpp
  src/mttf.cpp
  src/reference.cpp
)
target_include_directories(reliability PUBLIC include)
if(MSVC)
  target_compile_options(reliability PRIVATE /W4)
else()
  target_compile_options(reliability PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(analyze src/main.cpp)
target_link_libraries(analyze PRIVATE reliability)

if(BUILD_TESTING)
  include(CTest)
  include(FetchContent)
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

  include(GoogleTest)
  gtest_discover_tests(reliability_tests)
endif()
