cmake_minimum_required(VERSION 3.16)
project(silencer_lobby_sdk CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(silencer_lobby_sdk_sources
    src/codec.cpp
    src/sha1.cpp
)
# client.cpp uses POSIX sockets (sys/socket.h / arpa/inet.h). Per the SDK's
# CLAUDE.md, a Win32 port is a focused follow-up; the codec layer is fully
# portable and is what the test suite covers.
if (NOT WIN32)
    list(APPEND silencer_lobby_sdk_sources src/client.cpp)
endif()
add_library(silencer_lobby_sdk STATIC ${silencer_lobby_sdk_sources})
target_include_directories(silencer_lobby_sdk PUBLIC include)
if (MSVC)
    target_compile_options(silencer_lobby_sdk PRIVATE /W4 /permissive-)
else()
    target_compile_options(silencer_lobby_sdk PRIVATE -Wall -Wextra -Wpedantic)
endif()
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

if (SILENCER_LOBBY_SDK_EXAMPLES AND NOT WIN32)
    # chat_listener depends on the Client/socket path which is POSIX-only.
    add_executable(chat_listener examples/chat_listener.cpp)
    target_link_libraries(chat_listener PRIVATE silencer_lobby_sdk)
endif()
