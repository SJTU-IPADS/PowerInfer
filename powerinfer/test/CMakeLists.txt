cmake_minimum_required(VERSION 3.22)

project(powerinfer-test C CXX)
message("Add submodule powerinfer-test")

# -- Unit test

message(STATUS "Add module: Unit Test")
add_subdirectory(${PROJECT_SOURCE_DIR}/unit_test)

# -- Benchmark

message(STATUS "Add module: Benchmark")
add_subdirectory(${PROJECT_SOURCE_DIR}/benchmark)
