cmake_minimum_required(VERSION 3.16)

project(powerinfer-common C CXX)
message("Add submodule: ${PROJECT_NAME}")

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(${PROJECT_NAME} INTERFACE GROUP_SIZE=${POWERINFER_GROUP_SIZE})
