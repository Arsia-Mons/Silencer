#!/usr/bin/env bash
# P3 smoke test — drives the `clay_bridge_smoke` control op, dumps a 640x480
# PNG, and pixdiffs it against the committed reference. Pass bar: < 1.0%.
#
# Usage:   bash tests/lobby-clay/bridge_smoke/run.sh
# Updates: rerun with REGEN=1 to overwrite reference.png from the live binary.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"
. "$REPO/tests/cli-agent/e2e/lib.sh"

REF="$HERE/reference.png"
OUT="${TMPDIR:-/tmp}/bridge_smoke.$$.png"
PIXDIFF="$REPO/tools/pixdiff/build/pixdiff"

if [ ! -x "$PIXDIFF" ]; then
  echo "FAIL: pixdiff not built — run: cmake -S tools/pixdiff -B tools/pixdiff/build && cmake --build tools/pixdiff/build -j" >&2
  exit 1
fi

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap 'stop_silencer "$PID" "$PORT"; rm -f "$OUT"' EXIT
wait_alive "$PORT"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null
cli --port "$PORT" clay_bridge_smoke --out "$OUT" >/dev/null

if [ "${REGEN:-0}" = "1" ]; then
  cp "$OUT" "$REF"
  echo "regenerated $REF"
  exit 0
fi

DIFF=$("$PIXDIFF" "$OUT" "$REF")
echo "pixdiff = ${DIFF}%"

# Pass bar from prd.json P3.pass_check: < 1.0%.
awk -v d="$DIFF" 'BEGIN { exit (d + 0 < 1.0) ? 0 : 1 }' || {
  echo "FAIL: pixdiff ${DIFF}% >= 1.0% threshold" >&2
  echo "      output preserved at $OUT for inspection" >&2
  trap - EXIT
  stop_silencer "$PID" "$PORT" || true
  exit 1
}

echo "PASS bridge_smoke"
