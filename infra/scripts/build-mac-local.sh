#!/usr/bin/env bash
# Build the Silencer client on macOS, pointed at a local lobby server.
#
# Usage:
#   ./infra/scripts/build-mac-local.sh               # lobby at 127.0.0.1:15170
#   LOBBY_HOST=1.2.3.4 ./infra/scripts/build-mac-local.sh
#   LOBBY_PORT=517      ./infra/scripts/build-mac-local.sh
#   VERSION=00040       ./infra/scripts/build-mac-local.sh  # must match lobby -version flag
#
# The lobby host, port, and version are baked into the binary at compile time.
# Run `docker compose -f infra/docker-compose.yml up -d` first so the lobby is ready before launching.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LOBBY_HOST="${LOBBY_HOST:-127.0.0.1}"
LOBBY_PORT="${LOBBY_PORT:-15170}"
VERSION="${VERSION:-00040}"
BUILD_DIR="$REPO_ROOT/build"

echo "==> Installing/updating dependencies via Homebrew"
brew install cmake sdl3 minizip 2>/dev/null || brew upgrade sdl3 minizip 2>/dev/null || true

# Homebrew has no sdl3_mixer formula. Build it from source into a local
# prefix the cmake configure step picks up via CMAKE_PREFIX_PATH. MP3-only
# (drmp3) matches the audio formats the game ships. Cached so subsequent
# runs skip the build.
SDL3_MIXER_PREFIX="$HOME/.cache/silencer/sdl3-mixer"
if [ ! -f "$SDL3_MIXER_PREFIX/lib/libSDL3_mixer.dylib" ]; then
  echo "==> Building SDL3_mixer (one-time, cached at $SDL3_MIXER_PREFIX)"
  SRC_DIR="$(mktemp -d)/SDL_mixer"
  git clone --depth 1 --branch release-3.2.0 https://github.com/libsdl-org/SDL_mixer.git "$SRC_DIR"
  cmake -S "$SRC_DIR" -B "$SRC_DIR/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_INSTALL_PREFIX="$SDL3_MIXER_PREFIX" \
    -DBUILD_SHARED_LIBS=ON -DSDLMIXER_VENDORED=OFF \
    -DSDLMIXER_SAMPLES=OFF -DSDLMIXER_OPUS=OFF -DSDLMIXER_FLAC=OFF \
    -DSDLMIXER_MOD=OFF -DSDLMIXER_MIDI=OFF -DSDLMIXER_VORBIS=OFF \
    -DSDLMIXER_GME=OFF -DSDLMIXER_WAVPACK=OFF \
    -DSDLMIXER_MP3_DRMP3=ON -DSDLMIXER_MP3_MPG123=OFF
  cmake --build "$SRC_DIR/build" -j"$(sysctl -n hw.ncpu)" --target install
fi

echo "==> Configuring (lobby=${LOBBY_HOST}:${LOBBY_PORT}, version=${VERSION})"
cmake -B "$BUILD_DIR" -S "$REPO_ROOT/clients/silencer" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCMAKE_PREFIX_PATH="$SDL3_MIXER_PREFIX;/opt/homebrew" \
  -DSILENCER_LOBBY_HOST="$LOBBY_HOST" \
  -DSILENCER_LOBBY_PORT="$LOBBY_PORT" \
  -DSILENCER_VERSION="$VERSION"

echo "==> Building ($(sysctl -n hw.ncpu) cores)"
cmake --build "$BUILD_DIR" --config Release -j"$(sysctl -n hw.ncpu)"

APP="$BUILD_DIR/Silencer.app"
if [ -d "$APP" ]; then
  echo ""
  echo "Build complete."
  echo "    App bundle : $APP"
  echo "    Launch with: open $APP"
  echo "    Or run directly: $APP/Contents/MacOS/Silencer"
else
  echo ""
  echo "Build complete: $BUILD_DIR/silencer"
fi
