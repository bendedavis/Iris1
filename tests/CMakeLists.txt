cmake_minimum_required(VERSION 3.16)
project(iris1_unit_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(GTest REQUIRED)
include(GoogleTest)

enable_testing()

add_executable(iris1_unit_tests
  preset_learning_test.cpp
  midi_preset_controller_test.cpp
  midi_recall_timing_test.cpp
  reset_policy_test.cpp
  sequencer_test.cpp
  ../src/midi_preset_controller.cpp
  ../src/midi_recall_timing.cpp
  ../src/reset_policy.cpp
  ../src/sequencer.cpp
)

target_include_directories(iris1_unit_tests PRIVATE
  ../include
)

target_link_libraries(iris1_unit_tests PRIVATE
  GTest::gtest
  GTest::gtest_main
)

gtest_discover_tests(iris1_unit_tests)
