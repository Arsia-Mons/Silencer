#!/usr/bin/env bash
# Drives the modern cppx Options -> Controls (keybinds) screen and asserts the
# keybind list renders. In the cppx UI, Options and Controls are modal dialogs
# layered over MAINMENU (there is no separate OPTIONS / OPTIONSCONTROLS game
# state), so navigation is verified by polling the retained node tree rather
# than wait_for_state. The controls dialog shows one row per rebindable action
# (Fire/Jump/Use/Chat) with a Rebind button, plus preset/save/back controls.
# (Historically this scenario also drove a scroll op; the modern screens have no
# scrollable surface, so that no longer applies.)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"

# Poll the retained cppx tree for a node whose control id OR accessibility label
# equals the argument (the introspection `inspect` op returns `nodes`).
wait_for_node() {
  local label="$1"
  for i in $(seq 1 100); do
    found=$(cli --port "$PORT" inspect | LABEL="$label" bun -e \
      'const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
       const l = process.env.LABEL;
       console.log((r.nodes||[]).some((w)=>w.label===l||w.control_id===l) ? "yes" : "no");' 2>/dev/null || echo no)
    if [ "$found" = "yes" ]; then return 0; fi
    sleep 0.05
  done
  echo "node '$label' never appeared" >&2
  cli --port "$PORT" inspect >&2 || true
  return 1
}

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

# Open the Options dialog, then enter the Controls (keybinds) screen. Both are
# modal overlays over MAINMENU, surfaced as focusable buttons in the tree.
cli --port "$PORT" click --label "Options" >/dev/null
wait_for_node "OptionsControls"
cli --port "$PORT" click --label "OptionsControls" >/dev/null
wait_for_node "ControlsBack"
cli --port "$PORT" wait_frames --n 2 >/dev/null

# Assert the keybind list renders: a Rebind button per rebindable action, the
# matching action labels, and the chrome (preset cycle, back). Everything must
# be in-bounds of the UI-space viewport.
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const nodes = r.nodes ?? [];

const fail = (m) => { console.error(m); process.exit(1); };

// Per-action keybind rows: focusable buttons with a Rebind* control_id.
const rebinds = nodes.filter((n) =>
  n.role === "button" &&
  n.focusable === true &&
  typeof n.control_id === "string" &&
  n.control_id.startsWith("Rebind")
);
if (rebinds.length < 3) {
  fail(`expected multiple keybind rows, got ${rebinds.length}: ` +
    JSON.stringify(rebinds.map((n) => n.control_id)));
}

// The action labels for those rows (text nodes) should be present.
const textValues = new Set(
  nodes.filter((n) => n.role === "text").map((n) => n.value)
);
for (const action of ["Fire", "Jump", "Use", "Chat"]) {
  if (!textValues.has(action)) fail(`missing keybind action label: ${action}`);
}

// Controls-screen chrome must be present.
if (!textValues.has("Controls")) fail("missing Controls title");
if (!nodes.some((n) => n.control_id === "CyclePreset")) fail("missing Cycle Preset button");
if (!nodes.some((n) => n.control_id === "ControlsBack")) fail("missing Back button");

// Every keybind row must render with a positive in-bounds box. Root is the
// full UI-space viewport.
const root = nodes.find((n) => n.role === "generic") ?? nodes[0];
if (!root || !(root.w > 0) || !(root.h > 0)) fail("missing root viewport node");
for (const n of rebinds) {
  if (!(n.w > 0) || !(n.h > 0)) fail(`keybind row ${n.control_id} has no size`);
  if (n.x < 0 || n.y < 0 || n.x + n.w > root.w + 1 || n.y + n.h > root.h + 1) {
    fail(`keybind row ${n.control_id} out of bounds: ` +
      JSON.stringify({ x: n.x, y: n.y, w: n.w, h: n.h }));
  }
}
'

# Back returns to the Options dialog (its sub-buttons reappear).
cli --port "$PORT" click --label "ControlsBack" >/dev/null
wait_for_node "OptionsControls"

echo "PASS 12_controls_scroll"
