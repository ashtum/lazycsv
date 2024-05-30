add_executable(main main.cpp)

target_link_libraries(main lazycsv)

target_include_directories(main SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/doctest)

add_custom_command(
  TARGET main
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/inputs
          ${CMAKE_CURRENT_BINARY_DIR}/inputs)

add_test(main main)
