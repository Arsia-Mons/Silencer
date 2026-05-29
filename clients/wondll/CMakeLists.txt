cmake_minimum_required(VERSION 3.16)
project(wondll C)

# This target must be built as a 32-bit Windows DLL so it can be loaded by
# the original Silencer.exe (Win32/x86).
#
# On macOS/Linux, cross-compile with MinGW:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-i686.cmake
#   cmake --build build
#
# On Windows with MSVC, open a "x86 Native Tools" prompt and run:
#   cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# Output: build/WONDLL.dll — copy alongside Silencer.exe to override the
# original WON.net DLL.

add_library(WONDLL SHARED wondll.c)

target_compile_definitions(WONDLL PRIVATE WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)

# Link WinInet for HTTP and Ws2_32 for inet_addr.
target_link_libraries(WONDLL PRIVATE wininet ws2_32)

# Ensure the output is named exactly WONDLL.dll (case matters on some systems).
set_target_properties(WONDLL PROPERTIES
    OUTPUT_NAME "WONDLL"
    PREFIX      ""
    SUFFIX      ".dll"
)

# 32-bit forced when using MSVC via CMAKE_GENERATOR_PLATFORM or toolchain.
if(MSVC)
    target_compile_options(WONDLL PRIVATE /W3)
    # Export all __declspec(dllexport) symbols; no .def file needed.
endif()
