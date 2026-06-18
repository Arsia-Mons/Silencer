# Silencer Client — Building

Reference for building `clients/silencer/`. The one rule that belongs
in CLAUDE.md: **build/configure only through the wrapper, never raw
`cmake`/`cl`/`ninja`, never CLion's bundled MinGW** (MSVC-only
codebase — MinGW cannot link it). Everything below is on-demand
detail.

## Commands

```
# Windows
clients/silencer/build.ps1 [preset] [-Clean]
# macOS / Linux
clients/silencer/build.sh  [preset] [--clean]
```

`-Clean` / `--clean` wipes `CMakeCache.txt` + `CMakeFiles` (keeps
`vcpkg_installed`) before configuring — the correct recovery from a
poisoned cache. Don't hand-roll `rm`/reconfigure loops.

## Presets

| Preset | Build type | Dir | Notes |
|--------|-----------|-----|-------|
| `win-ninja` (default) | Debug | `build/` | day-to-day |
| `win-ninja-release` | Release | `build-release/` | |
| `win-ninja-unity` | Release + `SILENCER_UNITY_BUILD=ON` | `build-unity/` | fastest clean build |

Names are shared across platforms even though `win-` is a misnomer
off Windows. On Windows the preset is a real `CMakePresets.json`
entry (Ninja generator, `x64-windows` triplet, vcpkg toolchain); the
presets carry a `hostSystemName == Windows` condition, so `build.sh`
does **not** use them — it maps the preset name to
`-DCMAKE_BUILD_TYPE` / `-DSILENCER_UNITY_BUILD` and invokes `cmake`
directly with `-G Ninja` (if `ninja` is on `PATH`).

## What the wrapper enforces

- **Visual Studio (Windows only).** Pins the newest install carrying
  the C++ x64 toolset via `vswhere` and runs the build under its
  `vcvars64.bat`. No VS with that toolset → hard error.
- **`VCPKG_ROOT`.** Windows resolves it process env → persisted User
  env → hard error (with the `SetEnvironmentVariable` hint). macOS/
  Linux uses it when exported; otherwise it keeps the historical
  CMake/pkg-config system package discovery path. When set, it must
  contain `scripts/buildsystems/vcpkg.cmake`.
- **No concurrent configures.** Aborts if a build tool is already
  running (`cmake`/`ninja`/`cl`/`MSBuild` on Windows;
  `cmake`/`ninja`/`cc1plus`/`clang` elsewhere) — concurrent
  configures corrupt the shared CMake cache. Idle CLion is fine; just
  don't run an IDE build into the same dir at the same time.
- **No running target.** Refuses if a `Silencer` built from that dir
  is running — it locks the link target (Windows: cryptic `LNK1168`).
- **Cooperative lock.** Atomic `.silencer-build.lock` in the build
  dir; a stale lock from a dead PID is reclaimed automatically.

## Compile-time config knobs

These are CMake cache variables baked at configure time. Defaults
live in `CMakeLists.txt`; CI overrides them in its own configure
step (the wrapper takes no pass-through args, so changing them for a
local build means reconfiguring with the `-D` set).

- **`SILENCER_LOBBY_HOST` / `SILENCER_LOBBY_PORT`** — default
  `127.0.0.1:517`; CI sets `lobby.arsiamons.com`. Rebuild to point
  the client at a different lobby.
- **`SILENCER_VERSION`** — the wire-handshake version string. Must
  match the lobby's `-version` (which defaults to the same value);
  bump both together or the handshake fails. `CPACK_PACKAGE_VERSION`
  is installer metadata only — unrelated to the handshake.

## cppx UI pipeline (Python3 build dependency)

The UI is authored in `.cppx`/`.hx` (JSX-like C++) and transpiled to
ordinary C++ at **build time** by `tools/cppx_transpile.py`, wired into
CMake via `cmake/cppx_transpile.cmake`. Configure therefore requires
`Python3` (`find_package(Python3 COMPONENTS Interpreter REQUIRED)`) — a
hard build dependency; install it if configure errors with a missing
Python3.

Generated `.cpp`/`.h` land under `<build>/generated/cppx/`, are
**gitignored, never committed**, and regenerate every build (same model
as Unreal's UHT → `Intermediate/`). Authored sources are formatted by
`tools/cppx_format.py`; the `cppx_format_check` CTest gates them
(`ctest --test-dir <build> -R cppx_format_check`).

## vcpkg dependencies

`libmodplug` was removed from `vcpkg.json`: `sdl3-mixer` no longer
builds the MOD/XM/IT music plugin. The game uses IMA ADPCM
(`sound.bin`) and MP3 (`CLOSER2.mp3`) only. Cuts build time, removes
an unused dependency.

## Build artifacts

- **Linux** — binary `silencer` (lowercase, GNU convention).
- **macOS** — `Silencer.app` bundle (`MACOSX_BUNDLE`); runtime asset
  path inside the bundle is `Contents/assets/` (loaded via
  `src/main.cpp` `CDResDir`). The Xcode project was retired — CMake
  `MACOSX_BUNDLE` is the only macOS build path.
- **Windows** — `Silencer.exe`; runtime expects `assets\` next to the
  exe (`src/os.cpp` `GetResDir`). Resources / icon are wired through
  `resources.rc` (auto-included on Windows builds).
