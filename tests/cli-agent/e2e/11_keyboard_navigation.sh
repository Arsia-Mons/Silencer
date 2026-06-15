#!/usr/bin/env bash
# Keyboard-only navigation of the main menu: Tab cycles focus forward,
# Shift+Tab cycles it backward, and Enter activates the focused button. In the modern
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

focused_label() {
  cli --port "$PORT" inspect | bun -e '
  const r = JSON.parse(require("fs").readFileSync(0, "utf8"));
  const n = r.nodes.find((n) => n.id === r.focused_id);
  process.stdout.write(n ? (n.label || "") : "");
'
}
press_and_expect_focus() {
  local key="$1"
  local want="$2"
  cli --port "$PORT" key --key "$key" >/dev/null
  cli --port "$PORT" wait_frames --n 2 >/dev/null
  local got
  got="$(focused_label)"
  if [ "$got" != "$want" ]; then
    echo "FAIL 11_keyboard_navigation: after '$key' expected focus '$want', got '$got'" >&2
    exit 1
  fi
}

# Main-menu focus order: Tutorial -> Connect To Lobby -> Options -> Exit.
if [ "$(focused_label)" != "Tutorial" ]; then
  echo "FAIL 11_keyboard_navigation: expected initial focus on Tutorial, got '$(focused_label)'" >&2
  exit 1
fi
press_and_expect_focus tab "Connect To Lobby"
press_and_expect_focus tab "Options"
press_and_expect_focus tab "Exit"
press_and_expect_focus tab "Tutorial"
press_and_expect_focus shift-tab "Exit"
press_and_expect_focus shift-tab "Options"
press_and_expect_focus shift-tab "Connect To Lobby"
press_and_expect_focus tab "Options"

# Enter activates the focused button -> Options overlay opens over the menu.
# Tier-1 overlays fade out->in and commit at black, so wait for the overlay's
# widget rather than a fixed frame count.
cli --port "$PORT" key --key enter >/dev/null
wait_for_label "$PORT" "Go Back"
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
wait_for_label "$PORT" "Go Back" --gone
cli --port "$PORT" inspect | bun -e '
  const r = JSON.parse(require("fs").readFileSync(0, "utf8"));
  const labels = new Set((r.nodes ?? []).filter((n) => n.role === "button").map((b) => b.label));
  if (!labels.has("Connect To Lobby")) { console.error("escape did not return to the main menu"); process.exit(1); }
  if (labels.has("Go Back")) { console.error("Options overlay still open after escape"); process.exit(1); }
'

echo "PASS 11_keyboard_navigation"
