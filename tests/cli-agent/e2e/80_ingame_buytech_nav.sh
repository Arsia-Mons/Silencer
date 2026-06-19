#!/usr/bin/env bash
# In-game buy/tech overlay navigation vs origin (InGameUiController):
#   - Up/Down moves the selection and CLAMPS at both ends — origin
#     ClampBuyTechSelection never wraps (ingameuicontroller_detail).
#   - Esc closes the overlay (origin ApplyActions Cancel branch ->
#     isbuying/techstationactive = false).
# A real purchase (Enter -> Player::BuyItem, credits spent, ammo granted) is
# asserted end-to-end by scenario 76's capture drive: the tutorial's case-13
# message only advances when rocketammo > 0 after the keyed purchase.
# The tutorial buy list has exactly 2 rows (peer techchoices = LASER|ROCKET).
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT
wait_alive "$PORT"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$PORT" click --label Tutorial >/dev/null
cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 15000 >/dev/null
cli --port "$PORT" pause >/dev/null
for _ in $(seq 1 100); do
  if cli --port "$PORT" world_state | bun -e '
    const s = JSON.parse(await new Response(Bun.stdin.stream()).text());
    const r = s.result ?? s;
    process.exit((r.players?.length ?? 0) > 0 ? 0 : 1);'; then break; fi
  cli --port "$PORT" step --frames 4 >/dev/null
done

field() {
  cli --port "$PORT" ingame_ui_mode --mode status | bun -e \
    'const s=JSON.parse(await new Response(Bun.stdin.stream()).text());const r=s.result??s;console.log(r[process.argv[1]])' "$1"
}
expect() {
  if [ "$2" != "$3" ]; then echo "FAIL 80: $1 = $2, want $3" >&2; exit 1; fi
}
press() {
  cli --port "$PORT" key --key "$1" >/dev/null
  cli --port "$PORT" wait_frames --n 2 >/dev/null
}

cli --port "$PORT" ingame_ui_mode --mode buy >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
expect "buy overlay open" "$(field buy_active)" true
expect "initial selection" "$(field buy_selected_index)" 0

press up
expect "Up clamps at the top (no wrap)" "$(field buy_selected_index)" 0
press down
expect "Down moves to row 1" "$(field buy_selected_index)" 1
press down
expect "Down clamps at the bottom (no wrap)" "$(field buy_selected_index)" 1

press escape
expect "Esc closes the overlay" "$(field buy_active)" false

echo "PASS 80_ingame_buytech_nav"
