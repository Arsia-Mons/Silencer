#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

# Returns the label of the currently focused node, or "NONE".
focused_label() {
  cli --port "$PORT" inspect | bun -e '
    const r = JSON.parse(require("fs").readFileSync(0, "utf8"));
    const n = r.nodes.find((n) => n.id === r.focused_id);
    process.stdout.write(n ? (n.label || "NONE") : "NONE");
  '
}

# Returns "true" if the Options overlay (with the OptionsGoBack control) is
# currently mounted, else "false".
options_dialog_open() {
  cli --port "$PORT" inspect | bun -e '
    const r = JSON.parse(require("fs").readFileSync(0, "utf8"));
    const open = r.nodes.some((n) => n.control_id === "OptionsGoBack");
    process.stdout.write(String(open));
  '
}

# Main-menu focus order: Connect To Lobby -> Tutorial -> Options -> Quit, with
# initial focus on "Connect To Lobby" (index 0). The control "down" op mirrors the
# gamepad menu-navigation mapping in Game::TickGamepadMenuNavigation, so two
# "down" presses move focus to "Options". A third "down" would land on "Exit"
# and activating it quits the app.
cli --port "$PORT" key --key down >/dev/null
cli --port "$PORT" key --key down >/dev/null

# Verify directional nav landed focus on "Options" before activating it.
landed="$(focused_label)"
if [ "$landed" != "Options" ]; then
  echo "FAIL 14_directional_navigation: expected focus on 'Options', got '$landed'" >&2
  exit 1
fi

# Enter activates the focused button. In the modern cppx UI the Options screen
# is a modal dialog mounted over the main menu (the game state stays MAINMENU),
# so verify the dialog opened by inspecting the node tree rather than waiting
# on a now-defunct OPTIONS game state.
cli --port "$PORT" key --key enter >/dev/null
for _ in $(seq 1 30); do
  [ "$(options_dialog_open)" = "true" ] && break
  sleep 0.2
done
if [ "$(options_dialog_open)" != "true" ]; then
  echo "FAIL 14_directional_navigation: Options dialog did not open after enter" >&2
  exit 1
fi

# escape dismisses the dialog and returns to the plain main menu. The game
# state was never anything but MAINMENU, so re-assert it stays MAINMENU and
# the dialog is gone (the modern equivalent of the old "back -> MAINMENU").
cli --port "$PORT" key --key escape >/dev/null
for _ in $(seq 1 30); do
  [ "$(options_dialog_open)" = "false" ] && break
  sleep 0.2
done
if [ "$(options_dialog_open)" != "false" ]; then
  echo "FAIL 14_directional_navigation: Options dialog did not close after escape" >&2
  exit 1
fi
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000 >/dev/null

echo "PASS 14_directional_navigation"
