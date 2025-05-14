cmake_minimum_required(VERSION 3.8...3.20)

project(
  lazycsv
  VERSION 1.2.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

include(GNUInstallDirs)

add_library(lazycsv INTERFACE)
target_include_directories(lazycsv INTERFACE include/)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(/W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wfatal-errors -Wextra -Wnon-virtual-dtor -pedantic)
endif()

# Install headers
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp")

if (${BUILD_TESTING})
  include(CTest)
  add_subdirectory(test)
endif ()
