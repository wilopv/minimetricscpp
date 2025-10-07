cmake_minimum_required(VERSION 3.18)
project(minimetricscpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# LINK: Boost::system
find_package(Boost REQUIRED COMPONENTS system)

# LINK: Threads::Threads
find_package(Threads REQUIRED)

add_executable(minimetrics main.cpp)

target_link_libraries(minimetrics
  PRIVATE
    Boost::system
    Threads::Threads # for std::thread
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(minimetrics PRIVATE -Wall -Wextra -Wpedantic)
endif()
