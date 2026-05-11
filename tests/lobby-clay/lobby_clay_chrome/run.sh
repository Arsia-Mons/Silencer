#!/usr/bin/env bash
# P11 — verifies LobbyClayScreen chrome matches the legacy lobby chrome.
#
# Verification shape: Clay vs FRESH legacy capture, same run, same wait gate.
# Both client processes spin up against the same lobby instance, log in as the
# same account, wait the same number of frames, screenshot, and pixdiff.
#
# Why not against tests/lobby-clay/baselines/title_chrome.png? The committed
# baseline was captured at one non-reproducible palette-fade phase: re-running
# baselines/capture.sh against the legacy binary shows ~25% chrome-strip diff
# vs the committed baseline. The baseline is unreproducible by ANY
# implementation (legacy or Clay), so a fresh-legacy capture from the same
# binary in the same wall-clock regime is a strictly more faithful "before"
# reference for the design doc's <5% before/after constraint. The committed
# baseline diff is still printed below as an informational measurement.
#
# Requires:
#   - build/Silencer.app (worktree-root) or clients/silencer/build/Silencer.app
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
mkdir -p "$TMP/maps"

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)

SILENCER_VERSION=""
for d in build build-unity build-release; do
  cache="$REPO_ROOT/clients/silencer/$d/CMakeCache.txt"
  if [ -f "$cache" ]; then
    SILENCER_VERSION=$(awk -F= '/^SILENCER_VERSION:STRING=/{print $2}' "$cache")
    [ -n "$SILENCER_VERSION" ] && break
  fi
done
if [ -z "$SILENCER_VERSION" ] && [ -f "$REPO_ROOT/build/CMakeCache.txt" ]; then
  SILENCER_VERSION=$(awk -F= '/^SILENCER_VERSION:STRING=/{print $2}' "$REPO_ROOT/build/CMakeCache.txt")
fi
[ -z "$SILENCER_VERSION" ] && { echo "no SILENCER_VERSION in CMakeCache.txt" >&2; exit 1; }

LOBBY_PID=""
LEGACY_PID=""
CLAY_PID=""
LEGACY_CTRL=""
CLAY_CTRL=""

