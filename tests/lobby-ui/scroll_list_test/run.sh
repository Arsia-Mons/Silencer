#!/usr/bin/env bash
# P7 ScrollList primitive unit test — drives `clay_scroll_list_test` once
# (30-item list, selectedIndex=8, scrollPosition=3) and pixdiffs the
# resulting PNG against the committed reference. Then drives
# `clay_scroll_list_check` once for action-routing parity (press over
# row 5 emits Select exactly once with index=5).
#
# Pass bar (render):     < 2.0% pixdiff vs reference.png.
# Pass bar (behavioral): select_actions == 1, last_selected_index == 5.
#
# Usage:   bash tests/lobby-ui/scroll_list_test/run.sh
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

OUT="${TMPDIR:-/tmp}/scroll_list.$$.png"
TMPS+=("$OUT")
REF="$HERE/reference.png"
cli --port "$PORT" clay_scroll_list_test --out "$OUT" >/dev/null
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

# Click-routing + conditional-scrollbar check (no PNG; pure JSON).
CHECK=$(cli --port "$PORT" clay_scroll_list_check)
echo "check = $CHECK"
read FIRED IDX NO_OFL_CNT OFL_CNT OFL_W OFL_H <<EOF
$(bun -e "const j=JSON.parse(process.argv[1]); console.log([j.select_actions,j.last_selected_index,j.no_overflow_scrollbar_count,j.overflow_scrollbar_count,j.overflow_scrollbar_bbox_w,j.overflow_scrollbar_bbox_h].join(' '))" "$CHECK")
EOF

assert_eq() {
  if [ "$2" != "$3" ]; then
    echo "FAIL: $1 expected $3, got $2" >&2
    FAILED=1
  else
    echo "PASS: $1 = $2"
  fi
}
assert_eq "select_actions"             "$FIRED"      "1"
assert_eq "last_selected_index"        "$IDX"        "5"
# P7b — conditional scrollbar emission. itemCount=3, visibleLines=10 → 0
# scrollbar render commands. itemCount=50, visibleLines=10 → 1 command, and
# its bbox spans the configured scrollbarWidth (8) × height (130).
assert_eq "no_overflow_scrollbar_count" "$NO_OFL_CNT" "0"
assert_eq "overflow_scrollbar_count"    "$OFL_CNT"    "1"
assert_eq "overflow_scrollbar_bbox_w"   "$OFL_W"      "8"
assert_eq "overflow_scrollbar_bbox_h"   "$OFL_H"      "130"

if [ "$FAILED" != "0" ]; then
  exit 1
fi
echo "PASS scroll_list_test"
