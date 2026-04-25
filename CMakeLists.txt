cmake_minimum_required(VERSION 3.15)

set(PROJECT zsilencer)
project(${PROJECT})

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if(APPLE)
	file(GLOB_RECURSE sources src/*.cpp src/*.h src/*.mm)
else()
	file(GLOB_RECURSE sources src/*.cpp src/*.h)
endif()

# Extra files that get bundled into the .app on macOS (icon + data/).
set(bundle_extras "")
if(APPLE)
	set(bundle_icon "${CMAKE_CURRENT_SOURCE_DIR}/res/icon.icns")
	set_source_files_properties("${bundle_icon}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	list(APPEND bundle_extras "${bundle_icon}")

	file(GLOB_RECURSE data_files "${CMAKE_CURRENT_SOURCE_DIR}/data/*")
	foreach(f ${data_files})
		file(RELATIVE_PATH rel "${CMAKE_CURRENT_SOURCE_DIR}" "${f}")
		get_filename_component(rel_dir "${rel}" DIRECTORY)
		set_source_files_properties("${f}" PROPERTIES MACOSX_PACKAGE_LOCATION "${rel_dir}")
		list(APPEND bundle_extras "${f}")
	endforeach()
endif()

if(APPLE)
	add_executable(${PROJECT} MACOSX_BUNDLE ${sources} ${bundle_extras})
	set_target_properties(${PROJECT} PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/zSILENCER-Info.plist"
		MACOSX_BUNDLE_BUNDLE_NAME "zSILENCER"
		MACOSX_BUNDLE_GUI_IDENTIFIER "com.zsilencer.app"
		MACOSX_BUNDLE_ICON_FILE "icon.icns"
	)
elseif(WIN32)
	add_executable(${PROJECT} WIN32 ${sources})
else()
	add_executable(${PROJECT} ${sources})
endif()

target_compile_features(${PROJECT} PUBLIC cxx_std_14)

set(ZSILENCER_LOBBY_HOST "127.0.0.1" CACHE STRING "Lobby host the client connects to at startup")
set(ZSILENCER_LOBBY_PORT "517" CACHE STRING "Lobby TCP port")
set(ZSILENCER_VERSION "00025" CACHE STRING "Client version string sent to the lobby during MSG_VERSION")
target_compile_definitions(${PROJECT} PRIVATE
	ZSILENCER_LOBBY_HOST="${ZSILENCER_LOBBY_HOST}"
	ZSILENCER_LOBBY_PORT=${ZSILENCER_LOBBY_PORT}
	ZSILENCER_VERSION="${ZSILENCER_VERSION}"
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
	target_link_libraries(${PROJECT}
		${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY} ${ZLIB_LIBRARY}
		CURL::libcurl
		${COCOA_FRAMEWORK})
elseif(WIN32)
	target_link_libraries(${PROJECT}
		${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY} ${ZLIB_LIBRARY}
		CURL::libcurl ws2_32)
else()
	target_link_libraries(${PROJECT}
		${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY} ${ZLIB_LIBRARY}
		CURL::libcurl)
endif()

target_link_libraries(${PROJECT} minizip_dep)

install(TARGETS "${PROJECT}" RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" BUNDLE DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "data/" DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT}")
install(FILES "res/icon_16.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/16x16/apps" RENAME "zsilencer.png")
install(FILES "res/icon_32.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps" RENAME "zsilencer.png")
install(FILES "res/icon_64.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps" RENAME "zsilencer.png")
install(FILES "res/icon_128.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps" RENAME "zsilencer.png")
install(FILES "zsilencer.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")

set(CPACK_PACKAGE_NAME "zSilencer")
set(CPACK_PACKAGE_VERSION "00023")

option(ZSILENCER_BUILD_TESTS "Build unit tests" OFF)
if(ZSILENCER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(CPack)
