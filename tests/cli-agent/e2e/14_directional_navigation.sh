#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
wait_for_widget "Connect To Lobby"

# The control key op's directional names mirror the gamepad UI navigation
# mapping in Game::TickGamepadMenuNavigation. MainMenuView autofocuses
# Tutorial, so two downward moves land on Options.
cli --port "$PORT" key --key down >/dev/null
cli --port "$PORT" key --key down >/dev/null
cli --port "$PORT" key --key enter >/dev/null
wait_for_widget "Controls"

cli --port "$PORT" back >/dev/null
wait_for_widget "Connect To Lobby"

echo "PASS 14_directional_navigation"
