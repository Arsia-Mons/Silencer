#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 15000 >/dev/null

# Focus order is the current main-menu registry order:
# Tutorial -> Connect To Lobby -> Options -> Exit.
cli --port "$PORT" key --key tab >/dev/null
cli --port "$PORT" key --key tab >/dev/null
cli --port "$PORT" key --key tab >/dev/null
cli --port "$PORT" key --key enter >/dev/null
cli --port "$PORT" wait_for_ui --id options.controls --timeout-ms 5000 >/dev/null

cli --port "$PORT" back >/dev/null
cli --port "$PORT" wait_for_ui --id main_menu.options --timeout-ms 5000 >/dev/null

echo "PASS 11_keyboard_navigation"
