cmake_minimum_required(VERSION 3.15)

project(Silencer)

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

# /Z7 embeds debug info into .obj files instead of routing it through MSVC's
# mspdbsrv.exe — which has a habit of failing with C1090 PDB errors on some
# Windows installs. /Zi (the default) is faster but unreliable; /Z7 is rock
# solid and debuggers handle it identically. Linker still emits Silencer.pdb
# for runtime debugging.
if(MSVC)
	target_compile_options(${SILENCER_TARGET} PRIVATE $<$<CONFIG:Debug>:/Z7>)
endif()

set(SILENCER_LOBBY_HOST "127.0.0.1" CACHE STRING "Lobby host the client connects to at startup")
set(SILENCER_LOBBY_PORT "517" CACHE STRING "Lobby TCP port")
set(SILENCER_VERSION "00025" CACHE STRING "Client version string sent to the lobby during MSG_VERSION")
set(SILENCER_MAP_API_URL "http://127.0.0.1:15172" CACHE STRING "Base URL for the community map HTTP API (used when fetching a missing map before falling back to P2P)")
target_compile_definitions(${SILENCER_TARGET} PRIVATE
	SILENCER_LOBBY_HOST="${SILENCER_LOBBY_HOST}"
	SILENCER_LOBBY_PORT=${SILENCER_LOBBY_PORT}
	SILENCER_VERSION="${SILENCER_VERSION}"
	SILENCER_MAP_API_URL="${SILENCER_MAP_API_URL}"
)

find_package(ZLIB REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)
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

include_directories(${SDL2_INCLUDE_DIR} ${SDL2_MIXER_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})

if(APPLE)
	find_library(COCOA_FRAMEWORK Cocoa)
	target_link_libraries(${SILENCER_TARGET}
		${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY} ${ZLIB_LIBRARY}
		CURL::libcurl
		${COCOA_FRAMEWORK})
elseif(WIN32)
	target_link_libraries(${SILENCER_TARGET}
		${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY} ${ZLIB_LIBRARY}
		CURL::libcurl ws2_32)
else()
	target_link_libraries(${SILENCER_TARGET}
		${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY} ${ZLIB_LIBRARY}
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
