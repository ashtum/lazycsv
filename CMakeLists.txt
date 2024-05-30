cmake_minimum_required(VERSION 3.8)

project(
  lazycsv
  VERSION 1.2.0
  LANGUAGES CXX)

include(GNUInstallDirs)

add_library(lazycsv INTERFACE)
target_include_directories(lazycsv INTERFACE include/)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wfatal-errors -Wextra -Wnon-virtual-dtor -pedantic)

add_subdirectory(test)

# Install headers
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp")
