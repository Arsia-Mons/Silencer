#!/usr/bin/env bash
# SIL-108: the Options -> Controls screen restored to origin/main full-IA parity.
# It is now a REAL scrollable, selectable keybind table: every action in the
# ACTION_TABLE renders as a selectable two-column row (label + AND/OR binding),
# inside a virtualizing scroll-viewport (SIL-111). Selecting a row drives a
# single Rebind/Clear pair below; preset is the green oval; Save/Revert/Back at
# the foot. This scenario drives that screen: it mounts, the table SCROLLS
# (wheel changes which rows are present — proving the viewport + virtualization),
# and selecting a row updates the Selected readout.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"

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
  return 1
}

# Snapshot the set of visible keybind-row control ids (Bind*) — the virtualizing
# viewport only mounts the window of rows currently on screen.
visible_rows() {
  cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const n = r.nodes ?? [];
const binds = new Set(n.filter((x) => (x.control_id ?? "").startsWith("Bind")).map((x) => x.control_id));
console.log([...binds].sort().join(","));
'
}

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

cli --port "$PORT" click --label "Options" >/dev/null
wait_for_node "OptionsControls"
cli --port "$PORT" click --label "OptionsControls" >/dev/null
wait_for_node "ControlsBack"
cli --port "$PORT" wait_frames --n 3 >/dev/null

# The full screen chrome is present (not truncated): title, oval preset, the
# single select-then-act Rebind/Clear, and Save/Revert/Back.
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const n = r.nodes ?? [];
const fail = (m) => { console.error(m); process.exit(1); };
const has = (id) => n.some((x) => x.control_id === id);
for (const id of ["CyclePreset", "RebindSelected", "ClearSelected", "SaveBinds", "RevertBinds", "ControlsBack", "ControlsList"]) {
  if (!has(id)) fail(`Controls screen missing ${id}`);
}
if (!n.some((x) => x.role === "text" && x.value === "Controls")) fail("missing Controls title");
const rows = n.filter((x) => (x.control_id ?? "").startsWith("Bind"));
if (rows.length < 4) fail(`expected several visible keybind rows, got ${rows.length}`);
if (!n.some((x) => x.role === "text" && (x.value ?? "").startsWith("Selected:"))) fail("missing Selected readout");
'

# Scroll the table: the visible row window must CHANGE (viewport + virtualization).
top_rows="$(visible_rows)"
cli --port "$PORT" scroll --x 320 --y 250 --dy -8 >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
scrolled_rows="$(visible_rows)"
if [ "$top_rows" = "$scrolled_rows" ]; then
  echo "scroll did not change the visible row window ($top_rows)"; exit 1
fi

# Scroll back to the top and select a row; the Selected readout updates.
cli --port "$PORT" scroll --x 320 --y 250 --dy 40 >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" click --label Bind2 >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const sel = (r.nodes ?? []).find((n) => n.role === "text" && (n.value ?? "").startsWith("Selected:"));
if (!sel) { console.error("no Selected readout after click"); process.exit(1); }
if (sel.value === "Selected: (none)") { console.error("selection did not update"); process.exit(1); }
'

# Back returns to the Options dialog.
cli --port "$PORT" click --label "ControlsBack" >/dev/null
wait_for_node "OptionsControls"

echo "PASS 12_controls_scroll"
