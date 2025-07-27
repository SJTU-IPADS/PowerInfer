cmake_minimum_required(VERSION 3.10...3.26)

project(CLI11-package-config-test)

include(CTest)

find_package(PkgConfig)

if(CLI11_DIR)
  set(CMAKE_PREFIX_PATH ${CLI11_DIR} ${CLI11_DIR}/lib)
endif()

message(STATUS "${CLI11_DIR}-- ${CMAKE_PREFIX_PATH}")
pkg_check_modules(CLI11 REQUIRED IMPORTED_TARGET CLI11)

# Test the target
add_executable(package-config-test ../../examples/positional_validation.cpp)
target_link_libraries(package-config-test PkgConfig::CLI11)

add_test(NAME package-config-test1 COMMAND package-config-test one)
set_property(TEST package-config-test1 PROPERTY PASS_REGULAR_EXPRESSION "File 1 = one")
