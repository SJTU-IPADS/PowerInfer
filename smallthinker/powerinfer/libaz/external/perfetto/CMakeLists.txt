add_library(perfetto STATIC
    "perfetto.cc"
)

target_include_directories(perfetto INTERFACE .)
if (NOT ANDROID)
    target_link_libraries(perfetto PRIVATE pthread)
endif()


if (ANDROID)
    target_link_libraries(perfetto PRIVATE -llog)
endif()
