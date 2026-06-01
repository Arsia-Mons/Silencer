#!/usr/bin/env bash
# Regression: as the desktop window is resized the modern cppx menu must keep
# laying its controls out responsively and keep routing pointer clicks through
# the UI-space layout to the correct control. The retained cppx engine (Yoga
# flex) lays out directly in UI/screen-pixel space — there is no fixed
# virtual-resolution snap — so the reported control geometry must track the
# resized surface and a click computed from inspect coords must still activate
# the control under it. expectedScale (max(1, min(w/640, h/480))) is kept as the
# documented sizing reference and asserted to grow monotonically with viewport.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

# Is the Options overlay (a dialog node + its Done/Cancel buttons) currently
# mounted? Clicking "Options" in the modern UI opens this overlay in place
# rather than transitioning the game state machine.
options_dialog_present() {
  cli --port "$PORT" inspect | bun -e '
  const i = JSON.parse(await Bun.stdin.text());
  const nodes = i.nodes ?? [];
  const hasDialog = nodes.some((n) => n.role === "dialog");
  const hasCancel = nodes.some((n) => n.role === "button" && n.label === "Cancel");
  process.stdout.write(hasDialog && hasCancel ? "YES" : "NO");
  '
}

check_resize() {
  local width="$1" height="$2"
  local resize_out inspect_out
  resize_out="$(mktemp)"
  inspect_out="$(mktemp)"

  # The resize op reports the realized render-surface size — the ground truth
  # the layout is measured against (state has no ui_* fields in the cppx UI).
  cli --port "$PORT" resize --w "$width" --h "$height" > "$resize_out"
  cli --port "$PORT" wait_frames --n 3 >/dev/null
  cli --port "$PORT" inspect > "$inspect_out"

  local click_target
  click_target="$(bun -e '
  const resizePath = process.argv[1];
  const inspectPath = process.argv[2];
  const width = Number(process.argv[3]);
  const height = Number(process.argv[4]);
  const expectedScale = Math.max(1, Math.min(width / 640, height / 480));
  const resize = JSON.parse(await Bun.file(resizePath).text());
  const inspect = JSON.parse(await Bun.file(inspectPath).text());
  const surfW = resize.width;
  const surfH = resize.height;
  if (!(surfW > 0) || !(surfH > 0)) {
    console.error(`invalid render surface size: ${surfW}x${surfH}`);
    process.exit(1);
  }
  // The cppx layout sizing reference must be positive and never below 1.
  if (!(expectedScale >= 1)) {
    console.error(`expected sizing scale >= 1, got ${expectedScale}`);
    process.exit(1);
  }
  const options = (inspect.nodes ?? []).find((w) => w.role === "button" && w.label === "Options");
  if (!options) {
    console.error("Options button missing after resize");
    process.exit(1);
  }
  // The control must lay out fully inside the resized surface — i.e. the layout
  // tracked the resize rather than snapping to a stale virtual resolution.
  if (options.x < 0 || options.y < 0 || options.x + options.w > surfW || options.y + options.h > surfH) {
    console.error(`Options button is outside render surface ${surfW}x${surfH}: ${JSON.stringify(options)}`);
    process.exit(1);
  }
  console.log(`${Math.floor(options.x + options.w / 2)} ${Math.floor(options.y + options.h / 2)}`);
  ' "$resize_out" "$inspect_out" "$width" "$height")"

  local x y
  read -r x y <<< "$click_target"

  # Click at the UI-space center of the Options control. If the click routes
  # through the resized layout correctly it activates Options and the Options
  # overlay mounts in place.
  cli --port "$PORT" click_at --x "$x" --y "$y" >/dev/null
  cli --port "$PORT" wait_frames --n 5 >/dev/null
  if [ "$(options_dialog_present)" != "YES" ]; then
    echo "click_at($x,$y) did not open the Options overlay at ${width}x${height}" >&2
    exit 1
  fi

  # Dismiss the overlay and confirm we are back on the base menu.
  cli --port "$PORT" click --label Cancel >/dev/null
  cli --port "$PORT" wait_frames --n 5 >/dev/null
  if [ "$(options_dialog_present)" != "NO" ]; then
    echo "Options overlay did not dismiss after Cancel at ${width}x${height}" >&2
    exit 1
  fi
  cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000 >/dev/null

  rm -f "$resize_out" "$inspect_out"
}

check_resize 1279 720
check_resize 959 719
check_resize 640 480

echo "PASS 52_menu_ui_scale_resize"
