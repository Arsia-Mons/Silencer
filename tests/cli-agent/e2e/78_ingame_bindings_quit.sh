#!/usr/bin/env bash
# In-game hardcoded gameplay bindings vs origin (game_input.cpp
# OnScancodeDown/Up):
#   - F1 player list is HOLD semantics: down -> SetShowingPlayerList(true),
#     up -> false.
#   - F2 toggles team colors (SetShowingTeamColors flip per press).
#   - F4 shows the "          *MUSIC PAUSED*" top ticker; headless audio is
#     disabled so MusicPaused() is always false and a second press shows the
#     SAME text (origin behavior).
# Raw scancodes go over the TUI input port (these gameplay shortcut keys still
# flow through the OS-scancode handlers). The old ESC raw-scancode quit-prompt
# state machine (world.quitstate) was DELETED — cancel/back now routes through
# the UI-layer cancel router (use_cancel) and is covered by
# 91_cancel_back_router.sh via the control `key`/`back` ops.
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT="$(pick_port)"
FRAME_PORT="$(pick_port)"
INPUT_PORT="$(pick_port)"

bun -e '
  Bun.listen({ hostname: "127.0.0.1", port: Number(process.argv[1]), socket: { data() {} } });
  setInterval(() => {}, 60_000);
' "$FRAME_PORT" &
SINK_PID=$!

SILENCER_TUI_FRAME_HOST=127.0.0.1 SILENCER_TUI_FRAME_PORT="$FRAME_PORT" \
  "$SILENCER_BIN" --headless --tui --tui-input-port "$INPUT_PORT" \
  --control-port "$PORT" >"/tmp/silencer-e2e-$PORT.log" 2>&1 &
PID=$!
cleanup() {
  stop_silencer "$PID" "$PORT" || true
  kill "$SINK_PID" 2>/dev/null || true
}
trap cleanup EXIT
wait_alive "$PORT"

# send_scancodes <byte_index> <byte_value>: one scancode-bitmask snapshot
# (latest-wins). F1=58 -> byte7 bit2; F2=59 -> byte7 bit3; F4=61 -> byte7
# bit5; ESC=41 -> byte5 bit1; RETURN=40 -> byte5 bit0.
send_scancodes() {
  bun -e '
    const [port, bi, bv] = process.argv.slice(1).map(Number);
    const mask = new Uint8Array(64);
    mask[bi] = bv;
    const frame = new Uint8Array(1 + 3 + 64);
    frame[0] = 1; frame[1] = 2; frame[2] = 64; frame[3] = 0;
    frame.set(mask, 4);
    const sock = await Bun.connect({ hostname: "127.0.0.1", port,
      socket: { data() {}, error(_s, e) { console.error(e); process.exit(1); } } });
    sock.write(frame);
    await sock.flush?.();
    setTimeout(() => { sock.end(); process.exit(0); }, 150);
  ' "$INPUT_PORT" "$1" "$2"
}

ws_field() { # ws_field <name>
  cli --port "$PORT" world_state | bun -e \
    'const s=JSON.parse(await new Response(Bun.stdin.stream()).text());const r=s.result??s;console.log(r[process.argv[1]])' "$1"
}
mode_field() {
  cli --port "$PORT" ingame_ui_mode --mode status | bun -e \
    'const s=JSON.parse(await new Response(Bun.stdin.stream()).text());const r=s.result??s;console.log(r[process.argv[1]])' "$1"
}
expect() {
  if [ "$2" != "$3" ]; then echo "FAIL 78: $1 = $2, want $3" >&2; exit 1; fi
}
tap() { # tap <byte_index> <byte_value> — press one tick, release one tick
  send_scancodes "$1" "$2"
  cli --port "$PORT" step --frames 1 >/dev/null
  send_scancodes "$1" 0
  cli --port "$PORT" step --frames 1 >/dev/null
}

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$PORT" click --label Tutorial >/dev/null
cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 15000 >/dev/null
cli --port "$PORT" pause >/dev/null
for _ in $(seq 1 100); do
  if cli --port "$PORT" world_state | bun -e '
    const s=JSON.parse(await new Response(Bun.stdin.stream()).text());
    const r=s.result??s; process.exit((r.players?.length??0)>0?0:1);'; then break; fi
  cli --port "$PORT" step --frames 4 >/dev/null
done

# --- F1 hold semantics --------------------------------------------------------
expect "player list initially hidden" "$(mode_field show_player_list)" false
send_scancodes 7 4
cli --port "$PORT" step --frames 1 >/dev/null
expect "F1 down shows the player list" "$(mode_field show_player_list)" true
send_scancodes 7 0
cli --port "$PORT" step --frames 1 >/dev/null
expect "F1 up hides the player list" "$(mode_field show_player_list)" false

# --- F2 toggle ----------------------------------------------------------------
expect "team colors initially off" "$(ws_field show_team_colors)" false
tap 7 8
expect "F2 toggles team colors on" "$(ws_field show_team_colors)" true
tap 7 8
expect "F2 toggles team colors off" "$(ws_field show_team_colors)" false

# --- F4 ticker ----------------------------------------------------------------
tap 7 32
expect "F4 ticker text" "$(ws_field topmessage_text)" "          *MUSIC PAUSED*"
prog="$(ws_field topmessage_progress)"
[ "$prog" -gt 0 ] || { echo "FAIL 78: ticker progress $prog not > 0" >&2; exit 1; }
tap 7 32
expect "F4 again (audio disabled => never paused)" \
  "$(ws_field topmessage_text)" "          *MUSIC PAUSED*"

# --- mission-over signal is exposed (replaces the old quit_state machine) -----
# The control-socket world_state now reports the mission-over bool (END_MISSION),
# not the deleted quitstate machine. Idle in a fresh tutorial it must be false.
expect "mission_over idle" "$(ws_field mission_over)" false

echo "PASS 78_ingame_bindings_quit"
