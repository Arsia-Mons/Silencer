# All test sources aggregate into one binary: keeps link+run simple,
# doctest handles discovery via its built-in main().
# doctest v2.4.11 vendored verbatim from https://github.com/doctest/doctest
set(TEST_SRC
    smoke_test.cpp
    updater_sha256_test.cpp
    updater_download_test.cpp
    updater_zip_test.cpp
    updater_sm_test.cpp
    control_widget_test.cpp
    ui_architecture_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/runtime/clay_init.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/runtime/ClayService.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/runtime/UiInteractionRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/runtime/UiFrameContext.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/runtime/UiInputRouter.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/runtime/UiTextPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/primitives/button.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/primitives/bank_text.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/primitives/box.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/primitives/scroll_list.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/primitives/scroll_text_box.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/primitives/text_input.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/primitives/toggle.cpp
    ${CMAKE_SOURCE_DIR}/src/client/ui/ClientUi.cpp
    ${CMAKE_SOURCE_DIR}/src/client/ui/ClientUiInput.cpp
    ${CMAKE_SOURCE_DIR}/src/client/ui/hud/HudPayloadArena.cpp
    ${CMAKE_SOURCE_DIR}/src/client/ui/navigation/ScreenStack.cpp
    ${CMAKE_SOURCE_DIR}/src/updater/updatersha256.cpp
    ${CMAKE_SOURCE_DIR}/src/updater/updaterdownload.cpp
    ${CMAKE_SOURCE_DIR}/src/updater/updaterzip.cpp
    ${CMAKE_SOURCE_DIR}/src/updater/updater.cpp
)

add_executable(silencer_tests ${TEST_SRC})
target_compile_features(silencer_tests PUBLIC cxx_std_20)
target_include_directories(silencer_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/platform
    ${CMAKE_SOURCE_DIR}/src/render
    ${CMAKE_SOURCE_DIR}/src/audio
    ${CMAKE_SOURCE_DIR}/src/input
    ${CMAKE_SOURCE_DIR}/src/world
    ${CMAKE_SOURCE_DIR}/src/actors
    ${CMAKE_SOURCE_DIR}/src/projectiles
    ${CMAKE_SOURCE_DIR}/src/objects
    ${CMAKE_SOURCE_DIR}/src/stations
    ${CMAKE_SOURCE_DIR}/src/gas
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/ui/runtime
    ${CMAKE_SOURCE_DIR}/src/client
    ${CMAKE_SOURCE_DIR}/src/client/ui
    ${CMAKE_SOURCE_DIR}/src/client/ui/modals
    ${CMAKE_SOURCE_DIR}/src/updater
    ${CMAKE_SOURCE_DIR}/src/client/ui/screens
    ${CMAKE_SOURCE_DIR}/src/client/ui/screens/main_menu
    ${CMAKE_SOURCE_DIR}/src/client/ui/screens/options
    ${CMAKE_SOURCE_DIR}/src/client/ui/screens/lobby_connect
    ${CMAKE_SOURCE_DIR}/src/client/ui/screens/update
    ${CMAKE_SOURCE_DIR}/src/client/ui/screens/lobby
    ${CMAKE_SOURCE_DIR}/src/client/ui/screens/mission_summary
    ${CMAKE_SOURCE_DIR}/src/net
    ${CMAKE_SOURCE_DIR}/src/game
    ${CMAKE_SOURCE_DIR}/src/resources
    ${CMAKE_SOURCE_DIR}/src/events
)

add_test(NAME silencer_tests COMMAND silencer_tests)

target_compile_definitions(silencer_tests PRIVATE
    ZIP_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    SILENCER_TEST_BUILD
)

# Download wrapper needs curl headers even in tests (for the enum).
find_package(CURL REQUIRED)
target_link_libraries(silencer_tests PRIVATE CURL::libcurl)

find_package(ZLIB REQUIRED)
target_link_libraries(silencer_tests PRIVATE ${ZLIB_LIBRARY})
# Reuse the parent CMakeLists' minizip_dep interface target — it already
# handles vcpkg vs homebrew/pkg-config include-path differences (vcpkg
# flattens unzip.h into include/, homebrew nests it in include/minizip/).
if(TARGET minizip_dep)
    target_link_libraries(silencer_tests PRIVATE minizip_dep)
endif()
