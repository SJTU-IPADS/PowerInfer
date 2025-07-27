cmake_minimum_required(VERSION 3.10...3.31)
# Note: this is a header only library. If you have an older CMake than 3.5,
# just add the CLI11/include directory and that's all you need to do.

set(VERSION_REGEX "#define CLI11_VERSION[ \t]+\"(.+)\"")

# Read in the line containing the version
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/CLI/Version.hpp" VERSION_STRING
     REGEX ${VERSION_REGEX})

# Pick out just the version
string(REGEX REPLACE ${VERSION_REGEX} "\\1" VERSION_STRING "${VERSION_STRING}")

# Add the project
project(
  CLI11
  LANGUAGES CXX
  VERSION ${VERSION_STRING})

list(APPEND CMAKE_MODULE_PATH "${CLI11_SOURCE_DIR}/cmake")

# Print the version number of CMake if this is the main project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  message(STATUS "CMake ${CMAKE_VERSION}")

  find_package(Doxygen)

  if(NOT Doxygen_FOUND)
    message(STATUS "Doxygen not found, building docs has been disabled")
  endif()

  include(CTest)
else()
  if(NOT DEFINED BUILD_TESTING)
    set(BUILD_TESTING OFF)
  endif()
endif()

include(CMakeDependentOption)
include(GNUInstallDirs)

if(NOT CMAKE_VERSION VERSION_LESS 3.11)
  include(FetchContent)
endif()

list(APPEND force-libcxx "CMAKE_CXX_COMPILER_ID STREQUAL \"Clang\"")
list(APPEND force-libcxx "CMAKE_SYSTEM_NAME STREQUAL \"Linux\"")
list(APPEND force-libcxx "CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME")

list(APPEND build-docs "CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME")
list(APPEND build-docs "NOT CMAKE_VERSION VERSION_LESS 3.11")
list(APPEND build-docs "Doxygen_FOUND")

# Necessary to support paths with spaces, see #457
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs")
  set(docs_EXIST TRUE)
else()
  set(docs_EXIST FALSE)
endif()
list(APPEND build-docs "docs_EXIST")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
  set(examples_EXIST TRUE)
else()
  set(examples_EXIST FALSE)
endif()

option(CLI11_WARNINGS_AS_ERRORS "Turn all warnings into errors (for CI)")
option(CLI11_SINGLE_FILE "Generate a single header file")
option(CLI11_PRECOMPILED "Generate a precompiled static library instead of a header-only" OFF)
cmake_dependent_option(CLI11_SANITIZERS "Download the sanitizers CMake config" OFF
                       "NOT CMAKE_VERSION VERSION_LESS 3.13" OFF)

cmake_dependent_option(CLI11_BUILD_DOCS "Build CLI11 documentation" ON "${build-docs}" OFF)

cmake_dependent_option(CLI11_BUILD_TESTS "Build CLI11 tests" ON
                       "BUILD_TESTING;CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME" OFF)

cmake_dependent_option(CLI11_BUILD_EXAMPLES "Build CLI11 examples" ON
                       "CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME;${examples_EXIST}" OFF)

cmake_dependent_option(CLI11_BUILD_EXAMPLES_JSON "Build CLI11 json example" OFF
                       "CLI11_BUILD_EXAMPLES;NOT CMAKE_VERSION VERSION_LESS 3.11" OFF)

cmake_dependent_option(CLI11_SINGLE_FILE_TESTS "Duplicate all the tests for a single file build"
                       OFF "BUILD_TESTING;CLI11_SINGLE_FILE" OFF)

cmake_dependent_option(CLI11_INSTALL "Install the CLI11 folder to include during install process"
                       ON "CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME" OFF)

cmake_dependent_option(
  CLI11_FORCE_LIBCXX "Force clang to use libc++ instead of libstdc++ (Linux only)" OFF
  "${force-libcxx}" OFF)

cmake_dependent_option(
  CLI11_CUDA_TESTS "Build the tests with NVCC to check for warnings there - requires CMake 3.9+"
  OFF "CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME" OFF)

if(CLI11_PRECOMPILED AND CLI11_SINGLE_FILE)
  # Sanity check
  message(FATAL_ERROR "CLI11_PRECOMPILE and CLI11_SINGLE_FILE are mutually exclusive")
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 11)
endif()

if(NOT DEFINED CMAKE_CXX_EXTENSIONS)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

if(NOT DEFINED CMAKE_CXX_STANDARD_REQUIRED)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Allow IDE's to group targets into folders
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

include(CLI11Warnings)

# Sources

