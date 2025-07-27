cmake_minimum_required(VERSION 3.16)

project(powerinfer-cpu C CXX)
message("Add submodule: ${PROJECT_NAME}")

FILE(GLOB_RECURSE sources             ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_library(${PROJECT_NAME} STATIC ${sources} ${moe_pipeline_sources})
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PUBLIC powerinfer-common powerinfer-perf powerinfer-disk  "-Wl,-Bstatic"  uring "-Wl,-Bdynamic" az)
