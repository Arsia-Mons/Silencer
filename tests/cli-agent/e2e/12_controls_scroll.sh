#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"

OUT_DIR="$(mktemp -d)"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$PORT" click --label OPTIONS >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null
cli --port "$PORT" click --label CONTROLS >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONSCONTROLS --timeout-ms 5000 >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null

cli --port "$PORT" inspect | bun -e '
const text = await new Response(Bun.stdin.stream()).text();
const response = JSON.parse(text);
const widgets = response.widgets ?? [];
for (const label of ["Preset", "Scroll Down", "Cancel"]) {
  if (!widgets.some((w) => w.source === "clay" && w.label === label)) {
    console.error(`missing Clay widget: ${label}`);
    process.exit(1);
  }
}
'

primary="$(cli --port "$PORT" inspect | bun -e '
const text = await new Response(Bun.stdin.stream()).text();
const response = JSON.parse(text);
const widget = (response.widgets ?? []).find((w) => w.id === "options_controls.primary.0");
if (!widget) {
  console.error("missing primary binding widget");
  process.exit(1);
}
console.log(`${Math.floor(widget.x + widget.w / 2)} ${Math.floor(widget.y + widget.h / 2)}`);
')"
read -r primary_x primary_y <<< "$primary"
cli --port "$PORT" click_at --x "$primary_x" --y "$primary_y" >/dev/null
cli --port "$PORT" wait_frames --n 1 >/dev/null
cli --port "$PORT" key --key x >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" keybind get --action move_up | bun -e '
const text = await new Response(Bun.stdin.stream()).text();
const response = JSON.parse(text);
const result = response.result ?? response;
const bindings = JSON.stringify(result.bindings ?? []);
if (!bindings.includes("KEY:X")) {
  console.error(`controls rebind did not capture KEY:X: ${bindings}`);
  process.exit(1);
}
'

before="$OUT_DIR/controls-before.png"
after="$OUT_DIR/controls-after.png"
cli --port "$PORT" screenshot --out "$before" >/dev/null
cli --port "$PORT" scroll --label "Scroll Down" --amount 3 >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" screenshot --out "$after" >/dev/null

diff="$(tools/pixdiff/build/pixdiff --crop 80,90,500,230 "$before" "$after")"
bun -e '
const diff = Number(process.argv[1]);
if (!Number.isFinite(diff) || diff <= 0.01) {
  console.error(`expected controls list to visibly scroll, got diff ${diff}`);
  process.exit(1);
}
' "$diff"

cli --port "$PORT" click --label CANCEL >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null

echo "PASS 12_controls_scroll ($OUT_DIR)"
