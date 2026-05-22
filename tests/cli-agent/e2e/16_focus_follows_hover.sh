#!/usr/bin/env bash
set -euo pipefail

# Mouse hover and keyboard navigation must share ONE focus highlight. Moving
# the mouse over a different button moves the single focused state to it; the
# previously keyboard-focused button must stop being focused.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

# Pin a known viewport so the legacy 640x480 button centers are deterministic
# (see tests/cli-agent/e2e/21_main_menu_layout.sh): Options sits at (350,288)
# with a 196x33 oval, so its center is (448, 304).
cli --port "$PORT" resize --w 640 --h 480 >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

OUT_DIR="$(mktemp -d)"
KB="$OUT_DIR/inspect-keyboard.json"
HOVER="$OUT_DIR/inspect-hover.json"

# Keyboard-focus the first menu button (Tutorial).
cli --port "$PORT" key --key down >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" inspect > "$KB"

# Now hover the Options button with the mouse.
cli --port "$PORT" hover_at --x 448 --y 304 >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" inspect > "$HOVER"

bun -e '
const kb = JSON.parse(await Bun.file(process.argv[1]).text()).widgets ?? [];
const hover = JSON.parse(await Bun.file(process.argv[2]).text()).widgets ?? [];
const buttons = (ws) => ws.filter((w) => w.source === "clay" && w.kind === "button");
const focused = (ws) => buttons(ws).filter((w) => w.focused === true);

const kbFocused = focused(kb);
if (kbFocused.length !== 1) {
  console.error(`expected exactly one keyboard-focused button, got ${kbFocused.length}: ${JSON.stringify(buttons(kb).map((b) => ({ label: b.label, focused: b.focused })))}`);
  process.exit(1);
}
if (kbFocused[0].label === "Options") {
  console.error(`keyboard focused the same button the test hovers; pick a different start`);
  process.exit(1);
}

const hoverFocused = focused(hover);
if (hoverFocused.length !== 1) {
  console.error(`expected exactly one focused button after hover, got ${hoverFocused.length}: ${JSON.stringify(buttons(hover).map((b) => ({ label: b.label, focused: b.focused })))}`);
  process.exit(1);
}
if (hoverFocused[0].label !== "Options") {
  console.error(`hover did not move focus: expected "Options" focused, got "${hoverFocused[0].label}"`);
  process.exit(1);
}
' "$KB" "$HOVER"

echo "PASS 16_focus_follows_hover ($OUT_DIR)"
