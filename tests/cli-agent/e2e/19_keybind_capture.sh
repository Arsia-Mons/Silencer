#!/usr/bin/env bash
# SIL-19 §7b: multi-device keybind capture on the cppx path. The capture state
# machine lives in the composition root; the `keybind capture` control op feeds
# the same machine the windowed event path feeds (keyboard / mouse-left-gated /
# gamepad button+axis), so this drives the real begin → feed → confirm →
# use_key_map commit chain headlessly. Proves: a gamepad bind, a multi-key chord,
# CHORD_CAP enforcement, and that captures land in the live keymap (match
# in-game). Also checks the OptionsControls UI mounts with rebind rows.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

# --- gamepad button capture (append a combo to Fire) -----------------------
cli --port "$PORT" keybind capture --op begin --action fire >/dev/null
cli --port "$PORT" wait_frames --n 1 >/dev/null
cli --port "$PORT" keybind capture --op feed --binding "PAD:a" >/dev/null
cli --port "$PORT" keybind capture --op confirm >/dev/null
cli --port "$PORT" wait_frames --n 1 >/dev/null
cli --port "$PORT" keybind get --action fire | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const flat = JSON.stringify(r.bindings);
if (!flat.includes("PAD:a")) { console.error(`gamepad bind not captured into Fire: ${flat}`); process.exit(1); }
'

# --- multi-key chord capture (replace Jump combo 0) ------------------------
cli --port "$PORT" keybind capture --op begin --action jump --combo 0 >/dev/null
cli --port "$PORT" wait_frames --n 1 >/dev/null
cli --port "$PORT" keybind capture --op feed --binding "KEY:K" >/dev/null
cli --port "$PORT" keybind capture --op feed --binding "KEY:J" >/dev/null
cli --port "$PORT" keybind capture --op confirm >/dev/null
cli --port "$PORT" wait_frames --n 1 >/dev/null
cli --port "$PORT" keybind get --action jump | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const chord = (r.bindings ?? []).find((c) => Array.isArray(c) && c.length === 2 && c[0] === "KEY:K" && c[1] === "KEY:J");
if (!chord) { console.error(`chord not captured into Jump: ${JSON.stringify(r.bindings)}`); process.exit(1); }
'

# --- CHORD_CAP (3) is enforced: a 4th chip is rejected ---------------------
cli --port "$PORT" keybind capture --op begin --action use >/dev/null
cli --port "$PORT" keybind capture --op feed --binding "KEY:A" >/dev/null
cli --port "$PORT" keybind capture --op feed --binding "KEY:B" >/dev/null
cli --port "$PORT" keybind capture --op feed --binding "KEY:C" >/dev/null
over=$(cli --port "$PORT" keybind capture --op feed --binding "KEY:D" | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
console.log(r.added);
')
[ "$over" = "false" ] || { echo "CHORD_CAP not enforced: 4th chip was accepted"; exit 1; }
cli --port "$PORT" keybind capture --op status | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
if ((r.pending ?? []).length !== 3) { console.error(`expected 3 pending chips, got ${JSON.stringify(r.pending)}`); process.exit(1); }
'
cli --port "$PORT" keybind capture --op cancel >/dev/null

# --- OptionsControls UI mounts with rebind rows ----------------------------
cli --port "$PORT" click --label Options >/dev/null
cli --port "$PORT" wait_frames --n 2 >/dev/null
cli --port "$PORT" click --label OptionsControls >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" inspect | bun -e '
const r = JSON.parse(await new Response(Bun.stdin.stream()).text());
const rebinds = (r.nodes ?? []).filter((n) => n.role === "button" && (n.control_id ?? "").startsWith("Rebind"));
if (rebinds.length < 4) { console.error(`expected the controls rows-of-combos, got ${rebinds.length} Rebind buttons`); process.exit(1); }
const cycle = (r.nodes ?? []).some((n) => n.role === "button" && n.control_id === "CyclePreset");
if (!cycle) { console.error("OptionsControls missing the preset cycle"); process.exit(1); }
'

echo "PASS 19_keybind_capture"
