#!/usr/bin/env bash
# SIL-199: spatial Up/Down focus navigation in Options -> Controls must enter
# the visible bind rows instead of stopping on the scroll viewport or clipped
# overscan rows.
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
  cli --port "$PORT" wait_frames --n 1 >/dev/null
}

cli --port "$PORT" click --label Options >/dev/null
wait_for_node OptionsControls
cli --port "$PORT" click --label OptionsControls >/dev/null
wait_for_node ControlsBack
cli --port "$PORT" wait_frames --n 3 >/dev/null

expect_focus CyclePreset

for id in BindP0 BindP1 BindP2 BindP3; do
  press_nav down
  expect_focus "$id"
done

for id in BindP2 BindP1 BindP0 CyclePreset; do
  press_nav up
  expect_focus "$id"
done

for id in BindP0 BindP1 BindP2 BindP3; do
  press_nav down
  expect_focus "$id"
done

press_nav down
after_visible_rows="$(focused_control)"
case "$after_visible_rows" in
  BindP4|BindS4|BindP5|BindS5|ControlsList)
    echo "FAIL 86_controls_directional_focus: focused clipped/viewport node '$after_visible_rows' after visible rows" >&2
    exit 1
    ;;
esac

echo "PASS 86_controls_directional_focus"
