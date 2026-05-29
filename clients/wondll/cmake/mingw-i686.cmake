# cmake/mingw-i686.cmake — MinGW i686 cross-compile toolchain
# Used on macOS/Linux to build WONDLL.dll for 32-bit Windows.
#
# Install cross-compiler:
#   macOS:  brew install mingw-w64
#   Ubuntu: apt install gcc-mingw-w64-i686

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_C_COMPILER   i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  i686-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
