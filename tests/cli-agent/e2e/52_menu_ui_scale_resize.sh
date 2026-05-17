#!/usr/bin/env bash
# Regression: menu UI scale should follow the continuous menu sizing rule
# instead of snapping between whole-number steps as a desktop window is
# resized. Menus still route pointer clicks through the scaled virtual
# layout, but the reported ui_scale should now match
# max(1, min(width / 640, height / 480)).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

check_resize() {
  local width="$1" height="$2"
  local state_out inspect_out
  state_out="$(mktemp)"
  inspect_out="$(mktemp)"

  cli --port "$PORT" resize --w "$width" --h "$height" >/dev/null
  cli --port "$PORT" wait_frames --n 3 >/dev/null
  cli --port "$PORT" state > "$state_out"
  cli --port "$PORT" inspect > "$inspect_out"

  local click_target
  click_target="$(bun -e '
  const statePath = process.argv[1];
  const inspectPath = process.argv[2];
  const width = Number(process.argv[3]);
  const height = Number(process.argv[4]);
  const expectedScale = Math.max(1, Math.min(width / 640, height / 480));
  const state = JSON.parse(await Bun.file(statePath).text());
  const inspect = JSON.parse(await Bun.file(inspectPath).text());
  if (Math.abs(state.ui_scale - expectedScale) > 0.02) {
    console.error(`expected ui_scale≈${expectedScale}, got ${state.ui_scale}`);
    process.exit(1);
  }
  if (state.ui_width <= 0 || state.ui_height <= 0) {
    console.error(`invalid virtual ui size: ${state.ui_width}x${state.ui_height}`);
    process.exit(1);
  }
  const options = (inspect.widgets ?? []).find((w) => w.kind === "button" && w.label === "Options");
  if (!options) {
    console.error("Options button missing after resize");
    process.exit(1);
  }
  if (options.x < 0 || options.y < 0 || options.x + options.w > state.ui_width || options.y + options.h > state.ui_height) {
    console.error(`Options button is outside virtual ui bounds ${state.ui_width}x${state.ui_height}: ${JSON.stringify(options)}`);
    process.exit(1);
  }
  console.log(`${Math.floor(options.x + options.w / 2)} ${Math.floor(options.y + options.h / 2)}`);
  ' "$state_out" "$inspect_out" "$width" "$height")"

  local x y
  read -r x y <<< "$click_target"
  cli --port "$PORT" click_at --x "$x" --y "$y" >/dev/null
  cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null
  cli --port "$PORT" back >/dev/null
  cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000 >/dev/null

  rm -f "$state_out" "$inspect_out"
}

check_resize 1279 720
check_resize 959 719
check_resize 640 480

echo "PASS 52_menu_ui_scale_resize"
