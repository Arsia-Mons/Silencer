#!/usr/bin/env bash
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

OUT_DIR="$(mktemp -d)"
$CLI --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
$CLI --port "$PORT" screenshot --out "$OUT_DIR/main.png"
test -s "$OUT_DIR/main.png"

$CLI --port "$PORT" click --label OPTIONS
$CLI --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000
$CLI --port "$PORT" screenshot --out "$OUT_DIR/options.png"
test -s "$OUT_DIR/options.png"

# Frame headers should match (PNG magic).
head -c 8 "$OUT_DIR/main.png" | xxd | head -1 | grep -q '8950 4e47'
head -c 8 "$OUT_DIR/options.png" | xxd | head -1 | grep -q '8950 4e47'
echo "PASS 20_screenshot ($OUT_DIR)"
