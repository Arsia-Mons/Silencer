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
target_compile_definitions(${PROJECT} PRIVATE
	ZSILENCER_LOBBY_HOST="${ZSILENCER_LOBBY_HOST}"
	ZSILENCER_LOBBY_PORT=${ZSILENCER_LOBBY_PORT}
)

find_package(ZLIB REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)
include_directories(${SDL2_INCLUDE_DIR} ${SDL2_MIXER_INCLUDE_DIRS})

if(APPLE)
	find_library(COCOA_FRAMEWORK Cocoa)
	target_link_libraries(${PROJECT} ${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY} ${ZLIB_LIBRARY} ${COCOA_FRAMEWORK})
else()
	target_link_libraries(${PROJECT} ${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY} ${ZLIB_LIBRARY})
endif()

install(TARGETS "${PROJECT}" RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" BUNDLE DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY "data/" DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT}")
install(FILES "res/icon_16.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/16x16/apps" RENAME "zsilencer.png")
install(FILES "res/icon_32.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps" RENAME "zsilencer.png")
install(FILES "res/icon_64.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps" RENAME "zsilencer.png")
install(FILES "res/icon_128.png" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps" RENAME "zsilencer.png")
install(FILES "zsilencer.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")

set(CPACK_PACKAGE_NAME "zSilencer")
set(CPACK_PACKAGE_VERSION "00023")

include(CPack)
