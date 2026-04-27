cmake_minimum_required(VERSION 3.20)
project(silencer_design CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED)

file(GLOB SILENCER_DESIGN_COMPONENT_SRCS CONFIGURE_DEPENDS src/components/*.cpp)
file(GLOB SILENCER_DESIGN_SCREEN_SRCS CONFIGURE_DEPENDS src/screens/*.cpp)

add_executable(silencer_design
  src/main.cpp
  src/palette.cpp
  src/sprite.cpp
  src/font.cpp
  src/dump_runner.cpp
  ${SILENCER_DESIGN_COMPONENT_SRCS}
  ${SILENCER_DESIGN_SCREEN_SRCS}
)

target_include_directories(silencer_design PRIVATE src)
target_link_libraries(silencer_design PRIVATE SDL3::SDL3)

if(APPLE)
  target_compile_options(silencer_design PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
