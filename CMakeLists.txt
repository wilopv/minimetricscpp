cmake_minimum_required(VERSION 3.18)
project(minimetricscpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(minimetrics main.cpp)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(minimetrics PRIVATE -Wall -Wextra -Wpedantic)
endif()
