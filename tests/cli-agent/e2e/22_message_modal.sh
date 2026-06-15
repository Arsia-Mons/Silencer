#!/usr/bin/env bash
# SIL-18: MessageModal is an authored cppx OverlayScreen carrying its title +
# message as props (no provider). It proves the props-only modal renders its
# message over the phase screen and cancel-auto-pops on Escape.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

cli --port "$PORT" show_message_modal --title "Heads up" --message "Connection lost" >/dev/null
wait_for_label "$PORT" "OK"
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const nodes = r.nodes ?? [];
const msg = nodes.some((n) => n.value === "Connection lost");
const ok = nodes.some((n) => n.role === "button" && n.label === "OK");
if (!msg || !ok) { console.error("message modal did not render message + OK"); process.exit(1); }
'

cli --port "$PORT" key --key escape >/dev/null
wait_for_label "$PORT" "OK" --gone
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
if ((r.nodes ?? []).some((n) => n.value === "Connection lost")) {
  console.error("Escape did not cancel-auto-pop the message modal");
  process.exit(1);
}
'

echo "PASS 22_message_modal"
