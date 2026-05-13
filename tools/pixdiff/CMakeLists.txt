cmake_minimum_required(VERSION 3.16)
project(pixdiff CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(pixdiff pixdiff.cpp)
target_include_directories(pixdiff PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(gen_fixtures gen_fixtures.cpp)
target_include_directories(gen_fixtures PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
