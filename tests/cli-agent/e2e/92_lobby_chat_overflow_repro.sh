#!/usr/bin/env bash
# Repro/regression for the cppx draw-pipeline totality defect: spamming the lobby
# chat past the per-text-node wrapped-line ceiling used to abort the whole frame
# transcribe, dropping every sibling panel painted after the chat (the "Active
# Games" box, borders, text) and leaving an unbalanced layer/clip in the IR.
#
# Signal: the build-draw-command-list failure surfaces as the log line
#   "client/ui: failed to update retained runtime"
# It must be ABSENT before the spam and stay ABSENT after (totality: a frame is
# always presentable; content overflow degrades, never fails the frame).
#
# Captures lobby_before/after.png at 1920x1080 for visual confirmation that the
# Active Games panel persists.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

LOBBY_BIN="$(lobby_bin)"
W=1920; H=1080
# Short messages: each is its own '\n'-separated paragraph = one wrapped line.
# ~120 of them pack the retained node's 640-byte value cap with ~75 wrapped
# lines, well past the 32-line text ceiling (a long-message spam never fits
# enough lines into the value cap to overflow).
NMSG="${NMSG:-120}"

TMP=$(mktemp -d)
LOBBY_DB="$TMP/lobby.json"; SILENCER_HOME="$TMP/home"; mkdir -p "$SILENCER_HOME"
export SILENCER_LOBBYD_DIR="$TMP/lobbyd"; mkdir -p "$SILENCER_LOBBYD_DIR"
LOBBY_PORT=$(pick_port)
PLAYER_AUTH_PORT=$(pick_port); MAP_API_PORT=$(pick_port); CTRL_PORT=$(pick_port)
SILENCER_VERSION="$(silencer_version)"
WORK="${WORK:-$TMP/work}"; mkdir -p "$WORK"
GAME_LOG="/tmp/silencer-e2e-$CTRL_PORT.log"

cleanup() {
  cli --port "$CTRL_PORT" >/dev/null 2>&1 <<<'' || true
  bun "$REPO_ROOT/clients/cli/index.ts" lobby kill --all >/dev/null 2>&1 || true
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
  --lobby-host 127.0.0.1 --lobby-port "$LOBBY_PORT" >"$GAME_LOG" 2>&1 &
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

# --- drive game into the lobby (mirrors 71_visual_regression_lobby) ---
cli --port "$CTRL_PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$CTRL_PORT" resize --w "$W" --h "$H" >/dev/null
cli --port "$CTRL_PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 5000 >/dev/null
wait_for_widget "Username"
wait_for_lobby_state AUTHENTICATING 2>/dev/null || true
for ch in a l i c e; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
cli --port "$CTRL_PORT" key --key tab >/dev/null
for ch in s e c r e t; do cli --port "$CTRL_PORT" key --key "$ch" >/dev/null; done
cli --port "$CTRL_PORT" click --label "Login/Create" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state CREATECHARACTER --timeout-ms 15000 >/dev/null
wait_for_widget "Create New Character"
cli --port "$CTRL_PORT" click --label "Create New Character" >/dev/null
wait_for_widget "Alias"
cli --port "$CTRL_PORT" set_text --label "Alias" --text "Alice" >/dev/null
cli --port "$CTRL_PORT" key --key enter >/dev/null
wait_for_widget "Noxis"
cli --port "$CTRL_PORT" click --label "Noxis" >/dev/null
cli --port "$CTRL_PORT" wait_for_state --state LOBBY --timeout-ms 15000 >/dev/null
wait_for_widget "Agents"
cli --port "$CTRL_PORT" wait_frames --n 45 >/dev/null # let the screen fade-in settle
cli --port "$CTRL_PORT" screenshot --out "$WORK/lobby_before.png" >/dev/null

BEFORE=$(grep -c "failed to update retained runtime" "$GAME_LOG" || true)
echo "before: retained-runtime failures in log = $BEFORE"

# --- spam the Lobby channel from a fake player ---
bun "$REPO_ROOT/clients/cli/index.ts" lobby spawn --as bob --host 127.0.0.1 \
  --port "$LOBBY_PORT" --version "$SILENCER_VERSION" --user bob --pass bob >/dev/null
bun "$REPO_ROOT/clients/cli/index.ts" lobby join_channel --as bob --channel Lobby >/dev/null
for i in $(seq 1 "$NMSG"); do
  bun "$REPO_ROOT/clients/cli/index.ts" lobby chat --as bob --channel Lobby \
    --text "m$i" >/dev/null
done

cli --port "$CTRL_PORT" wait_frames --n 30 >/dev/null
cli --port "$CTRL_PORT" screenshot --out "$WORK/lobby_after.png" >/dev/null

AFTER=$(grep -c "failed to update retained runtime" "$GAME_LOG" || true)
echo "after:  retained-runtime failures in log = $AFTER"
echo "screens: $WORK/lobby_before.png  $WORK/lobby_after.png"

if [ "$AFTER" -gt "$BEFORE" ]; then
  echo "REPRO: chat spam drove $((AFTER - BEFORE)) frame-transcribe failures (panels drop). DEFECT PRESENT."
  exit 1
fi
echo "PASS 92_lobby_chat_overflow_repro (no frame-transcribe failure under chat spam)"
