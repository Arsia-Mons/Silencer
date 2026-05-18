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
  Linux requires it exported in your shell profile. Either way it
  must contain `scripts/buildsystems/vcpkg.cmake`.
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

## Steam Deck sideload (playtesting)

Sideloading is a two-step process: build a Linux x86_64 binary on your
machine, then deploy it to the Deck over SSH with the helper script.
No Steam account, no store listing, no flatpak required.

### Prerequisites

**On the Deck** — do this once:
1. **Settings → System → Developer Mode → on**
2. **Settings → System → SSH → on** — this sets the password for the
   `deck` user.
3. Note the Deck's local IP address (**Settings → Network → … → IP address**).

**On your machine** — Docker with `linux/amd64` emulation:
```
docker buildx ls   # should show linux/amd64 as an available platform
```
If not: `docker run --platform linux/amd64 hello-world` to trigger the
emulation layer install.

### 1 — Build the Docker image (one-time setup)

```bash
# From repo root
docker build --platform linux/amd64 \
  -t silencer-linux-build \
  -f /tmp/silencer-linux-build-ctx/Dockerfile .
```

The Dockerfile (not committed — recreate from the template below if
needed) installs build tools and compiles SDL3 + SDL3_mixer from source:

```dockerfile
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build git pkg-config \
    zlib1g-dev libcurl4-openssl-dev libssl-dev \
    libminizip-dev libmpg123-dev libogg-dev libvorbis-dev libflac-dev \
    libdrm-dev libgbm-dev libudev-dev libdbus-1-dev \
    libpulse-dev libasound2-dev libpipewire-0.3-dev libwayland-dev \
    libxkbcommon-dev libx11-dev libxext-dev libxrandr-dev libxi-dev \
    libxcursor-dev libxss-dev libxinerama-dev libxxf86vm-dev libxtst-dev \
    libgl1-mesa-dev libgles2-mesa-dev libvulkan-dev \
    wayland-protocols libdecor-0-dev \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 --branch release-3.4.8 \
      https://github.com/libsdl-org/SDL.git /tmp/SDL3 && \
    cmake -S /tmp/SDL3 -B /tmp/SDL3/build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_X11_XTEST=OFF && \
    cmake --build /tmp/SDL3/build && cmake --install /tmp/SDL3/build && \
    rm -rf /tmp/SDL3

RUN git clone --depth 1 --branch release-3.2.2 \
      https://github.com/libsdl-org/SDL_mixer.git /tmp/SDL3_mixer && \
    cmake -S /tmp/SDL3_mixer -B /tmp/SDL3_mixer/build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DSDLMIXER_TESTS=OFF -DSDLMIXER_SAMPLES=OFF \
      -DSDLMIXER_MP3_MPG123=ON && \
    cmake --build /tmp/SDL3_mixer/build && \
    cmake --install /tmp/SDL3_mixer/build && rm -rf /tmp/SDL3_mixer

RUN ldconfig
WORKDIR /src
```

### 2 — Compile the Linux binary

```bash
cd clients/silencer

docker run --platform linux/amd64 --rm \
  -v "$(pwd):/src" \
  -v "$(pwd)/../../shared:/shared" \
  silencer-linux-build \
  bash -c "
    cmake -S /src -B /src/build-linux -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DSDL3_DIR=/usr/local/lib/cmake/SDL3 \
      -DSDL3_mixer_DIR=/usr/local/lib/cmake/SDL3_mixer && \
    cmake --build /src/build-linux
  "
```

Output: `clients/silencer/build-linux/silencer`

### 3 — Deploy to the Deck

```bash
# From clients/silencer/
./deploy-deck.sh <deck-ip>          # copies binary + assets
./deploy-deck.sh <deck-ip> --run    # copies and launches
```

The script uses `scp` for the binary and `rsync` for assets (incremental
— only changed files transfer after the first deploy). It will prompt for
the `deck` SSH password unless you've set up key auth.

**First deploy only** — set up passwordless SSH to avoid typing it every time:
```bash
ssh-copy-id deck@<deck-ip>
```

### 4 — Run on the Deck

From the Deck's Desktop Mode terminal (or via SSH):
```bash
cd ~/silencer
DISPLAY=:0 ./silencer
```

Or launch Game Mode after adding a non-Steam game:
1. Desktop Mode → Steam → Add a Non-Steam Game
2. Browse to `/home/deck/silencer/silencer`
3. Switch to Game Mode and launch from the library

### Rendering

The game renders at a fixed 640×480 logical canvas and upscales via the
Vulkan (SPIR-V) backend. At 1280×800 native Deck resolution the image is
pillarboxed to 1066×800 to preserve the 4:3 aspect ratio — black bars on
the left and right.

### Gamepad

The game auto-detects the Deck's gamepad on startup and switches to the
built-in `gamepad` keybind profile (lives in `shared/assets/keybinds/gamepad.json`).
Disconnecting returns to the previous keyboard profile. The mapping:

| Stick / Button | Action |
|---|---|
| Left stick / D-pad | Move |
| Right stick (diagonal) | Aim |
| A (south) | Jump |
| X (west) | Use inventory |
| B (east) | Activate / hack |
| Y (north) | Disguise |
| RT | Fire |
| LT | Detonate |
| RB | Jetpack |
| LB | Next inventory |
| Start | Cycle weapon |
| Back | Chat |

Haptic feedback fires for shoot, hit, and land events via `SDL_RumbleGamepad`.

### Known limitations (sideload phase)

- **Assets must be redeployed** when `shared/assets/` changes (the
  `rsync` in `deploy-deck.sh` handles this incrementally).
- **No auto-updater.** The in-game updater targets Windows/macOS
  installers; on Linux it logs a warning and skips.
- **SteamOS updates** may reset Developer Mode — re-enable SSH if
  a deploy fails to connect.
