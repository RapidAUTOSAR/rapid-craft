cmake_minimum_required(VERSION 3.20)
project(rapid_craft LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(packages/common)
add_subdirectory(packages/analyzer)
add_subdirectory(packages/sud/indexer)
add_subdirectory(packages/sud/diagrams)