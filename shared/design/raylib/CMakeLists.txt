cmake_minimum_required(VERSION 3.16)
project(silencer_design_raylib C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Allow Homebrew prefixes on macOS / Linux
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /usr/local)
endif()

find_package(raylib 5.0 REQUIRED)

set(SRC
    src/main.c
    src/palette.c
    src/sprite.c
    src/font.c
    src/widgets/button.c
    src/widgets/toggle.c
    src/widgets/textinput.c
    src/widgets/textbox.c
    src/widgets/selectbox.c
    src/widgets/scrollbar.c
    src/widgets/overlay.c
    src/widgets/interface.c
    src/widgets/panel.c
    src/widgets/modal.c
    src/widgets/loading.c
    src/widgets/hud.c
    src/widgets/minimap.c
    src/screens/palette_screen.c
    src/screens/typography_screen.c
    src/screens/buttons_screen.c
    src/screens/toggles_screen.c
    src/screens/inputs_screen.c
    src/screens/lists_screen.c
    src/screens/panels_screen.c
    src/screens/main_menu_screen.c
    src/screens/lobby_screen.c
    src/screens/hud_screen.c
    src/screens/buy_menu_screen.c
)

add_executable(silencer_design ${SRC})
target_include_directories(silencer_design PRIVATE src)
target_link_libraries(silencer_design PRIVATE raylib)

# macOS frameworks (raylib transitively links these but be explicit)
if(APPLE)
    target_link_libraries(silencer_design PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
endif()

# Warnings
target_compile_options(silencer_design PRIVATE
    -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
)
