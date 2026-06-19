#!/usr/bin/env bash
# SIL-199 + SIL-213: spatial Up/Down focus navigation in Options -> Controls.
#
# SIL-199 established that directional nav must enter the bind rows (not stop on
# the scroll viewport) and must never REST focus on a row that is scrolled out of
# view. SIL-213 reconciles that with reaching EVERY row: Down now moves to the
# next in-order focusable AND the container scrolls it into view, so focus
# traverses every bind row (including ones that start below the fold) in order,
# the scroll follows focus, and focus never rests on an off-viewport row (the
# focused row is always pulled to the viewport edge). Up is the exact reverse.
#
# This test asserts the SIL-213 contract: Down from the preset row visits each
# bind row in order (BindP0, BindP1, ... past the initially-visible window),
# Up reverses through them, and only after the LAST bind row does Down leave the
# list for Save/Cancel. The previous SIL-199 assertion ("Down must NOT reach
# BindP4") is intentionally replaced: reaching BindP4+ via scroll-into-view is
# now the correct behavior — SIL-199's real intent (never resting on an invisible
# row) is preserved by the scroll following focus.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

wait_for_node() {
  local id="$1"
  for _ in $(seq 1 100); do
    found=$(cli --port "$PORT" inspect | ID="$id" bun -e '
      const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
      const id = process.env.ID;
      console.log((r.nodes || []).some((n) => n.control_id === id || n.label === id) ? "yes" : "no");
    ' 2>/dev/null || echo no)
    [ "$found" = "yes" ] && return 0
    sleep 0.05
  done
  echo "node '$id' never appeared" >&2
  return 1
}

focused_control() {
  cli --port "$PORT" inspect | bun -e '
    const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
    const n = (r.nodes || []).find((node) => node.id === r.focused_id);
    process.stdout.write(n ? (n.control_id || n.label || n.value || "NONE") : "NONE");
  '
}

# The viewport-relative y of the focused node — used to assert the focused row is
# inside the scroll viewport (scroll-into-view never leaves focus off-screen).
focused_view_y() {
  cli --port "$PORT" inspect | bun -e '
    const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
    const f = (r.nodes || []).find((node) => node.id === r.focused_id);
    const list = (r.nodes || []).find((node) => node.control_id === "ControlsList");
    process.stdout.write((!f || !list) ? "nan" : String((f.y + f.h / 2) - list.y));
  '
}

expect_focus() {
  local want="$1"
  local got
  got="$(focused_control)"
  if [ "$got" != "$want" ]; then
    echo "FAIL 86_controls_directional_focus: expected focus '$want', got '$got'" >&2
    exit 1
  fi
}

press_nav() {
  cli --port "$PORT" key --key "$1" >/dev/null
  # 2 frames: one for the focus move, one for the scroll-into-view to settle
  # (the scroll request is one-frame lagged, by design).
  cli --port "$PORT" wait_frames --n 2 >/dev/null
}

cli --port "$PORT" click --label Options >/dev/null
wait_for_node OptionsControls
cli --port "$PORT" click --label OptionsControls >/dev/null
wait_for_node ControlsBack
cli --port "$PORT" wait_frames --n 3 >/dev/null

expect_focus CyclePreset

# Walk Down through the bind rows. The list virtualizes (only a window of rows is
# ever committed) so we don't know the total up front: press Down until focus
# leaves the BindP sequence, asserting each step is the next contiguous row
# (BindP0, BindP1, ...) and that the focused row sits INSIDE the viewport
# (scroll-into-view never leaves focus off-screen — SIL-199's intent, preserved).
expect_row=0
for _ in $(seq 1 60); do
  press_nav down
  got="$(focused_control)"
  case "$got" in
    BindP*) ;;
    *) break ;; # left the list (action row) — done walking down
  esac
  want="BindP$expect_row"
  if [ "$got" != "$want" ]; then
    echo "FAIL 86_controls_directional_focus: Down expected '$want', got '$got' (rows skipped or out of order)" >&2
    exit 1
  fi
  vy="$(focused_view_y)"
  ok=$(VY="$vy" bun -e 'const v=Number(process.env.VY); console.log(Number.isFinite(v) && v >= -2 && v <= 371 ? "yes":"no");')
  if [ "$ok" != "yes" ]; then
    echo "FAIL 86_controls_directional_focus: $got focused but off-viewport (view_y=$vy)" >&2
    exit 1
  fi
  expect_row=$((expect_row + 1))
done

ROW_COUNT="$expect_row"
if [ "$ROW_COUNT" -lt 6 ]; then
  echo "FAIL 86_controls_directional_focus: Down only reached $ROW_COUNT bind rows (expected the full scrolled list, >6)" >&2
  exit 1
fi

# After the last bind row, Down must land on the action buttons (Save/Cancel),
# proving we walked the WHOLE list before leaving it (not jumping out early).
after_last="$(focused_control)"
case "$after_last" in
  SaveBinds|ControlsBack) ;; # correct
  *)
    echo "FAIL 86_controls_directional_focus: after last bind row expected Save/Cancel, got '$after_last'" >&2
    exit 1
    ;;
esac

# Up reverses through the whole list, scroll following, never resting off-screen.
# Each row has two focusable lanes (BindP = primary, BindS = secondary); Up may
# rest on either lane of a row depending on horizontal nearness, so accept both.
expect_row_lane() {
  local row="$1" got
  got="$(focused_control)"
  if [ "$got" != "BindP$row" ] && [ "$got" != "BindS$row" ]; then
    echo "FAIL 86_controls_directional_focus: Up expected row $row (BindP$row/BindS$row), got '$got'" >&2
    exit 1
  fi
}
press_nav up
expect_row_lane "$((ROW_COUNT - 1))"
for i in $(seq $((ROW_COUNT - 2)) -1 0); do
  press_nav up
  expect_row_lane "$i"
  vy="$(focused_view_y)"
  ok=$(VY="$vy" bun -e 'const v=Number(process.env.VY); console.log(Number.isFinite(v) && v >= -2 && v <= 371 ? "yes":"no");')
  if [ "$ok" != "yes" ]; then
    echo "FAIL 86_controls_directional_focus: row $i (Up) off-viewport (view_y=$vy)" >&2
    exit 1
  fi
done

# One more Up leaves the top of the list for the preset row.
press_nav up
expect_focus CyclePreset

echo "PASS 86_controls_directional_focus"
