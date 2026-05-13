#!/usr/bin/env bash
# P10 Panel primitive unit test — drives `clay_panel_test` for both
# variants (right, bare) and pixdiffs the resulting PNGs against the
# committed references.
#
# Pass bar: < 1.0% pixdiff vs reference_<variant>.png for each.
#
# Usage:   bash tests/lobby-ui/panel_test/run.sh
# Updates: rerun with REGEN=1 to overwrite both reference PNGs from the live
#          binary.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"

if [ -z "${SILENCER_BIN:-}" ]; then
  if [ -x "$REPO/build/Silencer.app/Contents/MacOS/Silencer" ]; then
    export SILENCER_BIN="$REPO/build/Silencer.app/Contents/MacOS/Silencer"
  elif [ -x "$REPO/build/silencer" ]; then
    export SILENCER_BIN="$REPO/build/silencer"
  fi
fi

. "$REPO/tests/cli-agent/e2e/lib.sh"

PIXDIFF="$REPO/tools/pixdiff/build/pixdiff"
if [ ! -x "$PIXDIFF" ]; then
  echo "FAIL: pixdiff not built — run: cmake -S tools/pixdiff -B tools/pixdiff/build && cmake --build tools/pixdiff/build -j" >&2
  exit 1
fi

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
TMPS=()
cleanup() {
  stop_silencer "$PID" "$PORT" || true
  for t in "${TMPS[@]+"${TMPS[@]}"}"; do rm -f "$t"; done
}
trap cleanup EXIT
wait_alive "$PORT"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

FAILED=0
for VARIANT in right bare; do
  OUT="${TMPDIR:-/tmp}/panel_${VARIANT}.$$.png"
  TMPS+=("$OUT")
  REF="$HERE/reference_${VARIANT}.png"
  cli --port "$PORT" clay_panel_test --variant "$VARIANT" --out "$OUT" >/dev/null
  if [ "${REGEN:-0}" = "1" ]; then
    cp "$OUT" "$REF"
    echo "regenerated $REF"
    continue
  fi
  DIFF=$("$PIXDIFF" "$OUT" "$REF")
  echo "pixdiff[$VARIANT] = ${DIFF}%"
  if ! awk -v d="$DIFF" 'BEGIN { exit (d + 0 < 1.0) ? 0 : 1 }'; then
    echo "FAIL: pixdiff[$VARIANT] ${DIFF}% >= 1.0% threshold" >&2
    echo "      output preserved at $OUT for inspection" >&2
    for i in "${!TMPS[@]}"; do [ "${TMPS[$i]}" = "$OUT" ] && unset 'TMPS[i]'; done
    FAILED=1
  fi
done

if [ "${REGEN:-0}" = "1" ]; then
  echo "re-run without REGEN to verify"
  exit 0
fi

if [ "$FAILED" != "0" ]; then
  exit 1
fi
echo "PASS panel_test"
