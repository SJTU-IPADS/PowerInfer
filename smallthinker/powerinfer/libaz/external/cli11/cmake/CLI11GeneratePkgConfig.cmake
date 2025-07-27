if(CLI11_PRECOMPILED)
  configure_file("cmake/CLI11precompiled.pc.in" "CLI11.pc" @ONLY)
else()
  configure_file("cmake/CLI11.pc.in" "CLI11.pc" @ONLY)
endif()

install(FILES "${PROJECT_BINARY_DIR}/CLI11.pc" DESTINATION "${CMAKE_INSTALL_DATADIR}/pkgconfig")
