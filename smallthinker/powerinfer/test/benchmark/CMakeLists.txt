cmake_minimum_required(VERSION 3.22)

project(powerinfer-benchmark C CXX)

function(powerinfer_add_benchmark bench_name bench_sources)
    add_executable(${bench_name} ${bench_sources})
    target_link_libraries(${bench_name} PRIVATE benchmark::benchmark)
    gtest_discover_tests(${bench_name})
endfunction(powerinfer_add_benchmark)

powerinfer_add_benchmark(bench_example ${PROJECT_SOURCE_DIR}/bench_example.cpp)
