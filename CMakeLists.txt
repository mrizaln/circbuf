cmake_minimum_required(VERSION 3.12)

project(
  circular-buffer
  VERSION 0.0.0
  LANGUAGES CXX
  DESCRIPTION "A simple C++ circular buffer written in C++20")

add_library(circular-buffer INTERFACE)
target_include_directories(circular-buffer INTERFACE include)
target_compile_features(circular-buffer INTERFACE cxx_std_20)
set_target_properties(circular-buffer PROPERTIES CXX_EXTENSIONS OFF)
