cmake_minimum_required(VERSION 3.16)
project(SilencerLauncherCppx CXX)

# resources.rc (icon + VERSIONINFO) needs the RC language. project() above names
# CXX explicitly, so RC is not implied — enable it rather than rely on the
# platform module, which is what clients/silencer gets away with.
if(WIN32)
  enable_language(RC)
endif()

# The launcher reuses the game client's cppx UI engine + updater primitives by
# compiling their sources directly (no changes to clients/silencer/). SIL points
# at the client tree two dirs over.
set(SIL "${CMAKE_CURRENT_SOURCE_DIR}/../silencer")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The release tag drives the version, never a literal in a file. release.yml
# passes -DSILENCER_LAUNCHER_VERSION=${GITHUB_REF_NAME#v}; the default marks a
# local build the same way the client's SILENCER_VERSION default does.
set(SILENCER_LAUNCHER_VERSION "00000" CACHE STRING "Launcher version (from the git tag)")

# Info.plist reads this for LSMinimumSystemVersion, so it must have a value even
# in a plain local build. 11.0 matches the client's release configure.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
endif()

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
  src/ui/runtime/clipboard.cpp
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

  # Icon + version resource. VERSIONINFO wants a numeric quad, but our tags are
  # a zero-padded counter (vNNNNN), so map it to 0.0.N.0 — the same mapping
  # clients/tui/scripts/stage-npm-packages.ts uses for npm semver. Leading
  # zeros have to go: "0042" is not a number to the RC compiler.
  string(REGEX REPLACE "^0+" "" LAUNCHER_VERSION_N "${SILENCER_LAUNCHER_VERSION}")
  if(NOT LAUNCHER_VERSION_N MATCHES "^[0-9]+$")
    set(LAUNCHER_VERSION_N 0) # "00000", or a non-numeric override
  endif()
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/launcher_version.h.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/launcher_version.h" @ONLY)
  target_sources(silencer-launcher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources.rc")
  # CMake passes <INCLUDES> to rc.exe, so this is how resources.rc finds the
  # generated header.
  target_include_directories(silencer-launcher PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endif()

target_compile_definitions(silencer-launcher PRIVATE
  LAUNCHER_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../shared/fonts"
  LAUNCHER_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../shared/assets")

# --- Resource staging ---------------------------------------------------------
# An installed launcher has no source tree, so the fonts and the sprite banks it
# actually reads ship beside the binary: Contents/Resources inside the macOS
# bundle, next to the executable elsewhere. resolve_resource_dir() in main.cpp
# finds them through SDL_GetBasePath and falls back to the paths baked above
# only when running straight out of build/.
set(SHARED "${CMAKE_CURRENT_SOURCE_DIR}/../../shared")

# Only the banks backgrounds.cpp decodes: 0-4 + 7 + 208 for the backdrops and
# the logo, 132-136 for the glyph faces. The whole asset tree is 24M; this is 6M.
set(LAUNCHER_SPR_BANKS 000 001 002 003 004 007 132 133 134 135 136 208)

if(APPLE)
  # A bundle, not a bare executable: it keeps the resources with the binary as
  # one movable unit, and it is what codesigning and the DMG need.
  #
  # The launcher shares the game's icon (shared/icons/icon.icns) on purpose —
  # it is the same product, and the Dock reads better with one mark than with
  # two near-identical ones. CMake copies it in through the source property.
  set(LAUNCHER_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/../../shared/icons/icon.icns")
  set_source_files_properties("${LAUNCHER_ICNS}" PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")
  target_sources(silencer-launcher PRIVATE "${LAUNCHER_ICNS}")

  set_target_properties(silencer-launcher PROPERTIES
    MACOSX_BUNDLE TRUE
    # With a space, so the bundle is "Silencer Launcher.app" and the Finder
    # name matches CFBundleName. Everything downstream (package-macos.sh, the
    # CI action, release.yml) therefore has to quote its paths.
    OUTPUT_NAME "Silencer Launcher"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/SilencerLauncher-Info.plist"
    MACOSX_BUNDLE_BUNDLE_NAME "Silencer Launcher"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.arsiamons.silencer.launcher"
    MACOSX_BUNDLE_ICON_FILE "icon.icns"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${SILENCER_LAUNCHER_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${SILENCER_LAUNCHER_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright (c) Arsia Mons. All rights reserved.")
  set(LAUNCHER_RES_DIR "$<TARGET_BUNDLE_CONTENT_DIR:silencer-launcher>/Resources")
else()
  set(LAUNCHER_RES_DIR "$<TARGET_FILE_DIR:silencer-launcher>")
endif()

add_custom_command(TARGET silencer-launcher POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${LAUNCHER_RES_DIR}/fonts"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${LAUNCHER_RES_DIR}/assets/bin_spr"
  VERBATIM)

file(GLOB LAUNCHER_OTF "${SHARED}/fonts/*.otf")
foreach(f ${LAUNCHER_OTF})
  add_custom_command(TARGET silencer-launcher POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${f}" "${LAUNCHER_RES_DIR}/fonts/"
    VERBATIM)
endforeach()

foreach(f BIN_SPR.DAT PALETTE.BIN)
  add_custom_command(TARGET silencer-launcher POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SHARED}/assets/${f}"
            "${LAUNCHER_RES_DIR}/assets/" VERBATIM)
endforeach()

foreach(b ${LAUNCHER_SPR_BANKS})
  add_custom_command(TARGET silencer-launcher POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SHARED}/assets/bin_spr/SPR_${b}.BIN" "${LAUNCHER_RES_DIR}/assets/bin_spr/"
    VERBATIM)
endforeach()
