cmake_minimum_required(VERSION 3.22)

project(powerinfer-unit-test C CXX)

enable_testing()
include(GoogleTest)

function(powerinfer_add_test test_name test_sources)
    add_executable(${test_name} ${test_sources})
    target_link_libraries(${test_name} PRIVATE GTest::gtest_main)
    gtest_discover_tests(${test_name}) 
endfunction(powerinfer_add_test)
