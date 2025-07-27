if(CLI11_SINGLE_FILE)
  # Single file test
  if(CMAKE_VERSION VERSION_LESS 3.12)
    find_package(PythonInterp REQUIRED)
    add_executable(Python::Interpreter IMPORTED)
    set_target_properties(Python::Interpreter PROPERTIES IMPORTED_LOCATION "${PYTHON_EXECUTABLE}"
                                                         VERSION "${PYTHON_VERSION_STRING}")
  else()
    find_package(
      Python
      COMPONENTS Interpreter
      REQUIRED)
  endif()

  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/single-include")
  add_custom_command(
    OUTPUT "${PROJECT_BINARY_DIR}/single-include/CLI11.hpp"
    COMMAND
      Python::Interpreter "${PROJECT_SOURCE_DIR}/scripts/MakeSingleHeader.py" ${CLI11_headers}
      ${CLI11_impl_headers} --main "${CMAKE_CURRENT_SOURCE_DIR}/CLI11.hpp.in" --output
      "${PROJECT_BINARY_DIR}/single-include/CLI11.hpp" --version "${CLI11_VERSION}"
    DEPENDS "${PROJECT_SOURCE_DIR}/include/CLI/CLI.hpp" CLI11.hpp.in ${CLI11_headers}
            ${CLI11_impl_headers})
  add_custom_target(CLI11-generate-single-file ALL
                    DEPENDS "${PROJECT_BINARY_DIR}/single-include/CLI11.hpp")
  set_property(TARGET CLI11-generate-single-file PROPERTY FOLDER "Scripts")
  if(CLI11_INSTALL)
    install(FILES "${PROJECT_BINARY_DIR}/single-include/CLI11.hpp"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  endif()
  add_library(CLI11_SINGLE INTERFACE)
  target_link_libraries(CLI11_SINGLE INTERFACE CLI11)
  add_dependencies(CLI11_SINGLE CLI11-generate-single-file)
  target_compile_definitions(CLI11_SINGLE INTERFACE -DCLI11_SINGLE_FILE)
  target_include_directories(
    CLI11_SINGLE INTERFACE $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/single-include/>
                           $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
endif()
