#!/usr/bin/env bash
# P5 BankButton primitive unit test — drives the `clay_bank_button_test`
# control op three times (one per variant: chrome / inline / checkbox),
# dumping a 640x480 PNG each time and pixdiffing against the committed
# references. Also drives `clay_bank_button_check` to validate hover-state
# parity (brightness 128 idle, 136 hovered) and click-fire-once-per-press
# parity. Pass bar (render): < 1.0% per variant.
#
# Usage:   bash tests/lobby-ui/bank_button_test/run.sh
# Updates: rerun with REGEN=1 to overwrite reference_*.png from the live binary.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"

# The cli-agent harness auto-detects `clients/silencer/build/...` first; in
# this worktree the canonical build sits at the worktree-root `build/`, so
# we MUST set SILENCER_BIN BEFORE sourcing lib.sh — otherwise lib.sh picks
# up a stale sibling binary (see progress.txt iteration 2026-05-11T??:??Z
# bank_text_test).
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

for VARIANT in chrome inline checkbox; do
  OUT="${TMPDIR:-/tmp}/bank_button_${VARIANT}.$$.png"
  TMPS+=("$OUT")
  REF="$HERE/reference_${VARIANT}.png"
  cli --port "$PORT" clay_bank_button_test --variant "$VARIANT" --out "$OUT" >/dev/null
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
    # keep the file for inspection — clear from TMPS
    for i in "${!TMPS[@]}"; do [ "${TMPS[$i]}" = "$OUT" ] && unset 'TMPS[i]'; done
    FAILED=1
  fi
done

if [ "${REGEN:-0}" = "1" ]; then
  echo "regenerated all references — re-run without REGEN to verify"
  exit 0
fi

# Hover + click parity check (no PNG; pure JSON).
CHECK=$(cli --port "$PORT" clay_bank_button_check)
echo "check = $CHECK"
# Extract fields with a tiny Bun one-liner — jq isn't required.
read CLICK_PRESS CLICK_HELD HOVER_BR IDLE_BR <<EOF
$(bun -e "const j=JSON.parse(process.argv[1]); console.log([j.clicks_fired_on_press,j.clicks_fired_when_held,j.chrome_brightness_hover,j.chrome_brightness_idle].join(' '))" "$CHECK")
EOF

assert_eq() {
  if [ "$2" != "$3" ]; then
    echo "FAIL: $1 expected $3, got $2" >&2
    FAILED=1
  else
    echo "PASS: $1 = $2"
  fi
}
assert_eq "chrome_brightness_idle"  "$IDLE_BR"     "128"
assert_eq "chrome_brightness_hover" "$HOVER_BR"    "136"
assert_eq "clicks_fired_on_press"   "$CLICK_PRESS" "1"
assert_eq "clicks_fired_when_held"  "$CLICK_HELD"  "0"

if [ "$FAILED" != "0" ]; then
  exit 1
fi
echo "PASS bank_button_test"
