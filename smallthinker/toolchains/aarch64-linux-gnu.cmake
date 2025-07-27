set(SYSROOT_PATH $ENV{SYSROOT_PATH} CACHE PATH "Sysroot path")
if ("${SYSROOT_PATH}" STREQUAL "")
    message(ERROR "SYSROOT_PATH is not defined as an environment variable")
endif()

# Search for linker program in host machine
find_program(AARCH64_GNU_LD_PROGRAM NAMES aarch64-linux-gnu-ld NO_CMAKE_FIND_ROOT_PATH)
find_program(MOLD_PROGRAM NAMES mold NO_CMAKE_FIND_ROOT_PATH)

if (NOT "${MOLD_PROGRAM}" STREQUAL "")
    set(AARCH64_LD ${MOLD_PROGRAM})
elseif (NOT "${AARCH64_GNU_LD_PROGRAM}" STREQUAL "")
    set(AARCH64_LD ${AARCH64_GNU_LD_PROGRAM})
else()
    message(ERROR "No suitable linker program found")
endif()

find_program(CLANG NAMES clang-${CLANG_VERSION} PATHS /usr REQUIRED)
find_program(CLANGPP NAMES clang++-${CLANG_VERSION} PATHS /usr REQUIRED)

set(TARGET "aarch64-linux-gnu")

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER ${CLANG})
set(CMAKE_CXX_COMPILER ${CLANGPP})

set(CMAKE_SYSROOT ${SYSROOT_PATH})
set(CMAKE_FIND_ROOT_PATH ${SYSROOT_PATH})

add_compile_options(
    --target=${TARGET}
    ${MARCH_FLAGS}
    -flto=thin
    # -fprofile-generate
    # -fprofile-use=/data/profraw/profile.profdata
    # -fcs-profile-generate
)
add_link_options(
    --target=${TARGET}
    -fuse-ld=${AARCH64_LD}
    -flto=thin
    # -fprofile-generate
    # -fprofile-use=/data/profraw/profile.profdata
    # -fcs-profile-generate
)

# TODO: Auto detect libstdc++ version
include_directories(
  ${SYSROOT_PATH}/usr/include/c++/${LIBSTDCXX_VERSION}
  ${SYSROOT_PATH}/usr/include/c++/${LIBSTDCXX_VERSION}/backward
  ${SYSROOT_PATH}/usr/include/aarch64-linux-gnu/c++/${LIBSTDCXX_VERSION}
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(DISABLE_ARM_FEATURE_CHECK ON)

# Tell pybind11 where the target python installation is
set(PYTHON_INCLUDE_DIRS ${SYSROOT_PATH}/usr/include/python${TARGET_PYTHON_VERSION} CACHE INTERNAL "Cross python include path")
set(PYTHON_MODULE_EXTENSION ".so" CACHE INTERNAL "Cross python lib extension")

# Disable pybind11 python search mechanism
set(PYTHONLIBS_FOUND TRUE CACHE INTERNAL "")
