cmake_minimum_required(VERSION 3.16)
project(silencer_design CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Allow Homebrew-installed SDL3
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")

find_package(SDL3 CONFIG REQUIRED)

add_executable(silencer_design
    src/main.cpp
)

if (TARGET SDL3::SDL3)
    target_link_libraries(silencer_design PRIVATE SDL3::SDL3)
elseif (TARGET SDL3::SDL3-shared)
    target_link_libraries(silencer_design PRIVATE SDL3::SDL3-shared)
endif()

if (APPLE)
    target_compile_options(silencer_design PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
