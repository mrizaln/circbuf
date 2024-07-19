cmake_minimum_required(VERSION 3.12)

project(
  circbuf
  VERSION 0.0.0
  LANGUAGES CXX
  DESCRIPTION "A simple C++ circular buffer written in C++20")

add_library(circbuf INTERFACE)
target_include_directories(circbuf INTERFACE include)
target_compile_features(circbuf INTERFACE cxx_std_20)
set_target_properties(circbuf PROPERTIES CXX_EXTENSIONS OFF)
