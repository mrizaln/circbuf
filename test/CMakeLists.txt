cmake_minimum_required(VERSION 3.16)
project(circbuf-test VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# set(CMAKE_COLOR_DIAGNOSTICS ON) # You might want to enable this (CMake 3.24+)

find_package(ut REQUIRED)
find_package(fmt REQUIRED)

add_subdirectory(lib/circbuf) # emits circbuf target

function(make_test NAME)
  add_executable(${NAME} ${NAME}.cpp)
  target_link_libraries(${NAME} PRIVATE fmt::fmt Boost::ut circbuf)
  target_compile_features(${NAME} PRIVATE cxx_std_23)
  set_target_properties(${NAME} PROPERTIES CXX_EXTENSIONS OFF)

  target_compile_options(${NAME} PRIVATE -Wall -Wextra -Wconversion)
  target_compile_options(${NAME} PRIVATE -fsanitize=address,leak,undefined)
  target_link_options(${NAME} PRIVATE -fsanitize=address,leak,undefined)

  add_test(NAME ${NAME} COMMAND $<TARGET_FILE:${NAME}>)

  add_custom_command(
    TARGET ${NAME}
    POST_BUILD
    COMMAND ${NAME})
endfunction()

enable_testing()
make_test(raw_buffer_test)
make_test(circular_buffer_test)
