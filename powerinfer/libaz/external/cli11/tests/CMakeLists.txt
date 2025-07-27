if(CLI11_SANITIZERS AND ${CMAKE_VERSION} VERSION_GREATER "3.13.0")
  message(STATUS "Using arsenm/sanitizers-cmake")
  FetchContent_Declare(
    sanitizers
    GIT_REPOSITORY https://github.com/arsenm/sanitizers-cmake.git
    GIT_SHALLOW 1
    GIT_TAG 0573e2e)

  FetchContent_GetProperties(sanitizers)

  if(NOT sanitizers_POPULATED)
    FetchContent_Populate(sanitizers)
  endif()

  list(APPEND CMAKE_MODULE_PATH "${sanitizers_SOURCE_DIR}/cmake")

  find_package(Sanitizers)
  if(SANITIZE_ADDRESS)
    message(STATUS "You might want to use \"${ASan_WRAPPER}\" to run your program")
  endif()
else()
  macro(add_sanitizers)

  endmacro()
endif()

# Add boost to test boost::optional (currently explicitly requested)"
option(CLI11_BOOST "Turn on boost test (currently may fail with Boost 1.70)" OFF)
if(CLI11_BOOST)
  find_package(Boost 1.61 REQUIRED)
endif()
set(boost-optional-def $<$<BOOL:${Boost_FOUND}>:CLI11_BOOST_OPTIONAL>)

set(CLI11_TESTS
    HelpersTest
    ConfigFileTest
    OptionTypeTest
    SimpleTest
    AppTest
    SetTest
    TransformTest
    CreationTest
    SubcommandTest
    HelpTest
    FormatterTest
    NewParseTest
    OptionalTest
    DeprecatedTest
    StringParseTest
    ComplexTypeTest
    TrueFalseTest
    OptionGroupTest
    EncodingTest)

if(WIN32)
  list(APPEND CLI11_TESTS WindowsTest)
endif()

if(CMAKE_CXX_STANDARD GREATER 16)
  list(APPEND CLI11_TESTS FuzzFailTest)
endif()

if(Boost_FOUND)
  list(APPEND CLI11_TESTS BoostOptionTypeTest)
endif()

set(CLI11_MULTIONLY_TESTS TimerTest)

find_package(Catch2 CONFIG)

