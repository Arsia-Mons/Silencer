#!/usr/bin/env bash
# C1 Rectangle alpha-blend bridge test — drives
# `clay_rectangle_alpha_test` and pixdiffs the resulting PNG against the
# committed reference. The scene is a fully-opaque rect with a 50%-opaque
# rect overlapping its right half; the overlap region exercises the
# palette alphaed-LUT path the bridge gained in C1.
#
# Pass bar: < 0.5% pixdiff vs reference.png.
#
# Usage:   bash tests/lobby-clay/rectangle_alpha_test/run.sh
# Updates: rerun with REGEN=1 to overwrite reference.png from the live binary.

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

OUT="${TMPDIR:-/tmp}/rectangle_alpha.$$.png"
TMPS+=("$OUT")
REF="$HERE/reference.png"
cli --port "$PORT" clay_rectangle_alpha_test --out "$OUT" >/dev/null
if [ "${REGEN:-0}" = "1" ]; then
  cp "$OUT" "$REF"
  echo "regenerated $REF"
  echo "re-run without REGEN to verify"
  exit 0
fi

DIFF=$("$PIXDIFF" "$OUT" "$REF")
echo "pixdiff = ${DIFF}%"
if ! awk -v d="$DIFF" 'BEGIN { exit (d + 0 < 0.5) ? 0 : 1 }'; then
  echo "FAIL: pixdiff ${DIFF}% >= 0.5% threshold" >&2
  echo "      output preserved at $OUT for inspection" >&2
  for i in "${!TMPS[@]}"; do [ "${TMPS[$i]}" = "$OUT" ] && unset 'TMPS[i]'; done
  exit 1
fi
echo "PASS rectangle_alpha_test"
