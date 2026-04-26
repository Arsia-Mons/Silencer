cmake_minimum_required(VERSION 3.20)
project(silencer_design_sdl3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find SDL3 (Homebrew or system install).
find_package(SDL3 CONFIG REQUIRED)

add_executable(silencer_design
    src/main.cpp
    src/palette.cpp
    src/sprite.cpp
    src/font.cpp
    src/widgets/button.cpp
    src/widgets/toggle.cpp
    src/widgets/textinput.cpp
    src/widgets/textbox.cpp
    src/widgets/selectbox.cpp
    src/widgets/scrollbar.cpp
    src/widgets/overlay.cpp
    src/widgets/interface.cpp
    src/widgets/panel.cpp
    src/widgets/modal.cpp
    src/widgets/loadingbar.cpp
    src/widgets/hudbars.cpp
    src/widgets/minimap.cpp
    src/widgets/primitives.cpp
    src/screens/screen_palette.cpp
    src/screens/screen_typography.cpp
    src/screens/screen_buttons.cpp
    src/screens/screen_inputs.cpp
    src/screens/screen_selectbox.cpp
    src/screens/screen_overlay.cpp
    src/screens/screen_panel.cpp
    src/screens/screen_modal.cpp
    src/screens/screen_loading.cpp
    src/screens/screen_hud.cpp
    src/screens/screen_minimap.cpp
    src/screens/screen_main_menu.cpp
    src/screens/screen_lobby.cpp
    src/screens/screen_buy_menu.cpp
)

target_include_directories(silencer_design PRIVATE src)

if(TARGET SDL3::SDL3)
    target_link_libraries(silencer_design PRIVATE SDL3::SDL3)
else()
    target_link_libraries(silencer_design PRIVATE SDL3::SDL3-shared)
endif()

if(APPLE)
    target_compile_options(silencer_design PRIVATE -Wall -Wno-unused-parameter)
elseif(UNIX)
    target_compile_options(silencer_design PRIVATE -Wall -Wno-unused-parameter)
endif()
