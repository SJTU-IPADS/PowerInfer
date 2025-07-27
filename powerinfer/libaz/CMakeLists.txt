cmake_minimum_required(VERSION 3.16)
project("libaz" C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED true)

set(BUILD_SHARED_LIBS OFF)

# Options

include(CMakeDependentOption)

option(AZ_ENABLE_ASAN "Enable address santizer" OFF)
option(AZ_ENABLE_UBSAN "Enable undefined santizer" OFF)
option(AZ_ENABLE_LTO "Enable link-time optimization" OFF)

option(AZ_DISABLE_ASSERT "Disable runtime assertions" OFF)
option(AZ_DISABLE_DEBUG_ASSERT "Disable runtime debug assertions" OFF)

option(AZ_ENABLE_PERFETTO "Enable Perfetto tracing" OFF)
option(AZ_USE_INTERNAL_PERFETTO "Use internal Perfetto" ON)


if (AZ_ENABLE_ASAN)
    if (ANDROID)
        add_compile_options(-fsanitize=hwaddress -fno-omit-frame-pointer)
        link_libraries(-fsanitize=hwaddress)
    else()
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        link_libraries(-fsanitize=address)
    endif()
endif()

if (AZ_ENABLE_UBSAN)
    if (ANDROID)
        add_compile_options(-fsanitize=undefined,bounds -static-libsan)
        link_libraries(-fsanitize=undefined,bounds -static-libsan)
    else()
        add_compile_options(-fsanitize=undefined)
        link_libraries(-fsanitize=undefined)
    endif()
endif()

if (AZ_ENABLE_LTO)
    if (${CMAKE_C_COMPILER_ID} MATCHES "Clang")
        add_compile_options(-flto=thin)
    else()
        add_compile_options(-flto)
    endif()
endif()

# Remove -DNDEBUG from the default RelWithDebInfo flags
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")

set(AZ_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(external)

add_subdirectory(az)

add_subdirectory(bin)
add_subdirectory(tests)
