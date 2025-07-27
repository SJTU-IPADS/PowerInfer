cmake_minimum_required(VERSION 3.11...3.31)

project(CLI11_Examples LANGUAGES CXX)

# Using CMake 3.11's ability to set imported interface targets
add_library(CLI11::CLI11 IMPORTED INTERFACE)
target_include_directories(CLI11::CLI11 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
target_compile_features(CLI11::CLI11 INTERFACE cxx_std_11)

# Add CTest
enable_testing()

# Quick function to add the base executable
function(add_cli_exe NAME)
  add_executable(${NAME} ${NAME}.cpp)
  target_link_libraries(${NAME} CLI11::CLI11)
endfunction()

add_cli_exe(simplest)
add_test(NAME simplest COMMAND simplest)

add_cli_exe(intro)
add_test(NAME intro COMMAND intro)
add_test(NAME intro_p COMMAND intro -p 5)

add_cli_exe(flags)
add_test(NAME flags COMMAND flags)
add_test(NAME flags_bip COMMAND flags -b -i -p)

add_cli_exe(geet)
add_test(NAME geet_add COMMAND geet add)
add_test(NAME geet_commit COMMAND geet commit -m "Test")
