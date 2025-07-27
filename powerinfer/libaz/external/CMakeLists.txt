add_subdirectory(fmt)


if (AZ_USE_INTERNAL_PERFETTO)
    add_subdirectory(perfetto)
endif()

set(CLI11_PRECOMPILED ON)
add_subdirectory(cli11)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest)
if (${CMAKE_C_COMPILER_ID} MATCHES "Clang")
    target_compile_options(gtest PRIVATE
        -Wno-character-conversion
        -Wno-deprecated-declarations
    )
endif()
