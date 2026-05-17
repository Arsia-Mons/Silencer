#!/usr/bin/env bash
# P9 TextInput primitive unit test — drives `clay_text_input_test` once
# (one focused TextInput with caret, FieldLarge text size, text
# "Player1") and pixdiffs the resulting PNG against the committed
# reference. Then drives `clay_text_input_check` once for the
# typed-submit + password-mask checks.
#
# Pass bar (render):     < 1.0% pixdiff vs reference.png.
# Pass bar (behavioral): submit_actions_for_enter==1,
#                        submit_actions_for_text==0,
#                        password_mask_applied_len==8.
#
# Usage:   bash tests/lobby-ui/text_input_test/run.sh
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

OUT="${TMPDIR:-/tmp}/text_input.$$.png"
TMPS+=("$OUT")
REF="$HERE/reference.png"
cli --port "$PORT" clay_text_input_test --out "$OUT" >/dev/null
if [ "${REGEN:-0}" = "1" ]; then
  cp "$OUT" "$REF"
  echo "regenerated $REF"
  echo "re-run without REGEN to verify"
  exit 0
fi

DIFF=$("$PIXDIFF" "$OUT" "$REF")
echo "pixdiff = ${DIFF}%"
if ! awk -v d="$DIFF" 'BEGIN { exit (d + 0 < 1.0) ? 0 : 1 }'; then
  echo "FAIL: pixdiff ${DIFF}% >= 1.0% threshold" >&2
  echo "      output preserved at $OUT for inspection" >&2
  for i in "${!TMPS[@]}"; do [ "${TMPS[$i]}" = "$OUT" ] && unset 'TMPS[i]'; done
  FAILED=1
fi

# Typed-submit + password-mask check (no PNG; pure JSON).
CHECK=$(cli --port "$PORT" clay_text_input_check)
echo "check = $CHECK"
read ENTER_SUBMITS TEXT_SUBMITS PW_LEN <<EOF
$(bun -e "const j=JSON.parse(process.argv[1]); console.log([j.submit_actions_for_enter,j.submit_actions_for_text,j.password_mask_applied_len].join(' '))" "$CHECK")
EOF

assert_eq() {
  if [ "$2" != "$3" ]; then
    echo "FAIL: $1 expected $3, got $2" >&2
    FAILED=1
  else
    echo "PASS: $1 = $2"
  fi
}
assert_eq "submit_actions_for_enter" "$ENTER_SUBMITS" "1"
assert_eq "submit_actions_for_text"  "$TEXT_SUBMITS"  "0"
assert_eq "password_mask_applied_len"  "$PW_LEN"    "8"

if [ "$FAILED" != "0" ]; then
  exit 1
fi
echo "PASS text_input_test"
