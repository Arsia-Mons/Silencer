#!/usr/bin/env bash
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

OUT=$(cli --port "$PORT" ping)
echo "$OUT" | grep -q '"version"'
echo "PASS 00_ping"
