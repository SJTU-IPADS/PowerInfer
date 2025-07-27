add_executable(az-random-memtest "random_memtest.cpp")
target_link_libraries(az-random-memtest PRIVATE az CLI11::CLI11)

add_executable(az-test-assert "test_assert.cpp")
target_link_libraries(az-test-assert PRIVATE az)
