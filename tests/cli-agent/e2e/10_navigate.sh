#!/usr/bin/env bash
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

# MAINMENU -> OPTIONS
$CLI --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
$CLI --port "$PORT" click --label OPTIONS
$CLI --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000
# OPTIONS -> back -> MAINMENU
$CLI --port "$PORT" back
$CLI --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000
echo "PASS 10_navigate"
