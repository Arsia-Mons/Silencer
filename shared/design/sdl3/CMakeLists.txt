cmake_minimum_required(VERSION 3.20)
project(silencer_design CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Homebrew SDL3 on macOS
if(APPLE AND NOT DEFINED CMAKE_PREFIX_PATH)
  list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

find_package(SDL3 CONFIG REQUIRED)

add_executable(silencer_design
  src/main.cpp
  src/palette.cpp
  src/sprite.cpp
  src/font.cpp
  src/widgets/overlay.cpp
  src/widgets/button.cpp
  src/widgets/interface.cpp
  src/screens/main_menu.cpp
)

target_include_directories(silencer_design PRIVATE src)

if(TARGET SDL3::SDL3)
  target_link_libraries(silencer_design PRIVATE SDL3::SDL3)
elseif(TARGET SDL3::SDL3-shared)
  target_link_libraries(silencer_design PRIVATE SDL3::SDL3-shared)
else()
  message(FATAL_ERROR "Could not find SDL3 imported target")
endif()

target_compile_options(silencer_design PRIVATE -Wall -Wextra -Wno-unused-parameter)
