#!/usr/bin/env bash
# Visual regression: lobby cluster (connect -> character create -> lobby) vs
# the ORIGIN goldens in golden/ (ground truth — never bless-able). Gated by
# tools/cap/pixdiff_tolerant.py's printed verdict. Captures at 1920x1080.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

SCRIPT_DIR="$(dirname "$0")"
LOBBY_BIN="$(lobby_bin)"
TOLERANT="$REPO_ROOT/tools/cap/pixdiff_tolerant.py"
GOLDEN_DIR="$SCRIPT_DIR/golden"
W=1920; H=1080

TMP=$(mktemp -d)
LOBBY_DB="$TMP/lobby.json"; SILENCER_HOME="$TMP/home"; mkdir -p "$SILENCER_HOME"
# The lobby port is PINNED to the origin golden capture's (the connect log
# renders "Connecting to 127.0.0.1:<port>"); random ports can never reach
# pixel parity. Falls back to a random port if 63532 is taken.
LOBBY_PORT=63532
(echo > /dev/tcp/127.0.0.1/$LOBBY_PORT) 2>/dev/null && LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port); MAP_API_PORT=$(pick_port); CTRL_PORT=$(pick_port)
SILENCER_VERSION="$(silencer_version)"
WORK="$TMP/work"; mkdir -p "$WORK"
FAIL=0

cleanup() {
  [ -n "${SILENCER_PID:-}" ] && stop_silencer "$SILENCER_PID" "$CTRL_PORT" || true
  [ -n "${LOBBY_PID:-}" ] && { kill "$LOBBY_PID" 2>/dev/null || true; wait "$LOBBY_PID" 2>/dev/null || true; }
  rm -rf "$TMP"
}
trap cleanup EXIT

"$LOBBY_BIN" -addr ":$LOBBY_PORT" -db "$LOBBY_DB" -version "$SILENCER_VERSION" \
  -game-binary "$SILENCER_BIN" -maps-dir "$TMP/maps" \
  -player-auth-addr ":$PLAYER_AUTH_PORT" -map-api-addr ":$MAP_API_PORT" >"$TMP/lobby.log" 2>&1 &
LOBBY_PID=$!
for i in $(seq 1 60); do (echo > "/dev/tcp/127.0.0.1/$LOBBY_PORT") 2>/dev/null && break; sleep 0.25
  [ "$i" = 60 ] && { echo "lobby never came up" >&2; cat "$TMP/lobby.log" >&2; exit 1; }; done

HOME="$SILENCER_HOME" "$SILENCER_BIN" --headless --control-port "$CTRL_PORT" \
  --lobby-host 127.0.0.1 --lobby-port "$LOBBY_PORT" >"/tmp/silencer-e2e-$CTRL_PORT.log" 2>&1 &
SILENCER_PID=$!
wait_alive "$CTRL_PORT"

wait_for_widget() {
  for i in $(seq 1 200); do
    found=$(cli --port "$CTRL_PORT" inspect | LABEL="$1" bun -e \
      'const r=JSON.parse(await new Response(Bun.stdin.stream()).text());const l=process.env.LABEL;
       console.log((r.nodes||[]).some((w)=>w.label===l||w.control_id===l)?"yes":"no")' 2>/dev/null || echo no)
    [ "$found" = yes ] && return 0; sleep 0.05
  done; echo "widget '$1' never appeared" >&2; return 1
}
wait_for_lobby_state() {
  for i in $(seq 1 80); do
    ls=$(cli --port "$CTRL_PORT" state | bun -e 'console.log(JSON.parse(await new Response(Bun.stdin.stream()).text()).lobby_state||"")')
    [ "$ls" = "$1" ] && return 0; sleep 0.1
  done; echo "lobby_state never became $1 (last=$ls)" >&2; return 1
}

cap() {
  local name="$1"
  cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null
  local png="$WORK/$name.png" golden="$GOLDEN_DIR/$name.png"
  cli --port "$CTRL_PORT" screenshot --out "$png" >/dev/null
  [ -f "$golden" ] || { echo "  MISSING origin golden: $name.png" >&2; FAIL=$((FAIL+1)); return 0; }
  local line
  line="$(python3 "$TOLERANT" "$png" "$golden" | head -1)"
  case "$line" in
    *PASS*) ;;
    *) echo "  DIVERGED $name vs origin golden: $line" >&2; FAIL=$((FAIL+1)) ;;
  esac
}

cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$CTRL_PORT" resize --w "$W" --h "$H" >/dev/null
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wait_for_widget "Username"
cap lobby_connect

for ch in a l i c e; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
wait_for_lobby_state AUTHENTICATING
cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
wait_for_widget "Create New Character"
cap character_create

# Alias step: Enter on the focused input submits (origin has no Continue button).
cli --port "$CTRL_PORT" click --label "Create New Character" >/dev/null
wait_for_widget "Alias"
cli --port "$CTRL_PORT" set_text --label "Alias" --text "Alice" >/dev/null
cap cc_alias
cli --port "$CTRL_PORT" key --key enter >/dev/null
wait_for_widget "Black Rose"
cap cc_select_agency
cli --port "$CTRL_PORT" click --label "Noxis" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null
wait_for_widget "Agents"
cap lobby_screen

if [ "$FAIL" -ne 0 ]; then
  echo "71_visual_regression_lobby: $FAIL surface(s) diverged"
  exit 1
fi
echo "PASS 71_visual_regression_lobby"
