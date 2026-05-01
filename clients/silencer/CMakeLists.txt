cmake_minimum_required(VERSION 3.16)

project(Silencer)

# nlohmann/json v3.12.0 — vendored header (third_party/nlohmann/json.hpp)
# Vendored to avoid CMake FetchContent redirect issues with GitHub CDN in CI.
set(NLOHMANN_JSON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

# Linux GNU lowercase convention; macOS/Windows pick up mixed-case via
# bundle/output-name overrides below.
set(SILENCER_TARGET silencer)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Shared monorepo dirs sit two levels up from clients/silencer/.
set(SILENCER_SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../shared")

if(APPLE)
	file(GLOB_RECURSE sources src/*.cpp src/*.h src/*.mm)
else()
	file(GLOB_RECURSE sources src/*.cpp src/*.h)
endif()

# Extra files that get bundled into the .app on macOS (icon + shared/assets/).
set(bundle_extras "")
if(APPLE)
	set(bundle_icon "${SILENCER_SHARED_DIR}/icons/icon.icns")
	set_source_files_properties("${bundle_icon}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	list(APPEND bundle_extras "${bundle_icon}")

	# Files land at Contents/assets/* in the .app — the C++ runtime's
	# fallback path (`main.cpp` `CDResDir`) looks for `../assets/PALETTE.BIN`
	# next to the executable in `Contents/MacOS/`. Engineer-facing docs
	# (CLAUDE.md, AGENTS.md) live in shared/assets/ but don't ship.
	file(GLOB_RECURSE asset_files "${SILENCER_SHARED_DIR}/assets/*")
	list(FILTER asset_files EXCLUDE REGEX "/(CLAUDE|AGENTS)[.]md$")
	foreach(f ${asset_files})
		file(RELATIVE_PATH rel "${SILENCER_SHARED_DIR}/assets" "${f}")
		get_filename_component(rel_dir "${rel}" DIRECTORY)
		if(rel_dir)
			set_source_files_properties("${f}" PROPERTIES MACOSX_PACKAGE_LOCATION "assets/${rel_dir}")
		else()
			set_source_files_properties("${f}" PROPERTIES MACOSX_PACKAGE_LOCATION "assets")
		endif()
		list(APPEND bundle_extras "${f}")
	endforeach()
endif()

if(APPLE)
	add_executable(${SILENCER_TARGET} MACOSX_BUNDLE ${sources} ${bundle_extras})
	set_target_properties(${SILENCER_TARGET} PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Silencer-Info.plist"
		MACOSX_BUNDLE_BUNDLE_NAME "Silencer"
		MACOSX_BUNDLE_GUI_IDENTIFIER "com.silencer.app"
		MACOSX_BUNDLE_ICON_FILE "icon.icns"
		OUTPUT_NAME "Silencer"
	)
elseif(WIN32)
	add_executable(${SILENCER_TARGET} WIN32 ${sources})
	set_target_properties(${SILENCER_TARGET} PROPERTIES OUTPUT_NAME "Silencer")
	# Embed the application icon (resources.rc references shared/icons/icon.ico).
	target_sources(${SILENCER_TARGET} PRIVATE resources.rc)
else()
	add_executable(${SILENCER_TARGET} ${sources})
endif()

target_compile_features(${SILENCER_TARGET} PUBLIC cxx_std_14)

if(WIN32)
	# Strip <winsock.h> (and other legacy subsystems) out of <windows.h>.
	# Without this, any TU that pulls in <windows.h> via zlib's zconf.h,
	# behaviortree.cpp, actordef.cpp, etc. drags in winsock 1.x; another
	# TU including <winsock2.h> then redefines sockaddr/fd_set/etc. The
	# collision is invisible to non-unity builds (each TU is isolated)
	# but surfaces in unity batches. We use winsock2 only — no Shell/
	# RPC/Crypto/DDE call sites in the codebase to break.
	target_compile_definitions(${SILENCER_TARGET} PRIVATE WIN32_LEAN_AND_MEAN)
endif()

# Unity (jumbo) build: combines src/*.cpp into batched TUs to dramatically
# speed up clean builds. Off by default — incremental single-file edits are
# slower under unity since changing one .cpp recompiles its whole batch.
# CI release/deploy workflows pass -DSILENCER_UNITY_BUILD=ON.
option(SILENCER_UNITY_BUILD "Combine .cpp files into jumbo TUs for faster clean builds" OFF)
if(SILENCER_UNITY_BUILD)
	set_target_properties(${SILENCER_TARGET} PROPERTIES UNITY_BUILD ON)
	if(APPLE)
		# CMake batches Objective-C++ .mm files into the CXX jumbo TU on
		# macOS rather than separating by language, so #import <Cocoa/Cocoa.h>
		# from cocoawrapper.mm leaks Objective-C types (NSString, etc.) into
		# a C++ translation unit and breaks compilation. We have only one .mm
		# file — unity savings from including it are nil — so skip it.
		file(GLOB_RECURSE _mm_sources src/*.mm)
		set_source_files_properties(${_mm_sources} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
	endif()
endif()

# Compile HLSL → DXIL bytecode headers for the SDL3 GPU backend. macOS keeps
# the existing MSL source strings; SPIRV/Linux is a separate change.
include(CompileShaders)
silencer_compile_shader(vert_screen   vertex  vert_screen)
silencer_compile_shader(frag_remap    pixel   frag_remap)
silencer_compile_shader(frag_upscale  pixel   frag_upscale)
silencer_compile_shader(frag_light    pixel   frag_light)
silencer_compile_shader(vert_particle vertex  vert_particle)
silencer_compile_shader(frag_particle pixel   frag_particle)
silencer_compile_shader(comp_particle compute update_particles)
silencer_finalize_shaders(
    vert_screen frag_remap frag_upscale frag_light
    vert_particle frag_particle comp_particle
)
add_dependencies(${SILENCER_TARGET} silencer_shaders)
target_include_directories(${SILENCER_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated")
target_include_directories(${SILENCER_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
# Expose src/ and every first-level subdomain so that bare-name includes
# (#include "lobby.h") continue to resolve after the src/ reorganisation.
target_include_directories(${SILENCER_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/render"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/actors"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/projectiles"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/objects"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/stations"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/net"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/updater"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/resources"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gas"
)

# /Z7 embeds debug info into .obj files instead of routing it through MSVC's
# mspdbsrv.exe — which has a habit of failing with C1090 PDB errors on some
# Windows installs. /Zi (the default) is faster but unreliable; /Z7 is rock
# solid and debuggers handle it identically. Linker still emits Silencer.pdb
# for runtime debugging.
if(MSVC)
	target_compile_options(${SILENCER_TARGET} PRIVATE $<$<CONFIG:Debug>:/Z7>)
	# MSVC 14.44+ removed _snprintf from std::; nlohmann/json uses it internally.
	# Force-include the shim before every TU to re-expose it.
	target_compile_options(${SILENCER_TARGET} PRIVATE
		"/FI${CMAKE_CURRENT_SOURCE_DIR}/cmake/msvc_snprintf_compat.h")
endif()

set(SILENCER_LOBBY_HOST "lobby.arsiamons.com" CACHE STRING "Lobby host the client connects to at startup")
set(SILENCER_LOBBY_PORT "517" CACHE STRING "Lobby TCP port")
set(SILENCER_VERSION "00000" CACHE STRING "Client version string sent to the lobby during MSG_VERSION; overridden by release.yml from git tag")
set(SILENCER_MAP_API_URL "https://admin.arsiamons.com" CACHE STRING "Base URL for the community map HTTP API (used when fetching a missing map before falling back to P2P)")
set(SILENCER_ADMIN_API_URL "https://admin.arsiamons.com" CACHE STRING "Base URL for the admin API (actordefs sync, etc.)")
target_compile_definitions(${SILENCER_TARGET} PRIVATE
	SILENCER_LOBBY_HOST="${SILENCER_LOBBY_HOST}"
	SILENCER_LOBBY_PORT=${SILENCER_LOBBY_PORT}
	SILENCER_VERSION="${SILENCER_VERSION}"
	SILENCER_MAP_API_URL="${SILENCER_MAP_API_URL}"
	SILENCER_ADMIN_API_URL="${SILENCER_ADMIN_API_URL}"
)

find_package(ZLIB REQUIRED)
find_package(SDL3 REQUIRED)
find_package(SDL3_mixer REQUIRED)
find_package(CURL REQUIRED)

# Minizip: prefer vcpkg's CMake CONFIG packages (Windows CI), then fall back
# to pkg-config for Homebrew/apt. Wrap whichever we find in a single
# `minizip_dep` interface target so the link sections below don't have to
# care which source it came from. Fail loudly if none resolves — a silent
# miss used to slip through and surface as `unzip.h not found`.
find_package(unofficial-minizip CONFIG QUIET)
if(NOT unofficial-minizip_FOUND)
	find_package(minizip CONFIG QUIET)
endif()

add_library(minizip_dep INTERFACE)
if(TARGET unofficial::minizip::minizip)
	target_link_libraries(minizip_dep INTERFACE unofficial::minizip::minizip)
elseif(TARGET minizip::minizip)
	target_link_libraries(minizip_dep INTERFACE minizip::minizip)
else()
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(MINIZIP REQUIRED IMPORTED_TARGET minizip)
	# Homebrew/apt minizip ships headers at <prefix>/include/minizip/unzip.h,
	# so the pkg-config -I flag alone doesn't let `#include <unzip.h>` resolve.
	# vcpkg's minizip flattens them into include/ directly, which is why
	# Windows doesn't hit this.
	foreach(_dir ${MINIZIP_INCLUDE_DIRS})
		if(IS_DIRECTORY "${_dir}/minizip")
			target_include_directories(minizip_dep INTERFACE "${_dir}/minizip")
		endif()
	endforeach()
	target_link_libraries(minizip_dep INTERFACE PkgConfig::MINIZIP)
endif()

include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${NLOHMANN_JSON_INCLUDE_DIR})
include_directories(${NLOHMANN_JSON_INCLUDE_DIR}/nlohmann)
find_package(SDL3_mixer REQUIRED CONFIG)
if(TARGET SDL3_mixer::SDL3_mixer)
  get_target_property(_sdl3mixer_inc SDL3_mixer::SDL3_mixer INTERFACE_INCLUDE_DIRECTORIES)
  if(_sdl3mixer_inc)
    include_directories(${_sdl3mixer_inc})
  endif()
endif()

if(APPLE)
	find_library(COCOA_FRAMEWORK Cocoa)
	target_link_libraries(${SILENCER_TARGET}
		SDL3::SDL3 SDL3_mixer::SDL3_mixer ${ZLIB_LIBRARY}
		CURL::libcurl
		${COCOA_FRAMEWORK})
elseif(WIN32)
	target_link_libraries(${SILENCER_TARGET}
		SDL3::SDL3 SDL3_mixer::SDL3_mixer ${ZLIB_LIBRARY}
		CURL::libcurl ws2_32)
else()
	target_link_libraries(${SILENCER_TARGET}
		SDL3::SDL3 SDL3_mixer::SDL3_mixer ${ZLIB_LIBRARY}
		CURL::libcurl)
endif()

target_link_libraries(${SILENCER_TARGET} minizip_dep)

install(TARGETS "${SILENCER_TARGET}" RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" BUNDLE DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "${SILENCER_SHARED_DIR}/assets/" DESTINATION "${CMAKE_INSTALL_DATADIR}/silencer")
install(FILES "${SILENCER_SHARED_DIR}/icons/icon_16.png"  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/16x16/apps"   RENAME "silencer.png")
install(FILES "${SILENCER_SHARED_DIR}/icons/icon_32.png"  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps"   RENAME "silencer.png")
install(FILES "${SILENCER_SHARED_DIR}/icons/icon_64.png"  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps"   RENAME "silencer.png")
install(FILES "${SILENCER_SHARED_DIR}/icons/icon_128.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps" RENAME "silencer.png")
install(FILES "silencer.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")

set(CPACK_PACKAGE_NAME "Silencer")
set(CPACK_PACKAGE_FILE_NAME "silencer")
set(CPACK_PACKAGE_VERSION "00023")

option(SILENCER_BUILD_TESTS "Build unit tests" OFF)
if(SILENCER_BUILD_TESTS)
	enable_testing()
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../tests ${CMAKE_BINARY_DIR}/tests)
endif()

include(CPack)
