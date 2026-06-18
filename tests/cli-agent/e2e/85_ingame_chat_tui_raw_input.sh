#!/usr/bin/env bash
# SIL-197: raw TUI scancodes must feed the retained in-game chat input.
# The control `key` op only injects cppx UI input; this test uses
# --tui-input-port scancode snapshots to cover the real terminal keyboard path.
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

# send_scancode <scancode|-1>: one latest-wins scancode-bitmask snapshot.
# KEY:T=23 (byte 2 bit 7), H=11, I=12, ESC=41.
send_scancode() {
  bun -e '
    const port = Number(process.argv[1]);
    const sc = Number(process.argv[2]);
    const mask = new Uint8Array(64);
    if (sc >= 0) mask[sc >> 3] = 1 << (sc & 7);
    const frame = new Uint8Array(1 + 3 + 64);
    frame[0] = 1; frame[1] = 2; frame[2] = 64; frame[3] = 0;
    frame.set(mask, 4);
    const sock = await Bun.connect({ hostname: "127.0.0.1", port,
      socket: { data() {}, error(_s, e) { console.error(e); process.exit(1); } } });
    sock.write(frame);
    await sock.flush?.();
    setTimeout(() => { sock.end(); process.exit(0); }, 150);
  ' "$INPUT_PORT" "$1"
}

tap_scancode() {
  send_scancode "$1"
  cli --port "$PORT" step --frames 1 >/dev/null
  send_scancode -1
  cli --port "$PORT" step --frames 1 >/dev/null
}

status() { cli --port "$PORT" ingame_ui_mode --mode status; }
field() {
  bun -e 'const s=JSON.parse(process.argv[1]);const r=s.result??s;console.log(r[process.argv[2]])' "$1" "$2"
}
chat_value() {
  cli --port "$PORT" inspect | bun -e '
    const s = JSON.parse(await new Response(Bun.stdin.stream()).text());
    const r = s.result ?? s;
    const n = r.nodes.find(n => n.control_id === "IngameChat");
    if (!n) { console.error("IngameChat not found"); process.exit(2); }
    console.log(n.value ?? "");
  '
}
focused_chat() {
  cli --port "$PORT" inspect | bun -e '
    const s = JSON.parse(await new Response(Bun.stdin.stream()).text());
    const r = s.result ?? s;
    const n = r.nodes.find(n => n.id === r.focused_id);
    console.log(n?.control_id === "IngameChat" ? "true" : "false");
  '
}
expect() {
  if [ "$2" != "$3" ]; then
    echo "FAIL 85: $1 = $2, want $3" >&2
    exit 1
  fi
}

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$PORT" click --label Tutorial >/dev/null
cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 15000 >/dev/null
cli --port "$PORT" pause >/dev/null
for _ in $(seq 1 100); do
  if cli --port "$PORT" world_state | bun -e '
    const s = JSON.parse(await new Response(Bun.stdin.stream()).text());
    const r = s.result ?? s; process.exit((r.players?.length??0)>0?0:1);'; then break; fi
  cli --port "$PORT" step --frames 4 >/dev/null
done

st="$(status)"
expect "chat initially closed" "$(field "$st" chat_active)" false
expect "chat buffer initially empty" "$(field "$st" chat_text_len)" 0

tap_scancode 23
cli --port "$PORT" wait_frames --n 3 >/dev/null
st="$(status)"
expect "raw T opens chat" "$(field "$st" chat_active)" true
expect "raw T is not typed into chat" "$(field "$st" chat_text_len)" 0
expect "chat input focused" "$(focused_chat)" true

tap_scancode 11
tap_scancode 12
cli --port "$PORT" wait_frames --n 3 >/dev/null
st="$(status)"
expect "raw h/i update compose length" "$(field "$st" chat_text_len)" 2
expect "raw h/i visible in IngameChat" "$(chat_value)" "hi"

tap_scancode 41
cli --port "$PORT" wait_frames --n 3 >/dev/null
st="$(status)"
expect "raw Escape closes chat" "$(field "$st" chat_active)" false
expect "raw Escape clears chat buffer" "$(field "$st" chat_text_len)" 0

echo "PASS 85_ingame_chat_tui_raw_input"
