# Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
# under NSF AWARD 1414736 and by the respective contributors.
# All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause

if(CMAKE_CXX_STANDARD GREATER 16)
  if(CLI11_FUZZ_TARGET)

    add_executable(cli11_app_fuzzer cli11_app_fuzz.cpp fuzzApp.cpp fuzzApp.hpp)
    target_link_libraries(cli11_app_fuzzer PUBLIC CLI11)
    set_property(TARGET cli11_app_fuzzer PROPERTY FOLDER "Tests")

    add_executable(cli11_file_fuzzer cli11_file_fuzz.cpp fuzzApp.cpp fuzzApp.hpp)
    target_link_libraries(cli11_file_fuzzer PUBLIC CLI11)
    set_property(TARGET cli11_file_fuzzer PROPERTY FOLDER "Tests")

    if(NOT CLI11_FUZZ_ARTIFACT_PATH)
      set(CLI11_FUZZ_ARTIFACT_PATH ${PROJECT_BINARY_DIR}/fuzz)
    endif()

    if(NOT CLI11_FUZZ_TIME_APP)
      set(CLI11_FUZZ_TIME_APP 600)
    endif()
    if(NOT CLI11_FUZZ_TIME_FILE)
      set(CLI11_FUZZ_TIME_FILE 240)
    endif()
    add_custom_target(
      QUICK_CLI11_APP_FUZZ
      COMMAND ${CMAKE_COMMAND} -E make_directory corp
      COMMAND
        cli11_app_fuzzer corp -max_total_time=${CLI11_FUZZ_TIME_APP} -max_len=4096
        -dict=${CMAKE_CURRENT_SOURCE_DIR}/fuzz_dictionary1.txt
        -exact_artifact_path=${CLI11_FUZZ_ARTIFACT_PATH}/cli11_app_roundtrip_fail_artifact.txt)

    add_custom_target(
      QUICK_CLI11_FILE_FUZZ
      COMMAND ${CMAKE_COMMAND} -E make_directory corp
      COMMAND
        cli11_file_fuzzer corp -max_total_time=${CLI11_FUZZ_TIME_FILE} -max_len=2048
        -dict=${CMAKE_CURRENT_SOURCE_DIR}/fuzz_dictionary2.txt
        -exact_artifact_path=${CLI11_FUZZ_ARTIFACT_PATH}/cli11_file_fail_artifact.txt)

  else()
    if(CLI11_BUILD_EXAMPLES)
      add_executable(cli11Fuzz fuzzCommand.cpp fuzzApp.cpp fuzzApp.hpp)
      target_link_libraries(cli11Fuzz PUBLIC CLI11)
      set_property(TARGET cli11Fuzz PROPERTY FOLDER "Examples")
    endif()
  endif()
endif()
