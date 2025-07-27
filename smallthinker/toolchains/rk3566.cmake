# rk3566.cmake
set(CLANG_VERSION 19)
set(LIBSTDCXX_VERSION 11)
set(TARGET_PYTHON_VERSION 3.10)
set(MARCH_FLAGS -march=armv8-a+nodotprod+noi8mm+nosve)

include(${CMAKE_CURRENT_LIST_DIR}/aarch64-linux-gnu.cmake)