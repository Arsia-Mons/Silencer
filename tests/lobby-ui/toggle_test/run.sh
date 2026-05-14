#!/usr/bin/env bash
# P6 Toggle primitive unit test — drives `clay_toggle_test` twice (once
# per state: selected / unselected) and `clay_toggle_check` once for the
# click-routing + per-state brightness check.
#
# Pass bar (render):     < 1.0% pixdiff per state vs committed reference.
# Pass bar (behavioral): toggle 1 emits exactly one Activate action on the
#                        press window over its bbox; toggles 0 and 2 do
#                        not fire; selected payload brightness == 128;
#                        unselected payload brightness == 32.
#
# Usage:   bash tests/lobby-ui/toggle_test/run.sh
# Updates: rerun with REGEN=1 to overwrite reference_*.png from the
#          live binary.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"

# The cli-agent harness auto-detects `clients/silencer/build/...` first;
# in this worktree the canonical build sits at the worktree-root `build/`,
# so we MUST set SILENCER_BIN BEFORE sourcing lib.sh — otherwise lib.sh
# picks up a stale sibling binary.
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

for STATE in selected unselected; do
  OUT="${TMPDIR:-/tmp}/toggle_${STATE}.$$.png"
  TMPS+=("$OUT")
  REF="$HERE/reference_${STATE}.png"
  cli --port "$PORT" clay_toggle_test --state "$STATE" --out "$OUT" >/dev/null
  if [ "${REGEN:-0}" = "1" ]; then
    cp "$OUT" "$REF"
    echo "regenerated $REF"
    continue
  fi
  DIFF=$("$PIXDIFF" "$OUT" "$REF")
  echo "pixdiff[$STATE] = ${DIFF}%"
  if ! awk -v d="$DIFF" 'BEGIN { exit (d + 0 < 1.0) ? 0 : 1 }'; then
    echo "FAIL: pixdiff[$STATE] ${DIFF}% >= 1.0% threshold" >&2
    echo "      output preserved at $OUT for inspection" >&2
    for i in "${!TMPS[@]}"; do [ "${TMPS[$i]}" = "$OUT" ] && unset 'TMPS[i]'; done
    FAILED=1
  fi
done

if [ "${REGEN:-0}" = "1" ]; then
  echo "regenerated all references — re-run without REGEN to verify"
  exit 0
fi

# Click-routing + brightness parity check (no PNG; pure JSON).
CHECK=$(cli --port "$PORT" clay_toggle_check)
echo "check = $CHECK"
read C0 C1 C2 SEL_BR UNSEL_BR <<EOF
$(bun -e "const j=JSON.parse(process.argv[1]); console.log([j.clicks_toggle_0,j.clicks_toggle_1,j.clicks_toggle_2,j.selected_brightness,j.unselected_brightness].join(' '))" "$CHECK")
EOF

assert_eq() {
  if [ "$2" != "$3" ]; then
    echo "FAIL: $1 expected $3, got $2" >&2
    FAILED=1
  else
    echo "PASS: $1 = $2"
  fi
}
assert_eq "clicks_toggle_0"      "$C0"       "0"
assert_eq "clicks_toggle_1"      "$C1"       "1"
assert_eq "clicks_toggle_2"      "$C2"       "0"
assert_eq "selected_brightness"  "$SEL_BR"   "128"
assert_eq "unselected_brightness" "$UNSEL_BR" "32"

if [ "$FAILED" != "0" ]; then
  exit 1
fi
echo "PASS toggle_test"
