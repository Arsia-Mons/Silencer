#!/usr/bin/env bash
# Golden cppx interaction model on the MainMenu (no lobby needed):
#   - Hover sets a *visual* pseudo-class (node.hovered=true) on the node under
#     the pointer; it does NOT move keyboard focus.
#   - Moving the pointer to empty space clears hovered on every button.
#   - A real click activates the control: it routes/opens (here, the Options
#     overlay dialog) and moves focus into the activated surface.
# This replaces the old "hover moves focus" premise, which is not how the
# retained cppx tree behaves.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

# Pin a known viewport so the menu layout is deterministic across runs.
cli --port "$PORT" resize --w 640 --h 480 >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null

OUT_DIR="$(mktemp -d)"
BASELINE="$OUT_DIR/inspect-baseline.json"
HOVER="$OUT_DIR/inspect-hover.json"
HOVER_OUT="$OUT_DIR/inspect-hover-out.json"
AFTER_CLICK="$OUT_DIR/inspect-after-click.json"

# Baseline: read the Options button bounds straight from the retained tree so
# we hover its true center (UI-space) instead of pixel-pinning legacy coords.
cli --port "$PORT" inspect > "$BASELINE"
read -r OPT_CX OPT_CY < <(bun -e '
const r = JSON.parse(await Bun.file(process.argv[1]).text());
const opt = (r.nodes || []).find((n) => n.role === "button" && n.label === "Options");
if (!opt) { console.error("Options button not found in main menu"); process.exit(1); }
console.log(`${Math.round(opt.x + opt.w / 2)} ${Math.round(opt.y + opt.h / 2)}`);
' "$BASELINE")

# (a) Hover the center of the Options button: it should pick up hovered=true
#     while no other menu button is hovered, and focus must NOT move there.
cli --port "$PORT" hover_at --x "$OPT_CX" --y "$OPT_CY" >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" inspect > "$HOVER"

# (b) Hover an empty point (top-left corner): no menu button is hovered.
cli --port "$PORT" hover_at --x 5 --y 5 >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" inspect > "$HOVER_OUT"

# (c) Click the Options button: it activates the Options overlay (a dialog with
#     its own focusable buttons) and moves focus into that surface. This proves
#     click — unlike hover — changes focus / activates.
cli --port "$PORT" click --label "Options" >/dev/null
cli --port "$PORT" wait_frames --n 5 >/dev/null
cli --port "$PORT" inspect > "$AFTER_CLICK"

bun -e '
const baseline = JSON.parse(await Bun.file(process.argv[1]).text());
const hover = JSON.parse(await Bun.file(process.argv[2]).text());
const hoverOut = JSON.parse(await Bun.file(process.argv[3]).text());
const afterClick = JSON.parse(await Bun.file(process.argv[4]).text());

const buttons = (r) => (r.nodes || []).filter((n) => n.role === "button");
const labelled = (r) => buttons(r).map((b) => ({ label: b.label, hovered: b.hovered, focused: b.focused }));

// --- (a) Hover sets the visual pseudo-class on exactly the hovered node. ---
const optHover = buttons(hover).find((b) => b.label === "Options");
if (!optHover) { console.error("Options button missing while hovered"); process.exit(1); }
if (optHover.hovered !== true) {
  console.error(`hover did not set Options.hovered=true: ${JSON.stringify(labelled(hover))}`);
  process.exit(1);
}
const otherHovered = buttons(hover).filter((b) => b.label !== "Options" && b.hovered === true);
if (otherHovered.length !== 0) {
  console.error(`hover bled onto other buttons: ${JSON.stringify(otherHovered.map((b) => b.label))}`);
  process.exit(1);
}

// Hover is a *visual* state only: focus must NOT have moved to Options. The
// menu autofocuses "Play Online", and hovering Options must not change that.
const focusedWhileHover = buttons(hover).filter((b) => b.focused === true);
if (focusedWhileHover.some((b) => b.label === "Options")) {
  console.error(`hover moved focus to Options (it must not): ${JSON.stringify(labelled(hover))}`);
  process.exit(1);
}
const focusedBaseline = buttons(baseline).filter((b) => b.focused === true).map((b) => b.label);
const focusedHover = focusedWhileHover.map((b) => b.label);
if (JSON.stringify(focusedBaseline) !== JSON.stringify(focusedHover)) {
  console.error(`hover changed which button is focused: ${JSON.stringify(focusedBaseline)} -> ${JSON.stringify(focusedHover)}`);
  process.exit(1);
}

// --- (b) Moving the pointer to empty space clears hovered everywhere. ---
const stillHovered = buttons(hoverOut).filter((b) => b.hovered === true);
if (stillHovered.length !== 0) {
  console.error(`hover-out left buttons hovered: ${JSON.stringify(stillHovered.map((b) => b.label))}`);
  process.exit(1);
}

// --- (c) Click activates Options: the overlay dialog appears, focus moves. ---
const dialog = (afterClick.nodes || []).find((n) => n.role === "dialog");
if (!dialog) {
  console.error(`click did not open the Options dialog: roles=${JSON.stringify((afterClick.nodes||[]).map((n)=>n.role))}`);
  process.exit(1);
}
// The dialog must own a focusable button that is now focused (focus moved into
// the activated surface — proof that click, not hover, drives focus).
const dialogButtons = buttons(afterClick).filter((b) => (b.control_id || "").startsWith("Options"));
const focusedDialogBtn = dialogButtons.find((b) => b.focused === true);
if (!focusedDialogBtn) {
  console.error(`click did not move focus into the Options dialog: ${JSON.stringify(buttons(afterClick).map((b) => ({ control_id: b.control_id, focused: b.focused })))}`);
  process.exit(1);
}
// And focus genuinely left the main-menu surface.
if (afterClick.focused_id === baseline.focused_id) {
  console.error(`focused_id did not change after click: ${afterClick.focused_id}`);
  process.exit(1);
}
console.log(JSON.stringify({ optCenter: [Number(process.argv[5]), Number(process.argv[6])], focusedAfterClick: focusedDialogBtn.control_id }));
' "$BASELINE" "$HOVER" "$HOVER_OUT" "$AFTER_CLICK" "$OPT_CX" "$OPT_CY"

echo "PASS 16_focus_follows_hover ($OUT_DIR)"
