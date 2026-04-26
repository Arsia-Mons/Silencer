#!/usr/bin/env bash
# Build the Silencer client on macOS, pointed at a local lobby server.
#
# Usage:
#   ./infra/scripts/build-mac-local.sh               # lobby at 127.0.0.1:15170
#   LOBBY_HOST=1.2.3.4 ./infra/scripts/build-mac-local.sh
#   LOBBY_PORT=517      ./infra/scripts/build-mac-local.sh
#
# The lobby host and port are baked into the binary at compile time.
# Run `docker compose -f infra/docker-compose.yml up -d` first so the lobby is ready before launching.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LOBBY_HOST="${LOBBY_HOST:-127.0.0.1}"
LOBBY_PORT="${LOBBY_PORT:-15170}"
BUILD_DIR="$REPO_ROOT/build"

echo "==> Installing/updating dependencies via Homebrew"
brew install cmake sdl2 sdl2_mixer minizip 2>/dev/null || brew upgrade sdl2 sdl2_mixer minizip 2>/dev/null || true

echo "==> Configuring (lobby=${LOBBY_HOST}:${LOBBY_PORT})"
cmake -B "$BUILD_DIR" -S "$REPO_ROOT/clients/silencer" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DSILENCER_LOBBY_HOST="$LOBBY_HOST" \
  -DSILENCER_LOBBY_PORT="$LOBBY_PORT"

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
