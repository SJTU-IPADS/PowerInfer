set(DOXYGEN_EXTRACT_ALL YES)
set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
set(PROJECT_BRIEF "C++11 Command Line Interface Parser")

file(
  GLOB DOC_LIST
  RELATIVE "${PROJECT_SOURCE_DIR}/include"
  "${PROJECT_SOURCE_DIR}/include/CLI/*.hpp")

doxygen_add_docs(docs ${DOC_LIST} "${CMAKE_CURRENT_SOURCE_DIR}/mainpage.md"
                 WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/include")
