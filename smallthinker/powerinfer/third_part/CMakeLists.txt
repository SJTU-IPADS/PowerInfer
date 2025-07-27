# Perfetto
if (POWERINFER_WITH_TRACING)
    find_package(Threads)
    add_library(perfetto STATIC ${CMAKE_CURRENT_SOURCE_DIR}/perfetto/sdk/perfetto.cc)
    target_include_directories(perfetto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/perfetto/sdk)

    if (WIN32)
        # The perfetto library contains many symbols, so it needs the big object
        # format.
        target_compile_options(perfetto PRIVATE "/bigobj")
        # Disable legacy features in windows.h.
        target_compile_definitions(perfetto PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        # On Windows we should link to WinSock2.
        target_link_libraries(perfetto PRIVATE ws2_32)
    endif (WIN32)

    # Enable standards-compliant mode when using the Visual Studio compiler.
    if (MSVC)
        target_compile_options(perfetto PRIVATE "/permissive-")
    endif (MSVC)
endif()

if (POWERINFER_ENABLE_TEST)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/googletest)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/benchmark)
endif()