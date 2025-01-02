#
# gpt-2

set(TEST_TARGET gpt-2-sparse)
add_executable(${TEST_TARGET} main7b.cpp)
# target_link_libraries(${TEST_TARGET} PRIVATE ggml common common-ggml)
target_link_libraries(${TEST_TARGET} PRIVATE common  llama ${CMAKE_THREAD_LIBS_INIT})

#
# gpt-2-quantize

set(TEST_TARGET gpt-2-quantize)
add_executable(${TEST_TARGET} quantize.cpp)
# target_link_libraries(${TEST_TARGET} PRIVATE ggml common common-ggml)
target_link_libraries(${TEST_TARGET} PRIVATE ggml common)
