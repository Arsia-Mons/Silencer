#!/usr/bin/env bash
# P8 ScrollTextBox primitive unit test — drives `clay_scroll_text_box_test`
# once (6-line text box, top-down origin, no scroll) and pixdiffs the
# resulting PNG against the committed reference. Then drives
# `clay_scroll_text_box_check` once for the auto-scroll helper's three
# scenarios.
#
# Pass bar (render):     < 2.0% pixdiff vs reference.png.
# Pass bar (behavioral): at_bottom_prev_pos==1, not_at_bottom_prev_pos==0,
#                        at_bottom_overflow_prev_pos==6.
#
# Usage:   bash tests/lobby-clay/scroll_text_box_test/run.sh
# Updates: rerun with REGEN=1 to overwrite reference.png from the live
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

OUT="${TMPDIR:-/tmp}/scroll_text_box.$$.png"
TMPS+=("$OUT")
REF="$HERE/reference.png"
cli --port "$PORT" clay_scroll_text_box_test --out "$OUT" >/dev/null
if [ "${REGEN:-0}" = "1" ]; then
  cp "$OUT" "$REF"
  echo "regenerated $REF"
  echo "re-run without REGEN to verify"
  exit 0
fi

DIFF=$("$PIXDIFF" "$OUT" "$REF")
echo "pixdiff = ${DIFF}%"
if ! awk -v d="$DIFF" 'BEGIN { exit (d + 0 < 2.0) ? 0 : 1 }'; then
  echo "FAIL: pixdiff ${DIFF}% >= 2.0% threshold" >&2
  echo "      output preserved at $OUT for inspection" >&2
  for i in "${!TMPS[@]}"; do [ "${TMPS[$i]}" = "$OUT" ] && unset 'TMPS[i]'; done
  FAILED=1
fi

# Auto-scroll helper check (no PNG; pure JSON).
CHECK=$(cli --port "$PORT" clay_scroll_text_box_check)
echo "check = $CHECK"
read AT_BOT NOT_BOT OVERFLOW <<EOF
$(bun -e "const j=JSON.parse(process.argv[1]); console.log([j.at_bottom_prev_pos,j.not_at_bottom_prev_pos,j.at_bottom_overflow_prev_pos].join(' '))" "$CHECK")
EOF

assert_eq() {
  if [ "$2" != "$3" ]; then
    echo "FAIL: $1 expected $3, got $2" >&2
    FAILED=1
  else
    echo "PASS: $1 = $2"
  fi
}
assert_eq "at_bottom_prev_pos"          "$AT_BOT"   "1"
assert_eq "not_at_bottom_prev_pos"      "$NOT_BOT"  "0"
assert_eq "at_bottom_overflow_prev_pos" "$OVERFLOW" "6"

if [ "$FAILED" != "0" ]; then
  exit 1
fi
echo "PASS scroll_text_box_test"
