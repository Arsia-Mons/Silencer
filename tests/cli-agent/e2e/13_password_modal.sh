#!/usr/bin/env bash
# SIL-18: PasswordModal is an authored cppx OverlayScreen pushed over the phase
# screen. It proves the modal chain end-to-end on the retained cppx path: the
# overlay traps focus on its autofocused field, typed input + Enter submits the
# value through the prop callback, and Escape auto-pops without submitting.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

# --- submit path -----------------------------------------------------------
cli --port "$PORT" show_password_modal >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const nodes = r.nodes ?? [];
const field = nodes.find((n) => n.role === "input" && n.label === "Password");
const ok = nodes.find((n) => n.role === "button" && n.label === "OK");
// Focus is trapped to the modal: its field is focused; the base MainMenu
// buttons are still present underneath but no longer focused.
const menuFocused = nodes.some((n) => n.role === "button" &&
  ["Connect To Lobby","Tutorial","Exit"].includes(n.label) && n.focused);
if (!field || !field.focusable || !field.focused || !ok || menuFocused) {
  console.error("password modal did not expose a focused, trapped field + OK button");
  process.exit(1);
}
'

for ch in s w o r d f i s h; do
  cli --port "$PORT" key --key "$ch" >/dev/null
done
cli --port "$PORT" key --key enter >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" password_modal_result | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const res = r.result ?? r;
if (!res.submitted || res.value !== "swordfish") {
  console.error(`password modal submit mismatch: ${JSON.stringify(res)}`);
  process.exit(1);
}
'

# the submitted modal popped itself
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
if ((r.nodes ?? []).some((n) => n.role === "input" && n.label === "Password")) {
  console.error("password modal did not pop after submit");
  process.exit(1);
}
'

# --- cancel-auto-pop path --------------------------------------------------
cli --port "$PORT" show_password_modal >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" key --key escape >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
if ((r.nodes ?? []).some((n) => n.role === "input" && n.label === "Password")) {
  console.error("Escape did not cancel-auto-pop the password modal");
  process.exit(1);
}
'

echo "PASS 13_password_modal"