set(CLI11_headerLoc "${PROJECT_SOURCE_DIR}/include/CLI")

set(CLI11_headers
    ${CLI11_headerLoc}/App.hpp
    ${CLI11_headerLoc}/Config.hpp
    ${CLI11_headerLoc}/ConfigFwd.hpp
    ${CLI11_headerLoc}/Error.hpp
    ${CLI11_headerLoc}/Formatter.hpp
    ${CLI11_headerLoc}/FormatterFwd.hpp
    ${CLI11_headerLoc}/Macros.hpp
    ${CLI11_headerLoc}/Option.hpp
    ${CLI11_headerLoc}/Split.hpp
    ${CLI11_headerLoc}/StringTools.hpp
    ${CLI11_headerLoc}/TypeTools.hpp
    ${CLI11_headerLoc}/Validators.hpp
    ${CLI11_headerLoc}/Version.hpp
    ${CLI11_headerLoc}/Encoding.hpp
    ${CLI11_headerLoc}/Argv.hpp)

set(CLI11_impl_headers
    ${CLI11_headerLoc}/impl/App_inl.hpp
    ${CLI11_headerLoc}/impl/Config_inl.hpp
    ${CLI11_headerLoc}/impl/Formatter_inl.hpp
    ${CLI11_headerLoc}/impl/Option_inl.hpp
    ${CLI11_headerLoc}/impl/Split_inl.hpp
    ${CLI11_headerLoc}/impl/StringTools_inl.hpp
    ${CLI11_headerLoc}/impl/Validators_inl.hpp
    ${CLI11_headerLoc}/impl/Encoding_inl.hpp
    ${CLI11_headerLoc}/impl/Argv_inl.hpp)

set(CLI11_library_headers ${CLI11_headerLoc}/CLI.hpp ${CLI11_headerLoc}/Timer.hpp)

# build the fuzzing example or fuzz entry point
add_subdirectory(fuzz)

add_subdirectory(src)
add_subdirectory(single-include)

# Allow tests to be run on CUDA
if(CLI11_CUDA_TESTS)
  enable_language(CUDA)

  # Print out warning and error numbers
  set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -Xcudafe --display_error_number")
endif()

# This folder should be installed
if(CLI11_INSTALL)

  # Use find_package on the installed package
  # Since we have no custom code, we can directly write this
  # to Config.cmake (otherwise we'd have a custom config and would
  # import Targets.cmake

  # Add the version in a CMake readable way
  configure_file("cmake/CLI11ConfigVersion.cmake.in" "CLI11ConfigVersion.cmake" @ONLY)

  # Make version available in the install
  install(FILES "${PROJECT_BINARY_DIR}/CLI11ConfigVersion.cmake"
          DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/CLI11")

  # Install the export target as a file
  install(
    EXPORT CLI11Targets
    FILE CLI11Config.cmake
    NAMESPACE CLI11::
    DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/CLI11")

  # Use find_package on the installed package
  export(
    TARGETS CLI11
    NAMESPACE CLI11::
    FILE CLI11Targets.cmake)

  include(cmake/CLI11GeneratePkgConfig.cmake)

  # Register in the user cmake package registry
  export(PACKAGE CLI11)
endif()

if(CLI11_BUILD_TESTS)
  include(CTest)
  add_subdirectory(tests)
endif()

if(CLI11_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(CLI11_BUILD_DOCS)
  add_subdirectory(docs)
endif()

# From a build system, this might not be included.
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/book")
  add_subdirectory(book)
endif()

# Packaging support
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(CPACK_PACKAGE_VENDOR "github.com/CLIUtils/CLI11")
  set(CPACK_PACKAGE_CONTACT "https://${CPACK_PACKAGE_VENDOR}")
  set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR}) # Automatic in CMake 3.12+
  set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR}) # Automatic in CMake 3.12+
  set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH}) # Automatic in CMake 3.12+
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Command line parser with simple and intuitive interface")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
  set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/CLI11.CPack.Description.txt")
  set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")

  # CPack collects *everything* except what's listed here.
  set(CPACK_SOURCE_IGNORE_FILES
      /.git
      /dist
      /.*build.*
      /\\\\.DS_Store
      /.*\\\\.egg-info
      /var
      /azure-pipelines.yml
      /.ci
      /docs
      /examples
      /test_package
      /book
      /.travis.yml
      .swp
      /.all-contributorsrc
      /.pre-commit.*yaml)

  set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "all")
  set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
  set(CPACK_DEBIAN_PACKAGE_NAME "libcli11-dev")

  include(CPack)
endif()
