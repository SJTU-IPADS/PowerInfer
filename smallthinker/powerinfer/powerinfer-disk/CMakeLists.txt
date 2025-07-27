cmake_minimum_required(VERSION 3.16)

project(powerinfer-disk C CXX)

message(STATUS "Configuring submodule: ${PROJECT_NAME}")


set(POWERINFER_EXTRA_LIBS "")


if (UNIX)
    list(APPEND POWERINFER_EXTRA_LIBS aio)

    if (NOT ANDROID)
        list(APPEND POWERINFER_EXTRA_LIBS rt)
    endif()

    find_library(LIBURING_LIBRARY_PATH uring)
    if(LIBURING_LIBRARY_PATH)
        message(STATUS "Found liburing, it will be added for static linking.")
        list(APPEND POWERINFER_EXTRA_LIBS uring)
    else()
        message(WARNING "liburing (liburing.a) was not found. It will not be linked. This may be expected on older systems.")
    endif()
endif()

FILE(GLOB_RECURSE sources CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)


add_library(${PROJECT_NAME} STATIC ${sources})


target_include_directories(${PROJECT_NAME} PUBLIC 
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)


target_link_libraries(${PROJECT_NAME} PUBLIC
    powerinfer-perf
    powerinfer-common
    
    "-Wl,-Bstatic" 
    
    ${POWERINFER_EXTRA_LIBS} 
    
    "-Wl,-Bdynamic"
)

message(STATUS "Configuration of ${PROJECT_NAME} finished.")