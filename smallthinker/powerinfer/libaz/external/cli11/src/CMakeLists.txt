if(CLI11_PRECOMPILED)
  # Create static lib
  file(GLOB CLI11_precompile_sources "${PROJECT_SOURCE_DIR}/src/*.cpp")
  add_library(CLI11 STATIC ${CLI11_headers} ${CLI11_library_headers} ${CLI11_impl_headers}
                           ${CLI11_precompile_sources})
  target_compile_definitions(CLI11 PUBLIC -DCLI11_COMPILE)

  set(PUBLIC_OR_INTERFACE PUBLIC)
else()
  add_library(CLI11 INTERFACE)
  if(CMAKE_VERSION VERSION_GREATER 3.19)
    # This is only useful for visual studio and other IDE builds
    target_sources(CLI11 PRIVATE ${CLI11_headers} ${CLI11_library_headers} ${CLI11_impl_headers})
  endif()

  set(PUBLIC_OR_INTERFACE INTERFACE)
endif()

# Allow IDE's to group targets into folders
add_library(CLI11::CLI11 ALIAS CLI11) # for add_subdirectory calls

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(SYSTEM_INCL "")
else()
  # If this project is included from somewhere else, we mark our headers as system headers to avoid
  # the compiler emitting any warnings about them
  set(SYSTEM_INCL "SYSTEM")
endif()

# Duplicated because CMake adds the current source dir if you don't.
target_include_directories(
  CLI11 ${SYSTEM_INCL} ${PUBLIC_OR_INTERFACE} $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

if(CMAKE_CXX_STANDARD LESS 14)
  target_compile_features(CLI11 INTERFACE cxx_std_11)
endif()

if(CLI11_INSTALL)

  # Make an export target
  install(TARGETS CLI11 EXPORT CLI11Targets)
  if(NOT CLI11_SINGLE_FILE)
    install(FILES ${CLI11_headers} ${CLI11_library_headers}
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/CLI")
    if(NOT CLI11_PRECOMPILED)
      install(FILES ${CLI11_impl_headers} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/CLI/impl")
    endif()
  endif()
endif()
