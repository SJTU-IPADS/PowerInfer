# Let's make everything harder
if(MSVC)
    # add_compile_options(/W4 /WX)
else()
    add_compile_options(
        -Wall -Wextra -Werror
        -Wno-unused-function
    )
endif()

include_directories(${AZ_SOURCE_DIR})

add_library(az)

target_sources(az PRIVATE
    "init.cpp"
)

# add_compile_definitions: Pass definitions to all sub-targets
# target_compile_definitions: Propagate definitions to all users of libaz

if (AZ_DISABLE_ASSERT)
    add_compile_definitions(AZ_DISABLE_ASSERT)
    target_compile_definitions(az PUBLIC AZ_DISABLE_ASSERT)
endif()

if (AZ_DISABLE_DEBUG_ASSERT)
    add_compile_definitions(AZ_DISABLE_DEBUG_ASSERT)
    target_compile_definitions(az PUBLIC AZ_DISABLE_DEBUG_ASSERT)
endif()

if (AZ_ENABLE_PERFETTO)
    add_compile_definitions(AZ_ENABLE_PERFETTO)
    target_compile_definitions(az PUBLIC AZ_ENABLE_PERFETTO)
endif()


# Add subdirectories here, after adding all definitions

add_subdirectory(core)
target_link_libraries(az PUBLIC az-core)

add_subdirectory(cpu)
target_link_libraries(az PUBLIC az-cpu)

add_subdirectory(pipeline)
target_link_libraries(az PUBLIC az-pipeline)


target_include_directories(az PUBLIC ${AZ_SOURCE_DIR})
