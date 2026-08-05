cmake_minimum_required(VERSION 3.20)
project(SilencerLauncherStub CXX)

# The bootstrap stub (issue #347). Its safety comes from having no
# dependencies: no SDL, no cppx, no vendored TLS/zip, and NOTHING compiled in
# from clients/silencer/ or clients/launcher/. Keep it that way.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(STUB_SOURCES
  src/main.cpp
  src/manifest.cpp
  src/sha256.cpp)

if(WIN32)
  list(APPEND STUB_SOURCES src/platform_win.cpp src/stub.manifest)
elseif(APPLE)
  enable_language(OBJCXX)
  # CMAKE_CXX_STANDARD does not apply to OBJCXX; without this the .mm
  # compiles at the toolchain default, which is <17 on older AppleClang and
  # then <filesystem> hard-fails.
  set(CMAKE_OBJCXX_STANDARD 17)
  set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
  list(APPEND STUB_SOURCES src/platform_posix.cpp src/platform_mac.mm)
else()
  list(APPEND STUB_SOURCES src/platform_posix.cpp src/platform_linux.cpp)
endif()

add_executable(silencer-launcher-stub ${STUB_SOURCES})

if(WIN32)
  # GUI subsystem so no console window ever flashes; wWinMain is in
  # platform_win.cpp. CMake embeds src/stub.manifest (comctl32 v6) itself.
  set_target_properties(silencer-launcher-stub PROPERTIES WIN32_EXECUTABLE TRUE)
  target_link_libraries(silencer-launcher-stub PRIVATE winhttp comctl32 user32)
  target_compile_definitions(silencer-launcher-stub PRIVATE _CRT_SECURE_NO_WARNINGS)
elseif(APPLE)
  target_link_libraries(silencer-launcher-stub PRIVATE "-framework Cocoa")
else()
  target_link_libraries(silencer-launcher-stub PRIVATE dl pthread)
endif()

# Stand-in payload for the e2e (tests/e2e/run.sh) - never shipped.
add_executable(stub-test-payload tests/payload/main.cpp)
if(MSVC)
  target_compile_definitions(stub-test-payload PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
