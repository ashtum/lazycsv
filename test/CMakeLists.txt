add_executable(driver main.cpp)

target_link_libraries(driver lazycsv)

target_include_directories(driver SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/doctest)

add_custom_command(
  TARGET driver
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/inputs
          ${CMAKE_CURRENT_BINARY_DIR}/inputs)
