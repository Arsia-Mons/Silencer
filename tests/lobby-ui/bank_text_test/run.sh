#!/usr/bin/env bash
# P4 BankText primitive unit test — drives the `clay_bank_text_test` control
# op, which renders "Silencer" via the BankText(Title, effectColor=152)
# primitive at (15, 32), dumps a 640x480 PNG, and pixdiffs it against the
# committed reference. Pass bar: < 0.5%.
#
# Usage:   bash tests/lobby-ui/bank_text_test/run.sh
# Updates: rerun with REGEN=1 to overwrite reference.png from the live binary.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"
. "$REPO/tests/cli-agent/e2e/lib.sh"

REF="$HERE/reference.png"
OUT="${TMPDIR:-/tmp}/bank_text_test.$$.png"
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
cli --port "$PORT" clay_bank_text_test --out "$OUT" >/dev/null

if [ "${REGEN:-0}" = "1" ]; then
  cp "$OUT" "$REF"
  echo "regenerated $REF"
  exit 0
fi

DIFF=$("$PIXDIFF" "$OUT" "$REF")
echo "pixdiff = ${DIFF}%"

# Pass bar from prd.json P4.pass_check: < 0.5%.
awk -v d="$DIFF" 'BEGIN { exit (d + 0 < 0.5) ? 0 : 1 }' || {
  echo "FAIL: pixdiff ${DIFF}% >= 0.5% threshold" >&2
  echo "      output preserved at $OUT for inspection" >&2
  trap - EXIT
  stop_silencer "$PID" "$PORT" || true
  exit 1
}

echo "PASS bank_text_test"
