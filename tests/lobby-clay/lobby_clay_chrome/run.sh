#!/usr/bin/env bash
# P11 — boots silencer with SILENCER_LOBBY_CLAY=1, drives main menu → lobby
# connect → login → LOBBY, screenshots, and pixdiffs against the legacy
# baseline at tests/lobby-clay/baselines/title_chrome.png.
#
# Requires:
#   - clients/silencer/build/Silencer.app (or worktree-root build/)
#   - services/lobby/lobby
#   - tools/pixdiff/build/pixdiff
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASELINE="$REPO_ROOT/tests/lobby-clay/baselines/title_chrome.png"
PIXDIFF="$REPO_ROOT/tools/pixdiff/build/pixdiff"

# Per progress.txt: the cli-agent harness auto-detects the binary at
# `clients/silencer/build/Silencer.app/...` first, so explicitly point
# SILENCER_BIN at the worktree-root build path BEFORE sourcing lib.sh.
if [ -x "$REPO_ROOT/build/Silencer.app/Contents/MacOS/Silencer" ]; then
  export SILENCER_BIN="$REPO_ROOT/build/Silencer.app/Contents/MacOS/Silencer"
elif [ -x "$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer" ]; then
  export SILENCER_BIN="$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer"
fi
. "$REPO_ROOT/tests/cli-agent/e2e/lib.sh"

LOBBY_BIN="$REPO_ROOT/services/lobby/lobby"
[ -x "$LOBBY_BIN" ] || { echo "lobby binary missing at $LOBBY_BIN" >&2; exit 1; }
[ -x "$PIXDIFF" ] || { echo "pixdiff binary missing at $PIXDIFF" >&2; exit 1; }
[ -f "$BASELINE" ] || { echo "baseline missing at $BASELINE" >&2; exit 1; }

TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
SILENCER_HOME="$TMP/home"
mkdir -p "$SILENCER_HOME"

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)
CTRL_PORT=$(pick_port)

SILENCER_VERSION=""
for d in build build-unity build-release; do
  cache="$REPO_ROOT/clients/silencer/$d/CMakeCache.txt"
  if [ -f "$cache" ]; then
    SILENCER_VERSION=$(awk -F= '/^SILENCER_VERSION:STRING=/{print $2}' "$cache")
    [ -n "$SILENCER_VERSION" ] && break
  fi
done
# Fall back to worktree-root build/.
if [ -z "$SILENCER_VERSION" ] && [ -f "$REPO_ROOT/build/CMakeCache.txt" ]; then
  SILENCER_VERSION=$(awk -F= '/^SILENCER_VERSION:STRING=/{print $2}' "$REPO_ROOT/build/CMakeCache.txt")
fi
[ -z "$SILENCER_VERSION" ] && { echo "no SILENCER_VERSION in CMakeCache.txt" >&2; exit 1; }

cleanup() {
  if [ -n "${SILENCER_PID:-}" ]; then
    stop_silencer "$SILENCER_PID" "$CTRL_PORT" || true
  fi
  if [ -n "${LOBBY_PID:-}" ]; then
    kill "$LOBBY_PID" 2>/dev/null || true
    wait "$LOBBY_PID" 2>/dev/null || true
  fi
  rm -rf "$TMP"
}
trap cleanup EXIT

"$LOBBY_BIN" \
  -addr ":$LOBBY_PORT" \
  -db "$LOBBY_DB" \
  -version "$SILENCER_VERSION" \
  -game-binary "$SILENCER_BIN" \
  -maps-dir "$TMP/maps" \
  -player-auth-addr ":$PLAYER_AUTH_PORT" \
  -map-api-addr ":$MAP_API_PORT" \
  >"$LOBBY_LOG" 2>&1 &
LOBBY_PID=$!

for i in $(seq 1 60); do
  if (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null; then break; fi
  sleep 0.25
  if [ "$i" = 60 ]; then
    echo "lobby on :$LOBBY_PORT never came up" >&2
    cat "$LOBBY_LOG" >&2; exit 1
  fi
done

# The Clay-lobby opt-in flag — the whole reason for this scenario.
HOME="$SILENCER_HOME" SILENCER_LOBBY_CLAY=1 "$SILENCER_BIN" \
  --headless \
  --control-port "$CTRL_PORT" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-p11-$CTRL_PORT.log" 2>&1 &
SILENCER_PID=$!
wait_alive "$CTRL_PORT"

wait_for_iface() {
  for i in $(seq 1 100); do
    iface=$(cli --port "$CTRL_PORT" state | bun -e \
      'const t=await new Response(Bun.stdin.stream()).text(); console.log(JSON.parse(t).current_interface_id||0);')
    [ "${iface:-0}" != "0" ] && return 0
    sleep 0.05
  done
  return 1
}

wait_for_lobby_state() {
  local target="$1"
  for i in $(seq 1 80); do
    ls=$(cli --port "$CTRL_PORT" state | bun -e \
      'const t=await new Response(Bun.stdin.stream()).text(); console.log(JSON.parse(t).lobby_state||"");')
    [ "$ls" = "$target" ] && return 0
    sleep 0.1
  done
  return 1
}

# MainMenu → LobbyConnect → Login → LOBBY
cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000
wait_for_iface
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000
wait_for_iface
cli --port "$CTRL_PORT" set_text --uid 1 --text "alice" >/dev/null
cli --port "$CTRL_PORT" set_text --uid 2 --text "secret" >/dev/null
wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000
wait_for_iface

# Let the lobby pump a few frames so panel content stabilizes.
cli --port "$CTRL_PORT" wait_frames --n 30 >/dev/null

SHOT="${LOBBY_CLAY_SHOT:-$OUT_DIR/screenshot.png}"
cli --port "$CTRL_PORT" screenshot --out "$SHOT" >/dev/null

DIFF=$("$PIXDIFF" "$BASELINE" "$SHOT")
echo "pixdiff = ${DIFF}%"
# Round-down comparison: 0.99 passes, 1.00 fails.
awk -v d="$DIFF" 'BEGIN { exit !(d+0 < 1.0) }' \
  && echo "P11 PASS (< 1.0% threshold)" \
  || { echo "P11 FAIL (>= 1.0% threshold)"; exit 1; }
