#!/usr/bin/env bash
# P5 Button primitive unit test — drives the `clay_button_test`
# control op across the public variant+size mappings. The legacy compact
# chrome baseline is pixdiffed; newer mappings are captured as render
# smoke probes unless a matching reference is committed. Also drives
# `clay_button_check` to validate hover-state parity, stable bounds,
# responsive oval sizing, and click-fire-once-per-press parity.
#
# Usage:   bash tests/lobby-ui/button_test/run.sh
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

CASES=(
  "chrome compact default reference_chrome.png"
  "oval sm default reference_oval_sm.png"
  "oval md default reference_oval_md.png"
  "oval md long reference_oval_md_long.png"
  "oval lg default reference_oval_lg.png"
  "oval auto short reference_oval_auto_short.png"
  "oval auto long reference_oval_auto_long.png"
  "oval auto multiline reference_oval_auto_multiline.png"
  "text compact default reference_text_compact.png"
  "ghost auto default reference_ghost_auto.png"
)

for CASE in "${CASES[@]}"; do
  read -r VARIANT SIZE LABEL REF_NAME <<<"$CASE"
  OUT="${TMPDIR:-/tmp}/button_${VARIANT}_${SIZE}_${LABEL}.$$.png"
  TMPS+=("$OUT")
  REF="$HERE/$REF_NAME"
  cli --port "$PORT" clay_button_test --variant "$VARIANT" --size "$SIZE" --label "$LABEL" --out "$OUT" >/dev/null
  if [ "${REGEN:-0}" = "1" ]; then
    cp "$OUT" "$REF"
    echo "regenerated $REF"
    continue
  fi
  if [ ! -f "$REF" ]; then
    echo "render[$VARIANT/$SIZE/$LABEL] = $OUT"
    continue
  fi
  DIFF=$("$PIXDIFF" "$OUT" "$REF")
  echo "pixdiff[$VARIANT/$SIZE/$LABEL] = ${DIFF}%"
  if ! awk -v d="$DIFF" 'BEGIN { exit (d + 0 < 1.0) ? 0 : 1 }'; then
    echo "FAIL: pixdiff[$VARIANT/$SIZE/$LABEL] ${DIFF}% >= 1.0% threshold" >&2
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
CHECK=$(cli --port "$PORT" clay_button_check)
echo "check = $CHECK"
# Extract fields with a tiny Bun one-liner — jq isn't required.
read CLICK_PRESS CLICK_HELD HOVER_BR IDLE_BR COMPACT_W COMPACT_H AUTO_SHORT AUTO_LONG AUTO_MULTI_H <<EOF
$(bun -e "const j=JSON.parse(process.argv[1]); console.log([j.clicks_fired_on_press,j.clicks_fired_when_held,j.chrome_brightness_hover,j.chrome_brightness_idle,j.compact_width,j.compact_height,j.auto_short_width,j.auto_long_width,j.auto_multiline_height].join(' '))" "$CHECK")
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
assert_eq "compact_width"           "$COMPACT_W"   "156"
assert_eq "compact_height"          "$COMPACT_H"   "21"
if ! awk -v s="$AUTO_SHORT" -v l="$AUTO_LONG" 'BEGIN { exit (l > s) ? 0 : 1 }'; then
  echo "FAIL: auto_long_width expected greater than auto_short_width, got $AUTO_LONG <= $AUTO_SHORT" >&2
  FAILED=1
else
  echo "PASS: auto width grows $AUTO_SHORT -> $AUTO_LONG"
fi
if ! awk -v h="$AUTO_MULTI_H" 'BEGIN { exit (h > 33) ? 0 : 1 }'; then
  echo "FAIL: auto_multiline_height expected > 33, got $AUTO_MULTI_H" >&2
  FAILED=1
else
  echo "PASS: auto_multiline_height = $AUTO_MULTI_H"
fi

if [ "$FAILED" != "0" ]; then
  exit 1
fi
echo "PASS button_test"
