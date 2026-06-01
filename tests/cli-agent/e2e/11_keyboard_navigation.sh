#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
wait_for_widget "Connect To Lobby"

# MainMenuView autofocuses Tutorial. From there, the current focus order is:
# Connect To Lobby -> Options -> Exit.
cli --port "$PORT" key --key tab >/dev/null
cli --port "$PORT" key --key tab >/dev/null
cli --port "$PORT" key --key enter >/dev/null
wait_for_widget "Controls"

cli --port "$PORT" back >/dev/null
wait_for_widget "Connect To Lobby"

echo "PASS 11_keyboard_navigation"
