add_executable(az-test-main "test_main.cpp")

add_subdirectory(core)

target_link_libraries(az-test-main PRIVATE az GTest::gtest)
