#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/lib.sh"

PORT="$(pick_port)"
PID="$(start_silencer "$PORT")"
trap 'stop_silencer "$PID" "$PORT"' EXIT

wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

# The focus runtime auto-focuses the first declared button after layout.
# Focus order is Tutorial -> Connect To Lobby -> Options -> Exit.
cli --port "$PORT" key --key tab >/dev/null
cli --port "$PORT" key --key tab >/dev/null
cli --port "$PORT" key --key enter >/dev/null
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000 >/dev/null

cli --port "$PORT" back >/dev/null
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000 >/dev/null

echo "PASS 11_keyboard_navigation"