if(Catch2_FOUND)
  if(NOT TARGET Catch2::Catch2)
    message(FATAL_ERROR "Found Catch2 at ${Catch2_DIR} but targets are missing.")
  endif()
  message(STATUS "Found Catch2 ${Catch2_VERSION}")

  if(Catch2_VERSION VERSION_LESS 3)
    add_library(catch_main main.cpp catch.hpp)
    target_include_directories(catch_main PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(catch_main PUBLIC Catch2::Catch2)
  else()
    add_library(catch_main ALIAS Catch2::Catch2WithMain)
    target_compile_definitions(Catch2::Catch2WithMain INTERFACE -DCLI11_CATCH3)
  endif()
else()
  message(STATUS "Downloading Catch2")

  # FetchContent would be better, but requires newer CMake.
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/catch2")
  set(url https://github.com/philsquared/Catch/releases/download/v2.13.10/catch.hpp)
  file(
    DOWNLOAD ${url} "${CMAKE_CURRENT_BINARY_DIR}/catch2/catch.hpp"
    STATUS status
    EXPECTED_HASH SHA256=3725c0f0a75f376a5005dde31ead0feb8f7da7507644c201b814443de8355170)
  list(GET status 0 error)
  if(error)
    message(FATAL_ERROR "Could not download ${url}, and Catch2 not found on your system.")
  endif()
  add_library(catch_main main.cpp catch.hpp)
  target_include_directories(catch_main PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
                                               "${CMAKE_CURRENT_BINARY_DIR}")
endif()

# Add special target that copies the data directory for tests
file(
  GLOB_RECURSE DATA_FILES
  LIST_DIRECTORIES false
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/data/*" "${CMAKE_CURRENT_SOURCE_DIR}/tests/.gitkeep")

foreach(DATA_FILE IN LISTS DATA_FILES)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${DATA_FILE}"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${DATA_FILE}"
            "${CMAKE_CURRENT_BINARY_DIR}/${DATA_FILE}"
    MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/${DATA_FILE}"
    VERBATIM)
endforeach()
add_custom_target(cli11_test_data DEPENDS ${DATA_FILES})

# Make a shim if we are building single file tests
if(CLI11_SINGLE_FILE AND CLI11_INSTALL_PACKAGE_TESTS)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/test_single/CLI/CLI.hpp" "#include <CLI11.hpp>")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/test_single/CLI/CLI.hpp" DESTINATION include/CLI)
endif()

# Build dependent applications which are launched from test code
set(CLI11_DEPENDENT_APPLICATIONS ensure_utf8 ensure_utf8_twice)

foreach(APP IN LISTS CLI11_DEPENDENT_APPLICATIONS)
  add_executable(${APP} applications/${APP}.cpp)
  target_include_directories(${APP} PRIVATE ${CMAKE_SOURCE_DIR}/include)
endforeach()

function(add_dependent_application_definitions TARGET)
  foreach(APP IN LISTS CLI11_DEPENDENT_APPLICATIONS)
    string(TOUPPER ${APP} APP_UPPERCASE)
    target_compile_definitions(${TARGET}
                               PRIVATE CLI11_${APP_UPPERCASE}_EXE="$<TARGET_FILE:${APP}>")
  endforeach()
endfunction()

# Target must already exist
macro(add_catch_test TESTNAME)
  target_link_libraries(${TESTNAME} PUBLIC catch_main)
  add_dependencies(${TESTNAME} cli11_test_data)

  add_test(${TESTNAME} ${TESTNAME})
  set_target_properties(${TESTNAME} PROPERTIES FOLDER "Tests")
  if(CLI11_FORCE_LIBCXX)
    set_property(
      TARGET ${T}
      APPEND_STRING
      PROPERTY LINK_FLAGS -stdlib=libc++)
  endif()
endmacro()

foreach(T IN LISTS CLI11_TESTS)
  if(CLI11_CUDA_TESTS)
    set_property(SOURCE ${T}.cpp PROPERTY LANGUAGE CUDA)
  endif()
  add_executable(${T} ${T}.cpp)

  add_dependent_application_definitions(${T})
  add_sanitizers(${T})
  if(NOT CLI11_CUDA_TESTS)
    target_link_libraries(${T} PRIVATE CLI11_warnings)
  endif()
  target_link_libraries(${T} PRIVATE CLI11)
  add_catch_test(${T})

  if(CLI11_SINGLE_FILE AND CLI11_SINGLE_FILE_TESTS)
    add_executable(${T}_Single ${T}.cpp)
    add_dependent_application_definitions(${T}_Single)
    target_link_libraries(${T}_Single PRIVATE CLI11_SINGLE)
    add_catch_test(${T}_Single)
    set_property(TARGET ${T}_Single PROPERTY FOLDER "Tests Single File")
  endif()
endforeach()

foreach(T IN LISTS CLI11_MULTIONLY_TESTS)
  add_executable(${T} ${T}.cpp)
  add_dependent_application_definitions(${T})
  add_sanitizers(${T})
  target_link_libraries(${T} PUBLIC CLI11)
  add_catch_test(${T})
endforeach()

if(CMAKE_CXX_STANDARD GREATER 16)
  set(TEST_FILE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(FuzzFailTest PUBLIC -DTEST_FILE_FOLDER="${TEST_FILE_FOLDER}")
  target_sources(FuzzFailTest PUBLIC ${PROJECT_SOURCE_DIR}/fuzz/fuzzApp.cpp)
  if(CLI11_SINGLE_FILE AND CLI11_SINGLE_FILE_TESTS)
    target_compile_definitions(FuzzFailTest_Single PUBLIC -DTEST_FILE_FOLDER="${TEST_FILE_FOLDER}")
    target_sources(FuzzFailTest_Single PUBLIC ${PROJECT_SOURCE_DIR}/fuzz/fuzzApp.cpp)
  endif()

  # Some platforms need to link to atomic
  file(WRITE "${CMAKE_BINARY_DIR}/test_atomic.cpp"
       "#include <atomic>\n" "int main() { std::atomic<int64_t> i(0); i++; return 0; }\n")
  try_compile(ATOMIC_BUILD_SUCCEEDED "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/test_atomic.cpp")
  if(NOT ATOMIC_BUILD_SUCCEEDED)
    target_link_libraries(FuzzFailTest PRIVATE atomic)
    if(CLI11_SINGLE_FILE AND CLI11_SINGLE_FILE_TESTS)
      target_link_libraries(FuzzFailTestSingle PRIVATE atomic)
    endif()
  endif()
  file(REMOVE ${CMAKE_BINARY_DIR}/test_atomic.cpp)
endif()

# Add -Wno-deprecated-declarations to DeprecatedTest
set(no-deprecated-declarations $<$<CXX_COMPILER_ID:MSVC>:/wd4996>
                               $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-deprecated-declarations>)
target_compile_options(DeprecatedTest PRIVATE ${no-deprecated-declarations})
if(TARGET DeprecatedTest_Single)
  target_compile_options(DeprecatedTest_Single PRIVATE ${no-deprecated-declarations})
endif()

# Link test (build error if inlines missing)
add_library(link_test_1 link_test_1.cpp)
target_link_libraries(link_test_1 PUBLIC CLI11)
set_target_properties(link_test_1 PROPERTIES FOLDER "Tests")
add_executable(link_test_2 link_test_2.cpp)
target_link_libraries(link_test_2 PUBLIC CLI11 link_test_1)
add_catch_test(link_test_2)
if(CLI11_FORCE_LIBCXX)
  set_property(
    TARGET link_test_1
    APPEND_STRING
    PROPERTY LINK_FLAGS -stdlib=libc++)
  set_property(
    TARGET link_test_2
    APPEND_STRING
    PROPERTY LINK_FLAGS -stdlib=libc++)
endif()

# Add informational printout
add_executable(informational informational.cpp)
target_link_libraries(informational PUBLIC CLI11)
if(CLI11_FORCE_LIBCXX)
  set_property(
    TARGET informational
    APPEND_STRING
    PROPERTY LINK_FLAGS -stdlib=libc++)
endif()

# Force this to be in a standard location so CTest can find it
set_target_properties(
  informational
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
             RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
             RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
             RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
             RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}")

# Adding this printout to CTest
file(WRITE "${PROJECT_BINARY_DIR}/CTestCustom.cmake"
     "set(CTEST_CUSTOM_PRE_TEST \"${CMAKE_BINARY_DIR}/informational\")")

target_compile_definitions(informational PRIVATE ${boost-optional-def})
target_compile_definitions(OptionalTest PRIVATE ${boost-optional-def})

if(TARGET Boost::boost)
  message(STATUS "including boost target")
  target_link_libraries(informational PRIVATE Boost::boost)
  target_link_libraries(OptionalTest PRIVATE Boost::boost)
  target_link_libraries(BoostOptionTypeTest PRIVATE Boost::boost)
  if(CLI11_SINGLE_FILE AND CLI11_SINGLE_FILE_TESTS)
    target_link_libraries(OptionalTest_Single PRIVATE Boost::boost)
    target_link_libraries(BoostOptionTypeTest_Single PRIVATE Boost::boost)
  endif()
  message(STATUS "Boost libs=${Boost_INCLUDE_DIRS}")
elseif(BOOST_FOUND)
  message(STATUS "no boost target")
  target_include_directories(informational PRIVATE ${Boost_INCLUDE_DIRS})
  target_include_directories(OptionalTest PRIVATE ${Boost_INCLUDE_DIRS})
  target_include_directories(BoostOptionTypeTest PRIVATE ${Boost_INCLUDE_DIRS})
  if(CLI11_SINGLE_FILE AND CLI11_SINGLE_FILE_TESTS)
    target_include_directories(OptionalTest_Single PRIVATE ${Boost_INCLUDE_DIRS})
    target_include_directories(BoostOptionTypeTest_Single PRIVATE ${Boost_INCLUDE_DIRS})
  endif()
  message(STATUS "Boost libs=${Boost_INCLUDE_DIRS}")
else()
  message(STATUS "Boost not found, not adding boost tests")
endif()

if(CMAKE_BUILD_TYPE STREQUAL Coverage)
  include(CodeCoverage)
  setup_target_for_coverage_lcov(
    NAME
    CLI11_coverage
    EXECUTABLE
    ctest
    DEPENDENCIES
    ${CLI11_TESTS}
    ${CLI11_MULTIONLY_TESTS})
endif()

set(CLI11_PACKAGE_SEARCH_LOC "")

# tests of the cmake package and pkg-config package
if(CLI11_INSTALL_PACKAGE_TESTS)
  if(NOT MSVC)
    set(package_test_command --test-command "${CMAKE_CTEST_COMMAND}")
  else() # don't try to run the tests on MSVC since that would require copying the dll's and doing
    # some other setup that isn't that important to run on all OS
    set(package_test_command)
  endif()

  if(CMAKE_BUILD_TYPE)
    set(CLI11_PACKAGE_TEST_BUILD_TYPE ${CMAKE_BUILD_TYPE})
  else()
    set(CLI11_PACKAGE_TEST_BUILD_TYPE Release)
  endif()

  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/find_package_tests")

  if(MSVC AND ${CMAKE_VERSION} VERSION_GREATER 3.12.9)
    # Tests for other CMake projects including and using CLI11 using find_package
    add_test(
      NAME find-package-testsA
      COMMAND
        ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" -A "${CMAKE_GENERATOR_PLATFORM}"
        "-DCLI11_DIR=${CMAKE_INSTALL_PREFIX}" "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}"
        "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        "${CMAKE_CURRENT_SOURCE_DIR}/find_package_tests"
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/find_package_tests")
  else()
    add_test(
      NAME find-package-testsA
      COMMAND
        ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" "-DCLI11_DIR=${CMAKE_INSTALL_PREFIX}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}" "-DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}"
        "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        "${CMAKE_CURRENT_SOURCE_DIR}/find_package_tests"
      WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/find_package_tests")
  endif()

  add_test(NAME find-package-testsB
           COMMAND ${CMAKE_COMMAND} --build "${CMAKE_CURRENT_BINARY_DIR}/find_package_tests"
                   --config ${CLI11_PACKAGE_TEST_BUILD_TYPE})

  add_test(
    NAME find-package-testsC
    COMMAND ${CMAKE_CTEST_COMMAND} -C ${CLI11_PACKAGE_TEST_BUILD_TYPE}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/find_package_tests")
  set_property(TEST find-package-testsA PROPERTY LABELS Packaging)
  set_property(TEST find-package-testsB PROPERTY LABELS Packaging)
  set_property(TEST find-package-testsB PROPERTY DEPENDS find-package-testsA)
  set_property(TEST find-package-testsC PROPERTY LABELS Packaging)
  set_property(TEST find-package-testsC PROPERTY DEPENDS find-package-testsB)

  if(NOT MSVC)
    # Tests for other CMake projects using the package_config files
    add_test(
      package-config-tests
      ${CMAKE_CTEST_COMMAND}
      -C
      --build-and-test
      "${CMAKE_CURRENT_SOURCE_DIR}/package_config_tests"
      "${CMAKE_CURRENT_BINARY_DIR}/package_config_tests"
      --build-generator
      "${CMAKE_GENERATOR}"
      --build-generator-platform
      "${CMAKE_GENERATOR_PLATFORM}"
      --build-options
      "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
      "-DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}"
      "-DCLI11_DIR=${CMAKE_INSTALL_PREFIX}"
      ${package_test_command})
    set_property(TEST package-config-tests PROPERTY LABELS Packaging)
  endif()
endif()
