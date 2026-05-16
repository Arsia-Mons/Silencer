#!/usr/bin/env bash
# P16 — verifies the migrated GameJoinPanel matches the legacy GameJoinPanel.
#
# Same verification shape as the P12-P15 panel tests: migrated UI vs FRESH same-run
# legacy, step-based determinism. Both clients navigate to LOBBY then
# transition into the GameJoin surface via the test-only op
# `lobby_show_panel --panel join`. The op routes through
# `LobbyScreen::ShowGameJoin` (virtual), so both legacy and migrated UI overrides
# fire. After the swap, `step --frames 30` advances sim ticks so palette
# fade and any tick-driven animations are pinned.
#
# Pass gate: clay vs fresh-legacy diff over the right pane rect
# (x=235, y=60, w=405, h=395) is under 5.0%. Same rect as P14/P15 — covers
# the right border chrome + the three stacked Chrome+Compact buttons (Choose Tech /
# Change Team / Ready).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASELINE="$REPO_ROOT/tests/lobby-ui/baselines/gamejoin.png"
PIXDIFF="$REPO_ROOT/tools/pixdiff/build/pixdiff"

if [ -x "$REPO_ROOT/build/Silencer.app/Contents/MacOS/Silencer" ]; then
  export SILENCER_BIN="$REPO_ROOT/build/Silencer.app/Contents/MacOS/Silencer"
elif [ -x "$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer" ]; then
  export SILENCER_BIN="$REPO_ROOT/clients/silencer/build/Silencer.app/Contents/MacOS/Silencer"
fi
. "$REPO_ROOT/tests/cli-agent/e2e/lib.sh"

LOBBY_BIN="$REPO_ROOT/services/lobby/lobby"
[ -x "$LOBBY_BIN" ] || { echo "lobby binary missing at $LOBBY_BIN" >&2; exit 1; }
[ -x "$PIXDIFF" ]   || { echo "pixdiff binary missing at $PIXDIFF" >&2; exit 1; }
[ -f "$BASELINE" ]  || { echo "baseline missing at $BASELINE" >&2; exit 1; }

TMP=$(mktemp -d)
LOBBY_LOG="$TMP/lobby.log"
LOBBY_DB="$TMP/lobby.json"
mkdir -p "$TMP/maps"

LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port)
MAP_API_PORT=$(pick_port)

SILENCER_VERSION=""
for cache in \
  "$REPO_ROOT/build/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build-unity/CMakeCache.txt" \
  "$REPO_ROOT/clients/silencer/build-release/CMakeCache.txt"; do
  if [ -f "$cache" ]; then
    SILENCER_VERSION=$(awk -F= '/^SILENCER_VERSION:STRING=/{print $2}' "$cache")
    [ -n "$SILENCER_VERSION" ] && break
  fi
done
[ -z "$SILENCER_VERSION" ] && { echo "no SILENCER_VERSION in CMakeCache.txt" >&2; exit 1; }

LOBBY_PID=""
LEGACY_PID=""
UI_PID=""
LEGACY_CTRL=""
UI_CTRL=""

cleanup() {
  [ -n "$UI_PID"   ] && stop_silencer "$UI_PID"   "$UI_CTRL"   2>/dev/null || true
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
  cli --port "$port" lobby_show_panel --panel join >/dev/null
  cli --port "$port" step --frames 30 >/dev/null
}

LEGACY_CTRL=$(pick_port)
"$SILENCER_BIN" \
  --headless \
  --control-port "$LEGACY_CTRL" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-p16-legacy-$LEGACY_CTRL.log" 2>&1 &
LEGACY_PID=$!
wait_alive "$LEGACY_CTRL"
drive_to_lobby "$LEGACY_CTRL" "alice"
LEGACY_SHOT="$OUT_DIR/legacy.png"
cli --port "$LEGACY_CTRL" screenshot --out "$LEGACY_SHOT" >/dev/null
stop_silencer "$LEGACY_PID" "$LEGACY_CTRL"
LEGACY_PID=""

UI_CTRL=$(pick_port)
"$SILENCER_BIN" \
  --headless \
  --control-port "$UI_CTRL" \
  --lobby-host 127.0.0.1 \
  --lobby-port "$LOBBY_PORT" \
  >"/tmp/silencer-p16-ui-$UI_CTRL.log" 2>&1 &
UI_PID=$!
wait_alive "$UI_CTRL"
drive_to_lobby "$UI_CTRL" "bob"
UI_SHOT="${LOBBY_UI_SHOT:-$OUT_DIR/screenshot.png}"
cli --port "$UI_CTRL" screenshot --out "$UI_SHOT" >/dev/null

# Right-pane rect — same as P14/P15.
CROP="235,60,405,395"

BASELINE_LEGACY_DIFF=$("$PIXDIFF" --crop "$CROP" "$BASELINE" "$LEGACY_SHOT" 2>/dev/null || echo "n/a")
BASELINE_UI_DIFF=$("$PIXDIFF" --crop "$CROP" "$BASELINE" "$UI_SHOT" 2>/dev/null || echo "n/a")
echo "informational: legacy vs committed baseline (crop $CROP) = ${BASELINE_LEGACY_DIFF}%"
echo "informational: ui     vs committed baseline (crop $CROP) = ${BASELINE_UI_DIFF}%"

DIFF=$("$PIXDIFF" --crop "$CROP" "$LEGACY_SHOT" "$UI_SHOT")
echo "pixdiff (migrated ui vs fresh legacy, crop $CROP) = ${DIFF}%"
awk -v d="$DIFF" 'BEGIN { exit !(d+0 < 5.0) }' \
  && echo "P16 PASS (< 5.0% threshold)" \
  || { echo "P16 FAIL (>= 5.0% threshold)"; exit 1; }
