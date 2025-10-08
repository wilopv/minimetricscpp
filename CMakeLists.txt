cmake_minimum_required(VERSION 3.18)
project(minimetricscpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# LINK: Boost::system
find_package(Boost REQUIRED COMPONENTS system)

# LINK: Threads::Threads
find_package(Threads REQUIRED)

add_library(minimetrics_core
  src/metrics_collector.cpp
  src/http_server.cpp
)

target_include_directories(minimetrics_core PUBLIC src)

target_link_libraries(minimetrics_core
  PUBLIC
    Boost::system
    Threads::Threads # for std::thread
)

add_executable(minimetrics main.cpp)

target_link_libraries(minimetrics
  PRIVATE
    Boost::system
    Threads::Threads # for std::thread
)
target_link_libraries(minimetrics PRIVATE minimetrics_core)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(minimetrics PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(minimetrics_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
