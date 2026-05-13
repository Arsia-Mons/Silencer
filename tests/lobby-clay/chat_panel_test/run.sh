#!/usr/bin/env bash
# P13 — verifies the Clay ChatPanel matches the legacy ChatPanel.
#
# Same verification shape as character_panel_test/run.sh (P12) — Clay vs
# FRESH same-run legacy, step-based determinism. The committed
# tests/lobby-clay/baselines/chat.png is non-reproducible (see P11-retry
# entry in progress.txt) so we use a same-run legacy capture as the
# "before" reference.
#
# Pass gate: clay vs fresh-legacy diff over the chat-panel rect
# (x=10, y=195, w=378, h=260) is under 5.0%. Rect covers the channel
# name header (15, 200), chat scrollback (19, 220, 242x207), presence
# list (267, 220, 110x207), and chat input (18, 437, 360x14).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASELINE="$REPO_ROOT/tests/lobby-clay/baselines/chat.png"
PIXDIFF="$REPO_ROOT/tools/pixdiff/build/pixdiff"

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

wait_for_widget() {
  local port="$1" label="$2"
  for i in $(seq 1 100); do
    found=$(cli --port "$port" inspect | LABEL="$label" bun -e 'const t=await new Response(Bun.stdin.stream()).text(); const r=JSON.parse(t); const label=process.env.LABEL; console.log(r.widgets.some((w)=>w.label===label) ? "yes" : "no");' 2>/dev/null || echo no)
    [ "$found" = "yes" ] && return 0
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

drive_to_lobby() {
  local port="$1" user="$2"
  cli --port "$port" wait_for_state --state MAINMENU --timeout-ms 15000
  wait_for_widget "$port" "Connect To Lobby"
  cli --port "$port" click --label "Connect To Lobby" >/dev/null
  cli --port "$port" wait_for_state --state LOBBYCONNECT --timeout-ms 5000
  wait_for_widget "$port" "Login"
  cli --port "$port" set_text --uid 1 --text "$user" >/dev/null
  cli --port "$port" set_text --uid 2 --text "secret" >/dev/null
  wait_for_lobby_state "$port" AUTHENTICATING
  cli --port "$port" click --label "Login" >/dev/null
  cli --port "$port" wait_for_state --state LOBBY --timeout-ms 15000
  wait_for_widget "$port" "Create Game"
  cli --port "$port" wait_frames --n 60 >/dev/null
  cli --port "$port" step --frames 30 >/dev/null
}

LEGACY_HOME="$TMP/home-legacy"
mkdir -p "$LEGACY_HOME"
LEGACY_CTRL=$(pick_port)
HOME="$LEGACY_HOME" "$SILENCER_BIN" \
  --headless \
  --control-port "$LEGACY_CTRL" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-p13-legacy-$LEGACY_CTRL.log" 2>&1 &
LEGACY_PID=$!
wait_alive "$LEGACY_CTRL"
drive_to_lobby "$LEGACY_CTRL" "alice"
LEGACY_SHOT="$OUT_DIR/legacy.png"
cli --port "$LEGACY_CTRL" screenshot --out "$LEGACY_SHOT" >/dev/null
stop_silencer "$LEGACY_PID" "$LEGACY_CTRL"
LEGACY_PID=""

CLAY_HOME="$TMP/home-clay"
mkdir -p "$CLAY_HOME"
CLAY_CTRL=$(pick_port)
HOME="$CLAY_HOME" SILENCER_LOBBY_CLAY=1 "$SILENCER_BIN" \
  --headless \
  --control-port "$CLAY_CTRL" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-p13-clay-$CLAY_CTRL.log" 2>&1 &
CLAY_PID=$!
wait_alive "$CLAY_CTRL"
drive_to_lobby "$CLAY_CTRL" "bob"
CLAY_SHOT="${LOBBY_CLAY_SHOT:-$OUT_DIR/screenshot.png}"
cli --port "$CLAY_CTRL" screenshot --out "$CLAY_SHOT" >/dev/null

# Chat panel rect: channel header (15,200) + chat box (19,220,242x207) +
# presence (267,220,110x207) + input (18,437,360x14). Slightly padded.
CROP="10,195,378,260"

BASELINE_LEGACY_DIFF=$("$PIXDIFF" --crop "$CROP" "$BASELINE" "$LEGACY_SHOT" 2>/dev/null || echo "n/a")
BASELINE_CLAY_DIFF=$("$PIXDIFF" --crop "$CROP" "$BASELINE" "$CLAY_SHOT" 2>/dev/null || echo "n/a")
echo "informational: legacy vs committed baseline (crop $CROP) = ${BASELINE_LEGACY_DIFF}%"
echo "informational: clay   vs committed baseline (crop $CROP) = ${BASELINE_CLAY_DIFF}%"

DIFF=$("$PIXDIFF" --crop "$CROP" "$LEGACY_SHOT" "$CLAY_SHOT")
echo "pixdiff (clay vs fresh legacy, crop $CROP) = ${DIFF}%"
awk -v d="$DIFF" 'BEGIN { exit !(d+0 < 5.0) }' \
  && echo "P13 PASS (< 5.0% threshold)" \
  || { echo "P13 FAIL (>= 5.0% threshold)"; exit 1; }
