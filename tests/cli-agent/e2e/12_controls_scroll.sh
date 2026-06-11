#!/usr/bin/env bash
# Options -> Controls keybind screen renders its full chrome and keybind table.
# Navigation: MAINMENU -> click "Options" (overlay dialog; game state stays
# MAINMENU) -> click "Controls" (control_id OptionsControls) -> the Configure
# Controls screen mounts with the preset cycle, the keybind list (action label +
# primary bind | OR | secondary bind per row), and Save/Cancel. This scenario
# asserts the chrome + several in-viewport keybind rows, then Cancel returns to
# the Options dialog. NOTE: the modern cppx screens have no scroll op — the
# list assertion is presence + geometry, not wheel-driven.
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

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

cli --port "$PORT" click --label "Options" >/dev/null
wait_for_node "OptionsControls"
cli --port "$PORT" click --label "OptionsControls" >/dev/null
wait_for_node "ControlsBack"
cli --port "$PORT" wait_frames --n 3 >/dev/null

# Full chrome present: "Configure Controls" title, the preset cycle button,
# the keybind list container, and Save/Cancel. The keybind table renders as
# per-action rows: action label text + primary bind button (BindPn) + "OR" +
# secondary bind button (BindSn). Several rows must be focusable buttons that
# actually sit inside the ControlsList viewport (in-bounds), each with its
# paired secondary bind.
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const n = r.nodes ?? [];
const fail = (m) => { console.error(m); process.exit(1); };
const byId = (id) => n.find((x) => x.control_id === id);
for (const id of ["CyclePreset", "SaveBinds", "ControlsBack", "ControlsList"]) {
  if (!byId(id)) fail(`Controls screen missing ${id}`);
}
if (!n.some((x) => x.role === "text" && x.value === "Configure Controls")) fail("missing Configure Controls title");

const list = byId("ControlsList");
const primaries = n.filter((x) => /^BindP\d+$/.test(x.control_id ?? ""));
if (primaries.length < 4) fail(`expected several primary bind buttons, got ${primaries.length}`);

const intersects = (a, b) =>
  a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
const inView = primaries.filter((x) => intersects(x, list));
if (inView.length < 3) fail(`expected >=3 keybind rows inside the ControlsList viewport, got ${inView.length}`);

for (const p of inView) {
  if (p.role !== "button" || !p.focusable) fail(`${p.control_id} is not a focusable button`);
  if (!(p.w > 0 && p.h > 0)) fail(`${p.control_id} has degenerate geometry ${p.w}x${p.h}`);
  const sid = p.control_id.replace("BindP", "BindS");
  const s = byId(sid);
  if (!s || s.role !== "button") fail(`row ${p.control_id} missing secondary bind ${sid}`);
}

// Rows are labelled actions: the action-name texts render alongside the binds.
const actionTexts = n.filter((x) => x.role === "text" && /:\s*$/.test(x.value ?? ""));
if (actionTexts.length < 3) fail(`expected action-label texts next to bind rows, got ${actionTexts.length}`);
'

# Cancel/Back returns to the Options dialog.
cli --port "$PORT" click --label "ControlsBack" >/dev/null
wait_for_node "OptionsControls"

echo "PASS 12_controls_scroll"
