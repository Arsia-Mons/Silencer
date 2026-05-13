cmake_minimum_required(VERSION 3.16)
project(clay_demo_dispatch)

# The `clay_demo` executable target is defined in the main silencer
# CMakeLists.txt — it shares the silencer source tree with the lobby
# screens filtered out and uses the SAME third-party dependencies
# (SDL3, zlib, minizip, etc.), so building it through silencer's
# build system avoids duplicating that configuration here.
#
# Running `cmake -B build -S clients/silencer && cmake --build build
# --target clay_demo` from the repo root produces the binary at
# `tools/clay-demo/build/clay_demo` (set via the target's
# RUNTIME_OUTPUT_DIRECTORY property).
#
# This file exists so consumers can `cmake -S tools/clay-demo` and have
# it auto-discover the silencer build. It forwards by adding silencer
# as a subdirectory (the silencer cmake declares both targets).

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../clients/silencer"
                 silencer_build)
