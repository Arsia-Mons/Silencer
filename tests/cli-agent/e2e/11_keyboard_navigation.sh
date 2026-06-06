#!/usr/bin/env bash
# Keyboard-only navigation of the main menu: tab (= down) moves focus through
# the registry order, and enter activates the focused button. In the modern
# cppx UI the Options cluster is an overlay pushed over the menu (the session
# phase stays MAINMENU; see 10_navigate.sh), so we verify the overlay opened
# rather than waiting for a defunct OPTIONS game state.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

# Main-menu focus order: Connect To Lobby -> Tutorial -> Options -> Quit (focus
# starts on Connect To Lobby). Two tabs (tab == down) move focus to "Options".
# A third tab would land on "Exit"; activating it quits the app.
cli --port "$PORT" key --key tab >/dev/null
cli --port "$PORT" key --key tab >/dev/null

# Verify keyboard nav landed focus on "Options" before activating it.
focused_label="$(cli --port "$PORT" inspect | bun -e '
  const r = JSON.parse(require("fs").readFileSync(0, "utf8"));
  const n = r.nodes.find((n) => n.id === r.focused_id);
  process.stdout.write(n ? (n.label || "") : "");
')"
if [ "$focused_label" != "Options" ]; then
  echo "FAIL 11_keyboard_navigation: expected focus on 'Options', got '$focused_label'" >&2
  exit 1
fi

# Enter activates the focused button -> Options overlay opens over the menu.
cli --port "$PORT" key --key enter >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" inspect | bun -e '
  const r = JSON.parse(require("fs").readFileSync(0, "utf8"));
  const labels = new Set((r.nodes ?? []).filter((n) => n.role === "button").map((b) => b.label));
  for (const x of ["Controls", "Display", "Audio", "Go Back"]) {
    if (!labels.has(x)) { console.error(`enter on Options did not open the overlay (missing button: ${x})`); process.exit(1); }
  }
'
# Options is an overlay, so the session phase must still be MAINMENU.
cli --port "$PORT" state | bun -e '
  const r = JSON.parse(require("fs").readFileSync(0, "utf8"));
  if (r.state !== "MAINMENU") { console.error(`Options should be an overlay, not a state change (got ${r.state})`); process.exit(1); }
'

# Escape dismisses the overlay and returns to the bare main menu.
cli --port "$PORT" key --key escape >/dev/null
cli --port "$PORT" wait_frames --n 3 >/dev/null
cli --port "$PORT" inspect | bun -e '
  const r = JSON.parse(require("fs").readFileSync(0, "utf8"));
  const labels = new Set((r.nodes ?? []).filter((n) => n.role === "button").map((b) => b.label));
  if (!labels.has("Connect To Lobby")) { console.error("escape did not return to the main menu"); process.exit(1); }
  if (labels.has("Go Back")) { console.error("Options overlay still open after escape"); process.exit(1); }
'

echo "PASS 11_keyboard_navigation"
