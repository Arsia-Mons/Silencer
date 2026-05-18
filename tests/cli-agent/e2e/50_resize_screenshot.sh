#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"

OUT_DIR="$(mktemp -d)"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000

check_resize() {
  local width="$1" height="$2" out="$3"
  local inspect_out="$OUT_DIR/inspect-${width}x${height}.json"
  cli --port "$PORT" resize --w "$width" --h "$height" >/dev/null
  cli --port "$PORT" wait_frames --n 2 >/dev/null
  cli --port "$PORT" inspect > "$inspect_out"
  cli --port "$PORT" screenshot --out "$out" >/dev/null
  bun -e '
  const path = process.argv[1];
  const expectedWidth = Number(process.argv[2]);
  const expectedHeight = Number(process.argv[3]);
  const bytes = await Bun.file(path).arrayBuffer();
  const view = new DataView(bytes);
  const width = view.getUint32(16);
  const height = view.getUint32(20);
  if (width !== expectedWidth || height !== expectedHeight) {
    console.error(`expected ${expectedWidth}x${expectedHeight}, got ${width}x${height}`);
    process.exit(1);
  }
  ' "$out" "$width" "$height"
  local options_center
  options_center="$(bun -e '
  const inspectPath = process.argv[1];
  const width = Number(process.argv[2]);
  const height = Number(process.argv[3]);
  const data = JSON.parse(await Bun.file(inspectPath).text());
  const expected = ["Tutorial", "Connect To Lobby", "Options", "Exit"];
  const byLabel = new Map((data.widgets ?? []).map((w) => [w.label, w]));
  for (const label of expected) {
    const widget = byLabel.get(label);
    if (!widget) {
      console.error(`missing widget: ${label}`);
      process.exit(1);
    }
    if (widget.w <= 0 || widget.h <= 0) {
      console.error(`widget has invalid size: ${label} ${widget.w}x${widget.h}`);
      process.exit(1);
    }
    if (widget.x < 0 || widget.y < 0 || widget.x + widget.w > width || widget.y + widget.h > height) {
      console.error(`widget out of bounds: ${label} at ${widget.x},${widget.y} ${widget.w}x${widget.h} in ${width}x${height}`);
      process.exit(1);
    }
  }
  const options = byLabel.get("Options");
  console.log(`${Math.floor(options.x + options.w / 2)} ${Math.floor(options.y + options.h / 2)}`);
  ' "$inspect_out" "$width" "$height")"
  local options_x options_y
  read -r options_x options_y <<< "$options_center"
  cli --port "$PORT" click_at --x "$options_x" --y "$options_y" >/dev/null
  cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null
  cli --port "$PORT" back >/dev/null
  cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000 >/dev/null
}

check_resize 1280 720 "$OUT_DIR/main-1280x720.png"
check_resize 390 844 "$OUT_DIR/main-390x844.png"

echo "PASS 50_resize_screenshot ($OUT_DIR)"
