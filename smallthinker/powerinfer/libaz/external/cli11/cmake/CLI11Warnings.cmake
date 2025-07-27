# Special target that adds warnings. Is not exported.
add_library(CLI11_warnings INTERFACE)

set(unix-warnings -Wall -Wextra -pedantic -Wshadow -Wsign-conversion -Wswitch-enum)

# Clang warnings
# -Wfloat-equal could be added with Catch::literals and _a usage
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  list(
    APPEND
    unix-warnings
    -Wcast-align
    -Wimplicit-atomic-properties
    -Wmissing-declarations
    -Woverlength-strings
    -Wshadow
    -Wstrict-selector-match
    -Wundeclared-selector)
  # -Wunreachable-code Doesn't work on Clang 3.4
endif()

# Buggy in GCC 4.8
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
  list(APPEND unix-warnings -Weffc++)
endif()

target_compile_options(
  CLI11_warnings
  INTERFACE $<$<BOOL:${CLI11_FORCE_LIBCXX}>:-stdlib=libc++>
            $<$<CXX_COMPILER_ID:MSVC>:/W4
            $<$<BOOL:${CLI11_WARNINGS_AS_ERRORS}>:/WX>>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:${unix-warnings}
            $<$<BOOL:${CLI11_WARNINGS_AS_ERRORS}>:-Werror>>)

if(NOT CMAKE_VERSION VERSION_LESS 3.13)
  target_link_options(CLI11_warnings INTERFACE $<$<BOOL:${CLI11_FORCE_LIBCXX}>:-stdlib=libc++>)
endif()