cleanup() {
  [ -n "$CLAY_PID"   ] && stop_silencer "$CLAY_PID"   "$CLAY_CTRL"   2>/dev/null || true
  [ -n "$LEGACY_PID" ] && stop_silencer "$LEGACY_PID" "$LEGACY_CTRL" 2>/dev/null || true
  if [ -n "$LOBBY_PID" ]; then
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

wait_for_iface() {
  local port="$1"
  for i in $(seq 1 100); do
    iface=$(cli --port "$port" state | bun -e \
      'const t=await new Response(Bun.stdin.stream()).text(); console.log(JSON.parse(t).current_interface_id||0);')
    [ "${iface:-0}" != "0" ] && return 0
    sleep 0.05
  done
  return 1
}

wait_for_lobby_state() {
  local port="$1" target="$2"
  for i in $(seq 1 80); do
    ls=$(cli --port "$port" state | bun -e \
      'const t=await new Response(Bun.stdin.stream()).text(); console.log(JSON.parse(t).lobby_state||"");')
    [ "$ls" = "$target" ] && return 0
    sleep 0.1
  done
  return 1
}

# drive_to_lobby <ctrl-port> <username>
# Walks MainMenu → LobbyConnect → Login → LOBBY and waits the same number of
# frames as the baseline capture. The username argument matters because two
# clients cannot log in as the same account (lobby refuses) — pick distinct
# names so both screenshots come from accounts in their own first-ever-login
# state (no map-list-prefetch differences).
drive_to_lobby() {
  local port="$1" user="$2"
  cli --port "$port" wait_for_state --state MAINMENU --timeout-ms 15000
  wait_for_iface "$port"
  cli --port "$port" click --label "Connect To Lobby" >/dev/null
  cli --port "$port" wait_for_state --state LOBBYCONNECT --timeout-ms 5000
  wait_for_iface "$port"
  cli --port "$port" set_text --uid 1 --text "$user" >/dev/null
  cli --port "$port" set_text --uid 2 --text "secret" >/dev/null
  wait_for_lobby_state "$port" AUTHENTICATING
  cli --port "$port" click --label "Login" >/dev/null
  cli --port "$port" wait_for_state --state LOBBY --timeout-ms 15000
  wait_for_iface "$port"
  # Let lobby data (map list, character info) arrive over TCP — wall-clock
  # gated, non-deterministic, but converged by the time wait_frames returns.
  cli --port "$port" wait_frames --n 60 >/dev/null
  # Then advance the sim by EXACTLY 30 sim-ticks via the `step` op
  # (controldispatch.cpp:656). `step` pauses the catch-up loop and decrements
  # stepFramesRemaining once per sim tick, completing when stepFramesRemaining
  # hits 0. Sim-tick-deterministic — does NOT depend on the screen's render
  # cost, unlike `wait_frames` which is wall-clock-gated. Both legacy and
  # Clay land at the same fade_i (saturated at 16, since 30 > 16) and the
  # same animation phase for every world-object whose update is tick-driven.
  cli --port "$port" step --frames 30 >/dev/null
}

# Capture the LEGACY chrome first (no env var). Same wait as the Clay capture.
LEGACY_HOME="$TMP/home-legacy"
mkdir -p "$LEGACY_HOME"
LEGACY_CTRL=$(pick_port)
HOME="$LEGACY_HOME" "$SILENCER_BIN" \
  --headless \
  --control-port "$LEGACY_CTRL" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-p11-legacy-$LEGACY_CTRL.log" 2>&1 &
LEGACY_PID=$!
wait_alive "$LEGACY_CTRL"
drive_to_lobby "$LEGACY_CTRL" "alice"
LEGACY_SHOT="$OUT_DIR/legacy.png"
cli --port "$LEGACY_CTRL" screenshot --out "$LEGACY_SHOT" >/dev/null
stop_silencer "$LEGACY_PID" "$LEGACY_CTRL"
LEGACY_PID=""

# Capture the CLAY chrome (SILENCER_LOBBY_CLAY=1) immediately after. Distinct
# username + distinct HOME so the two captures are independent except for the
# SILENCER_LOBBY_CLAY flag.
CLAY_HOME="$TMP/home-clay"
mkdir -p "$CLAY_HOME"
CLAY_CTRL=$(pick_port)
HOME="$CLAY_HOME" SILENCER_LOBBY_CLAY=1 "$SILENCER_BIN" \
  --headless \
  --control-port "$CLAY_CTRL" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-p11-clay-$CLAY_CTRL.log" 2>&1 &
CLAY_PID=$!
wait_alive "$CLAY_CTRL"
drive_to_lobby "$CLAY_CTRL" "bob"
CLAY_SHOT="${LOBBY_CLAY_SHOT:-$OUT_DIR/screenshot.png}"
cli --port "$CLAY_CTRL" screenshot --out "$CLAY_SHOT" >/dev/null

# Chrome strip = top 60 px, animation-free in both implementations
# (bg + "Silencer" title + version + map-name overlay + "Go Back" button).
CROP="0,0,640,60"

# Informational: legacy-vs-committed-baseline (expected ~17–27%, see notes).
BASELINE_LEGACY_DIFF=$("$PIXDIFF" --crop "$CROP" "$BASELINE" "$LEGACY_SHOT" 2>/dev/null || echo "n/a")
BASELINE_CLAY_DIFF=$("$PIXDIFF" --crop "$CROP" "$BASELINE" "$CLAY_SHOT" 2>/dev/null || echo "n/a")
echo "informational: legacy vs committed baseline (crop $CROP) = ${BASELINE_LEGACY_DIFF}%"
echo "informational: clay   vs committed baseline (crop $CROP) = ${BASELINE_CLAY_DIFF}%"

# Pass gate: Clay vs fresh-legacy, same run.
DIFF=$("$PIXDIFF" --crop "$CROP" "$LEGACY_SHOT" "$CLAY_SHOT")
echo "pixdiff (clay vs fresh legacy, crop $CROP) = ${DIFF}%"
awk -v d="$DIFF" 'BEGIN { exit !(d+0 < 1.0) }' \
  && echo "P11 PASS (< 1.0% threshold)" \
  || { echo "P11 FAIL (>= 1.0% threshold)"; exit 1; }
