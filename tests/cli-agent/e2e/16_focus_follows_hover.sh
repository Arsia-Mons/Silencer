#!/usr/bin/env bash
set -euo pipefail

# Mouse hover and keyboard navigation must share one focus highlight while the
# pointer is over a button. Moving the pointer away clears pointer-origin focus
# without restoring the earlier keyboard-focused button.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
wait_for_widget "Connect To Lobby"

# Pin a known viewport, then read the retained button center from inspect so
# the test follows the actual JSX layout instead of a stale coordinate table.
cli --port "$PORT" resize --w 640 --h 480 >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

OUT_DIR="$(mktemp -d)"
KB="$OUT_DIR/inspect-keyboard.json"
HOVER="$OUT_DIR/inspect-hover.json"
HOVER_OUT="$OUT_DIR/inspect-hover-out.json"

# Keyboard-focus the first menu button (Tutorial).
cli --port "$PORT" key --key down >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" inspect > "$KB"

# Now hover the Options button with the mouse.
options_center="$(cli --port "$PORT" inspect | bun -e '
const text = await new Response(Bun.stdin.stream()).text();
const widgets = JSON.parse(text).widgets ?? [];
const options = widgets.find((w) => w.source === "ui" && w.kind === "button" && w.label === "Options");
if (!options) {
  console.error("Options button missing");
  process.exit(1);
}
console.log(`${Math.floor(options.x + options.w / 2)} ${Math.floor(options.y + options.h / 2)}`);
')"
read -r options_x options_y <<< "$options_center"
cli --port "$PORT" hover_at --x "$options_x" --y "$options_y" >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" inspect > "$HOVER"

# Moving the pointer off interactables clears pointer-origin focus; keyboard
# focus remains durable only until hover has explicitly taken pointer focus.
cli --port "$PORT" hover_at --x 20 --y 20 >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" inspect > "$HOVER_OUT"

bun -e '
const kb = JSON.parse(await Bun.file(process.argv[1]).text()).widgets ?? [];
const hover = JSON.parse(await Bun.file(process.argv[2]).text()).widgets ?? [];
const hoverOut = JSON.parse(await Bun.file(process.argv[3]).text()).widgets ?? [];
const buttons = (ws) => ws.filter((w) => w.source === "ui" && w.kind === "button");
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

const hoverOutFocused = focused(hoverOut);
if (hoverOutFocused.length !== 0) {
  console.error(`expected no focused button after hover-out, got ${hoverOutFocused.length}: ${JSON.stringify(buttons(hoverOut).map((b) => ({ label: b.label, focused: b.focused })))}`);
  process.exit(1);
}
' "$KB" "$HOVER" "$HOVER_OUT"

echo "PASS 16_focus_follows_hover ($OUT_DIR)"
