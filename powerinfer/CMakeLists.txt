cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED     true)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED       true)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-narrowing")

option(CREATE_DISK_CREDIT       "Create a new disk credit if not existed"   OFF)
option(POWERINFER_ENABLE_TEST   "Enable compiling test"                     OFF)
option(POWERINFER_BUILD_SHARED  "Build shared library of powerinfer"        ON )

if (NOT DEFINED POWERINFER_GROUP_SIZE)
    set(POWERINFER_GROUP_SIZE 32)
endif ()

set(POWERINFER_VERSION 0.1.0)

project(powerinfer
    LANGUAGES C CXX ${extrapowerinfer_language}
    VERSION ${POWERINFER_VERSION})

if (CREATE_DISK_CREDIT)
    add_compile_definitions(CREATE_DISK_CREDIT)
endif()

add_compile_definitions(POWERINFER_BUILD)

# --- Architecture environment

include(${PROJECT_SOURCE_DIR}/cmake/Arch.cmake)

# --- Third part libraries

add_subdirectory(third_part)

if (POWERINFER_WITH_TRACING)
    set(AZ_ENABLE_PERFETTO ON CACHE BOOL "" FORCE)
    set(AZ_USE_INTERNAL_PERFETTO OFF CACHE BOOL ""  FORCE)
endif()
add_subdirectory(libaz)

# --- Project Configuration

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/out)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

FILE(GLOB_RECURSE sources ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/src/*.cu)
FILE(GLOB_RECURSE headers ${PROJECT_SOURCE_DIR}/include/*.h ${PROJECT_SOURCE_DIR}/include/*.hpp)

add_subdirectory(${PROJECT_SOURCE_DIR}/powerinfer-perf)
add_subdirectory(${PROJECT_SOURCE_DIR}/powerinfer-common)
set(COMMON_LIB_TARGET powerinfer-perf powerinfer-common)
add_subdirectory(${PROJECT_SOURCE_DIR}/powerinfer-disk)
set(DISK_LIB_TARGET powerinfer-disk)
add_subdirectory(${PROJECT_SOURCE_DIR}/powerinfer-cpu)
set(CPU_LIB_TARGET powerinfer-cpu)

add_subdirectory(${PROJECT_SOURCE_DIR}/moe_sparse_pipeline)
set(MOE_SPARSE_PIPELINE_TARGET powerinfer-moe-sparse-pipeline)

add_subdirectory(${PROJECT_SOURCE_DIR}/fused_sparse_moe)
set(FUSED_SPARSE_MOE_TARGET powerinfer-fused-sparse-moe)


if (NOT BUILD_SHARED_LIBS)
    set(POWERINFER_BUILD_SHARED OFF)
endif()

if (POWERINFER_BUILD_SHARED)
    set(LIB_TARGET "powerinfer-${POWERINFER_GROUP_SIZE}")
    add_library(${LIB_TARGET} SHARED ${sources})
    target_include_directories(${LIB_TARGET} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/${INSTALL_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(${LIB_TARGET} PRIVATE ${POWERINFER_EXTRA_LIBS} ${COMMON_LIB_TARGET} ${CPU_LIB_TARGET} ${DISK_LIB_TARGET} ${CACHE_LIB_TARGET})
    target_link_libraries(${LIB_TARGET} PUBLIC ${MOE_SPARSE_PIPELINE_TARGET} ${FUSED_SPARSE_MOE_TARGET})


    # --- Install
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
            PowerInferConfigVersion.cmake
            VERSION ${PACKAGE_VERSION}
            COMPATIBILITY AnyNewerVersion  # 表示该函数库向下兼容
    )
    install(TARGETS "powerinfer-${POWERINFER_GROUP_SIZE}"
            EXPORT PowerInferTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT lib
            RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT bin
            PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(EXPORT PowerInferTargets
            FILE PowerInferTargets.cmake
            NAMESPACE PowerInfer::
            DESTINATION lib/cmake/PowerInfer
    )

    # --- Test

    if (POWERINFER_ENABLE_TEST)
        add_subdirectory(test)
    endif()
else()
    set(LIB_TARGET "powerinfer-${POWERINFER_GROUP_SIZE}")
    add_library(${LIB_TARGET} STATIC ${sources}
            include/powerinfer-az.h
            src/interface_az.cpp)
    target_include_directories(${LIB_TARGET} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/${INSTALL_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(${LIB_TARGET} PRIVATE ${POWERINFER_EXTRA_LIBS} ${COMMON_LIB_TARGET} ${CPU_LIB_TARGET} ${DISK_LIB_TARGET} ${CACHE_LIB_TARGET})
    target_link_libraries(${LIB_TARGET} PUBLIC ${MOE_SPARSE_PIPELINE_TARGET} ${FUSED_SPARSE_MOE_TARGET})
endif()
