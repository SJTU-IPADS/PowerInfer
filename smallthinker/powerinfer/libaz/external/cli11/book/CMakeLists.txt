set(book_sources README.md SUMMARY.md)

file(
  GLOB book_chapters
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  chapters/*.md)
add_custom_target(cli_book SOURCES ${book_sources} ${book_chapters})
