#!/usr/bin/env bash
# Text-input length caps vs origin:
#   - lobby_connect username caps at 16 (origin lobby_connect_screen.h
#     username[17]) and password at 28 (password[29]).
#   - the password modal caps at 20 (origin password_modal.cpp maxLength 20,
#     password[21]).
# Our Inputs are fully controlled (value prop round-trips through on_change),
# so the capped model value is what `inspect` reports back as the node value.
# (The character-create alias cap, 16, shares the same implementation; the
# in-game chat cap, 100, is asserted by scenario 77.)
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT
wait_alive "$PORT"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

LONG="$(printf 'x%.0s' $(seq 1 60))"

node_uid() { # node_uid <control_id or label>
  cli --port "$PORT" inspect | bun -e '
    const s = JSON.parse(await new Response(Bun.stdin.stream()).text());
    const r = s.result ?? s;
    const t = process.argv[1];
    const n = r.nodes.find(n => n.control_id === t || n.label === t || n.accessibility_label === t);
    if (!n) { console.error(`node ${t} not found`); process.exit(1); }
    console.log(n.id);' "$1"
}
wait_for_node() { # wait_for_node <target>
  for _ in $(seq 1 50); do
    if cli --port "$PORT" inspect | bun -e '
      const s = JSON.parse(await new Response(Bun.stdin.stream()).text());
      const r = s.result ?? s;
      const t = process.argv[1];
      process.exit(r.nodes.some(n => n.control_id === t || n.label === t) ? 0 : 1);' "$1"; then
      return 0
    fi
    cli --port "$PORT" wait_frames --n 2 >/dev/null
  done
  echo "FAIL 79: node $1 never appeared" >&2
  exit 1
}

node_value_len() {
  cli --port "$PORT" inspect | bun -e '
    const s = JSON.parse(await new Response(Bun.stdin.stream()).text());
    const r = s.result ?? s;
    const t = process.argv[1];
    const n = r.nodes.find(n => n.control_id === t || n.label === t || n.accessibility_label === t);
    if (!n) { console.error(`node ${t} not found`); process.exit(1); }
    console.log((n.value ?? "").length);' "$1"
}
expect_len() { # expect_len <target> <want> — polls (overlay mount + value
  # round-trip take a few frames under suite load)
  local got=""
  for _ in $(seq 1 25); do
    got="$(node_value_len "$1" 2>/dev/null || true)"
    [ "$got" = "$2" ] && return 0
    cli --port "$PORT" wait_frames --n 2 >/dev/null
  done
  echo "FAIL 79: $1 value length ${got:-<missing>}, want $2" >&2
  exit 1
}

# --- lobby_connect username (16) / password (28) ------------------------------
cli --port "$PORT" click --label "Connect To Lobby" >/dev/null
cli --port "$PORT" wait_for_state --state LOBBYCONNECT --timeout-ms 15000 >/dev/null
wait_for_node Username
cli --port "$PORT" set_text --label Username --text "$LONG" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
expect_len Username 16
cli --port "$PORT" set_text --label Password --text "$LONG" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
expect_len Password 28
cli --port "$PORT" back >/dev/null
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

# --- password modal (20) -------------------------------------------------------
cli --port "$PORT" show_password_modal --title "Password" >/dev/null
wait_for_node Password
cli --port "$PORT" set_text --label Password --text "$LONG" >/dev/null
cli --port "$PORT" wait_frames --n 6 >/dev/null
expect_len Password 20

echo "PASS 79_text_input_caps"
