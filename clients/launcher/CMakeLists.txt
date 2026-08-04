cmake_minimum_required(VERSION 3.16)
project(SilencerLauncherCppx CXX)

# The launcher reuses the game client's cppx UI engine + updater primitives by
# compiling their sources directly (no changes to clients/silencer/). SIL points
# at the client tree two dirs over.
set(SIL "${CMAKE_CURRENT_SOURCE_DIR}/../silencer")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies (mirror clients/silencer: Homebrew SDL3, system libcurl) ----
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(SDL3 REQUIRED)
find_package(SDL3_ttf REQUIRED CONFIG)
find_package(CURL REQUIRED)

# --- Yoga (the flex backend behind the retained runtime) ----------------------
# Reuse the copy the client build already fetched to stay offline; fall back to
# a shallow clone only if it isn't present.
include(FetchContent)
if(EXISTS "${SIL}/build-release/_deps/yoga-src/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_YOGA "${SIL}/build-release/_deps/yoga-src" CACHE PATH "" FORCE)
elseif(EXISTS "${SIL}/build/_deps/yoga-src/CMakeLists.txt")
  set(FETCHCONTENT_SOURCE_DIR_YOGA "${SIL}/build/_deps/yoga-src" CACHE PATH "" FORCE)
endif()
FetchContent_Declare(
  yoga
  GIT_REPOSITORY https://github.com/facebook/yoga.git
  GIT_TAG v3.2.1
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR yoga)
FetchContent_MakeAvailable(yoga)

# --- cppx primitive transpile (box/text/button/input/scroll_view) -------------
# The generic ui::components primitives are authored as .cppx/.hx. Transpile the
# ones the launcher uses with the client's own tool, into a src-mirroring
# generated tree so `#include "ui/components/box.h"` resolves.
set(CPPX_TOOL "${SIL}/tools/cppx_transpile.py")
set(GEN_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated/cppx")
set(GEN_DIR "${GEN_ROOT}/src/ui/components")
file(MAKE_DIRECTORY "${GEN_DIR}")

set(CPPX_GENERATED "")
foreach(name box text button input scroll_view)
  add_custom_command(
    OUTPUT "${GEN_DIR}/${name}.h"
    COMMAND "${Python3_EXECUTABLE}" "${CPPX_TOOL}"
            "${SIL}/src/ui/components/${name}.hx" -o "${GEN_DIR}/${name}.h"
    DEPENDS "${SIL}/src/ui/components/${name}.hx" "${CPPX_TOOL}"
    WORKING_DIRECTORY "${SIL}" VERBATIM)
  add_custom_command(
    OUTPUT "${GEN_DIR}/${name}.cpp"
    COMMAND "${Python3_EXECUTABLE}" "${CPPX_TOOL}"
            "${SIL}/src/ui/components/${name}.cppx" -o "${GEN_DIR}/${name}.cpp"
    DEPENDS "${SIL}/src/ui/components/${name}.cppx" "${CPPX_TOOL}" "${GEN_DIR}/${name}.h"
    WORKING_DIRECTORY "${SIL}" VERBATIM)
  list(APPEND CPPX_GENERATED "${GEN_DIR}/${name}.cpp" "${GEN_DIR}/${name}.h")
endforeach()

# --- Reused client sources (the game-dep-free cppx UI closure) -----------------
set(SIL_UI_RUNTIME
  src/ui/runtime/react.cpp
  src/ui/runtime/element.cpp
  src/ui/runtime/tree.cpp
  src/ui/runtime/focus.cpp
  src/ui/runtime/interaction_hooks.cpp
  src/ui/runtime/flex_layout.cpp
  src/ui/runtime/yoga_flex_layout.cpp
  src/ui/runtime/draw_command.cpp
  src/ui/runtime/draw_command_builder.cpp
  src/ui/runtime/geometry.cpp
  src/ui/style/text_measure.cpp
  src/ui/style/default_theme.cpp
  src/ui/style/resolve.cpp)

set(SIL_UI_BRIDGE
  src/render/cppx_ui/pipeline_host.cpp
  src/render/cppx_ui/ui_draw_program.cpp
  src/render/cppx_ui/ui_surface.cpp
  src/render/cppx_ui/draw_executor.cpp
  src/render/cppx_ui/ui_draw_geometry.cpp
  src/render/cppx_ui/glyph_fonts.cpp
  src/render/cppx_ui/sdf_raster.cpp
  src/render/cppx_ui/font_registry.cpp
  src/render/cppx_ui/texture_registry.cpp
  src/render/cppx_ui/sprite_bake.cpp
  src/render/cppx_ui/text_measure.cpp
  src/platform/perf_trace.cpp)

set(SIL_APP_SHELL
  src/client/ui/app_shell/ui_pipeline.cpp
  src/client/ui/app_shell/client_ui.cpp
  src/client/ui/app_shell/navigation/screen_stack.cpp
  src/client/ui/providers/navigation_provider.cpp)

# Updater primitives (download + sha256 + zip extract). NOT updater.cpp /
# updaterstage2.cpp — the launcher is plain extract-and-replace.
set(SIL_UPDATER
  src/updater/updaterdownload.cpp
  src/updater/updatersha256.cpp
  src/updater/updaterzip.cpp)

set(SIL_SOURCES "")
foreach(s ${SIL_UI_RUNTIME} ${SIL_UI_BRIDGE} ${SIL_APP_SHELL} ${SIL_UPDATER})
  list(APPEND SIL_SOURCES "${SIL}/${s}")
endforeach()

# Lobby SDK (clients/lobby-sdk/cpp) — TCP opAuth sign-in against the lobby.
set(LOBBY_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../lobby-sdk/cpp")
set(LOBBY_SDK_SOURCES
  "${LOBBY_SDK}/src/client.cpp"
  "${LOBBY_SDK}/src/codec.cpp"
  "${LOBBY_SDK}/src/sha1.cpp")

# --- Launcher target ----------------------------------------------------------
add_executable(silencer-launcher
  src/main.cpp
  src/config.cpp
  src/net.cpp
  src/app.cpp
  src/backgrounds.cpp
  src/ui.cpp
  ${CPPX_GENERATED}
  ${SIL_SOURCES}
  ${LOBBY_SDK_SOURCES})

target_include_directories(silencer-launcher PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${SIL}/src"
  "${SIL}/src/platform"
  "${SIL}/src/render"
  "${SIL}/src/render/cppx_ui"
  "${SIL}/src/ui"
  "${SIL}/src/ui/runtime"
  "${SIL}/src/client/ui"
  "${SIL}/src/updater"
  "${LOBBY_SDK}/include"
  "${GEN_ROOT}/src"
  "${GEN_DIR}")

# Vendored headers (nlohmann/json, stb) — SYSTEM to mute their warnings.
target_include_directories(silencer-launcher SYSTEM PRIVATE "${SIL}/third_party")

target_link_libraries(silencer-launcher PRIVATE
  SDL3::SDL3 SDL3_ttf::SDL3_ttf CURL::libcurl yogacore)

# minizip is only needed by updaterzip.cpp off macOS (macOS shells out to
# ditto). Same find chain as the client: vcpkg's CMake config first, then
# pkg-config (Homebrew/apt).
if(NOT APPLE)
  find_package(unofficial-minizip CONFIG QUIET)
  if(TARGET unofficial::minizip::minizip)
    target_link_libraries(silencer-launcher PRIVATE unofficial::minizip::minizip)
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MINIZIP REQUIRED IMPORTED_TARGET minizip)
    target_link_libraries(silencer-launcher PRIVATE PkgConfig::MINIZIP)
  endif()
endif()

if(WIN32)
  target_link_libraries(silencer-launcher PRIVATE ws2_32)
endif()

target_compile_definitions(silencer-launcher PRIVATE
  LAUNCHER_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../shared/fonts"
  LAUNCHER_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../shared/assets")

if(APPLE)
  set_target_properties(silencer-launcher PROPERTIES OUTPUT_NAME "SilencerLauncher")
endif()
