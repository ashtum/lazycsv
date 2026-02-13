add_executable(main main.cpp)

target_link_libraries(main lazycsv)

if(MSVC)
  add_compile_options(/W4 /WX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wfatal-errors -Wextra -Wnon-virtual-dtor -pedantic -Werror)
endif()

target_include_directories(main SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/doctest)

add_custom_command(
  TARGET main
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/inputs
          ${CMAKE_CURRENT_BINARY_DIR}/inputs)

add_test(main main)
