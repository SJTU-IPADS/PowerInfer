cmake_minimum_required(VERSION 3.16)

option(POWERINFER_WITH_TRACING  "Enable tracing for powerinfer library"     OFF)

project(powerinfer-perf C CXX)
message("Add submodule: ${PROJECT_NAME}")

file(GLOB_RECURSE perf_source_files ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_library(${PROJECT_NAME} STATIC ${perf_source_files})
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)

if (WIN32)
    # The perfetto library contains many symbols, so it needs the big object
    # format.
    target_compile_options(${PROJECT_NAME} PRIVATE "/bigobj")
    # Disable legacy features in windows.h.
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    # On Windows we should link to WinSock2.
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif (WIN32)

# Enable standards-compliant mode when using the Visual Studio compiler.
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/permissive-")
endif (MSVC)

if (POWERINFER_WITH_TRACING)
    target_link_libraries(${PROJECT_NAME} PRIVATE perfetto)
    target_compile_definitions(${PROJECT_NAME} PUBLIC POWERINFER_WITH_TRACING)
endif()
