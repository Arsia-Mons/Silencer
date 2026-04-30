cmake_minimum_required(VERSION 3.16)
project(silencer_lobby_sdk CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_library(silencer_lobby_sdk STATIC
    src/codec.cpp
    src/client.cpp
    src/sha1.cpp
)
target_include_directories(silencer_lobby_sdk PUBLIC include)
target_compile_options(silencer_lobby_sdk PRIVATE -Wall -Wextra -Wpedantic)
add_library(silencer::lobby_sdk ALIAS silencer_lobby_sdk)

option(SILENCER_LOBBY_SDK_TESTS "Build SDK tests" ON)
option(SILENCER_LOBBY_SDK_EXAMPLES "Build SDK examples" ON)

if (SILENCER_LOBBY_SDK_TESTS)
    enable_testing()
    add_executable(codec_test tests/codec_test.cpp)
    target_link_libraries(codec_test PRIVATE silencer_lobby_sdk)
    target_compile_definitions(codec_test PRIVATE
        SILENCER_VECTORS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../../../shared/lobby-protocol/vectors.json")
    add_test(NAME codec_test COMMAND codec_test)
endif()

if (SILENCER_LOBBY_SDK_EXAMPLES)
    add_executable(chat_listener examples/chat_listener.cpp)
    target_link_libraries(chat_listener PRIVATE silencer_lobby_sdk)
endif()
