#!/usr/bin/env bash
# Runs the assertion-bearing Clay UI primitive probes through the live
# control socket. No PNGs or visual references are produced.

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

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
cleanup() {
  stop_silencer "$PID" "$PORT" || true
}
trap cleanup EXIT
wait_alive "$PORT"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000 >/dev/null

FAILED=0

assert_eq() {
  if [ "$2" != "$3" ]; then
    echo "FAIL: $1 expected $3, got $2" >&2
    FAILED=1
  else
    echo "PASS: $1 = $2"
  fi
}

BUTTON=$(cli --port "$PORT" clay_button_check)
echo "button = $BUTTON"
read CLICK_PRESS CLICK_HELD HOVER_BR IDLE_BR CHROME_IDX OVAL_HOVER_IDX OVAL_HOVER_BR OVAL_UNHOVER_IDX OVAL_UNHOVER_BR OVAL_FOCUS_IDX OVAL_FOCUS_BR OVAL_WC_PARTIAL_IDX OVAL_WC_PARTIAL_BR OVAL_WC_NEXT_IDX OVAL_WC_NEXT_BR COMPACT_W COMPACT_H CHROME_AUTO_W CHROME_AUTO_H TEXT_COMPACT_W TEXT_COMPACT_H TEXT_COMPACT_XOFF TEXT_COMPACT_TEXT_W TEXT_COMPACT_YOFF AUTO_SHORT AUTO_LONG AUTO_MULTI_H <<EOF
$(bun -e "const j=JSON.parse(process.argv[1]); console.log([j.clicks_fired_on_press,j.clicks_fired_when_held,j.chrome_brightness_hover,j.chrome_brightness_idle,j.chrome_sprite_index_hover,j.oval_hover_sprite_indices.join(','),j.oval_hover_brightness.join(','),j.oval_unhover_sprite_indices.join(','),j.oval_unhover_brightness.join(','),j.oval_focus_sprite_index,j.oval_focus_brightness,j.oval_wall_clock_partial_sprite_index,j.oval_wall_clock_partial_brightness,j.oval_wall_clock_next_sprite_index,j.oval_wall_clock_next_brightness,j.compact_width,j.compact_height,j.chrome_auto_width,j.chrome_auto_height,j.text_compact_width,j.text_compact_height,j.text_compact_text_x_offset,j.text_compact_text_width,j.text_compact_text_y_offset,j.auto_short_width,j.auto_long_width,j.auto_multiline_height].join(' '))" "$BUTTON")
EOF

assert_eq "chrome_brightness_idle" "$IDLE_BR" "128"
assert_eq "chrome_brightness_hover" "$HOVER_BR" "136"
assert_eq "chrome_sprite_index_hover" "$CHROME_IDX" "24"
assert_eq "oval_hover_sprite_indices" "$OVAL_HOVER_IDX" "7,8,9,10,11"
assert_eq "oval_hover_brightness" "$OVAL_HOVER_BR" "128,130,132,134,136"
assert_eq "oval_unhover_sprite_indices" "$OVAL_UNHOVER_IDX" "11,10,9,8,7"
assert_eq "oval_unhover_brightness" "$OVAL_UNHOVER_BR" "136,134,132,130,128"
assert_eq "oval_focus_sprite_index" "$OVAL_FOCUS_IDX" "11"
assert_eq "oval_focus_brightness" "$OVAL_FOCUS_BR" "136"
assert_eq "oval_wall_clock_partial_sprite_index" "$OVAL_WC_PARTIAL_IDX" "7"
assert_eq "oval_wall_clock_partial_brightness" "$OVAL_WC_PARTIAL_BR" "128"
assert_eq "oval_wall_clock_next_sprite_index" "$OVAL_WC_NEXT_IDX" "8"
assert_eq "oval_wall_clock_next_brightness" "$OVAL_WC_NEXT_BR" "130"
assert_eq "clicks_fired_on_press" "$CLICK_PRESS" "1"
assert_eq "clicks_fired_when_held" "$CLICK_HELD" "0"
assert_eq "compact_width" "$COMPACT_W" "156"
assert_eq "compact_height" "$COMPACT_H" "21"
assert_eq "chrome_auto_width" "$CHROME_AUTO_W" "212"
assert_eq "chrome_auto_height" "$CHROME_AUTO_H" "21"
assert_eq "text_compact_width" "$TEXT_COMPACT_W" "52"
assert_eq "text_compact_height" "$TEXT_COMPACT_H" "21"
assert_eq "text_compact_text_y_offset" "$TEXT_COMPACT_YOFF" "8"

TEXT_COMPACT_RIGHT=$((TEXT_COMPACT_W - TEXT_COMPACT_XOFF - TEXT_COMPACT_TEXT_W))
if [ "$TEXT_COMPACT_XOFF" -lt 0 ] || [ "$TEXT_COMPACT_RIGHT" -lt 0 ] || [ $(( TEXT_COMPACT_XOFF > TEXT_COMPACT_RIGHT ? TEXT_COMPACT_XOFF - TEXT_COMPACT_RIGHT : TEXT_COMPACT_RIGHT - TEXT_COMPACT_XOFF )) -gt 1 ]; then
  echo "FAIL: text_compact horizontal ink centering left=$TEXT_COMPACT_XOFF right=$TEXT_COMPACT_RIGHT width=$TEXT_COMPACT_TEXT_W" >&2
  FAILED=1
else
  echo "PASS: text_compact horizontal ink centering left=$TEXT_COMPACT_XOFF right=$TEXT_COMPACT_RIGHT width=$TEXT_COMPACT_TEXT_W"
fi

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

TOGGLE=$(cli --port "$PORT" clay_toggle_check)
echo "toggle = $TOGGLE"
read C0 C1 C2 SEL_BR UNSEL_BR <<EOF
$(bun -e "const j=JSON.parse(process.argv[1]); console.log([j.clicks_toggle_0,j.clicks_toggle_1,j.clicks_toggle_2,j.selected_brightness,j.unselected_brightness].join(' '))" "$TOGGLE")
EOF
assert_eq "clicks_toggle_0" "$C0" "0"
assert_eq "clicks_toggle_1" "$C1" "1"
assert_eq "clicks_toggle_2" "$C2" "0"
assert_eq "selected_brightness" "$SEL_BR" "128"
assert_eq "unselected_brightness" "$UNSEL_BR" "32"

SCROLL_LIST=$(cli --port "$PORT" clay_scroll_list_check)
echo "scroll_list = $SCROLL_LIST"
read FIRED IDX NO_OFL_CNT OFL_CNT OFL_W OFL_H <<EOF
$(bun -e "const j=JSON.parse(process.argv[1]); console.log([j.select_actions,j.last_selected_index,j.no_overflow_scrollbar_count,j.overflow_scrollbar_count,j.overflow_scrollbar_bbox_w,j.overflow_scrollbar_bbox_h].join(' '))" "$SCROLL_LIST")
EOF
assert_eq "select_actions" "$FIRED" "1"
assert_eq "last_selected_index" "$IDX" "5"
assert_eq "no_overflow_scrollbar_count" "$NO_OFL_CNT" "0"
assert_eq "overflow_scrollbar_count" "$OFL_CNT" "1"
assert_eq "overflow_scrollbar_bbox_w" "$OFL_W" "8"
assert_eq "overflow_scrollbar_bbox_h" "$OFL_H" "130"

SCROLL_TEXT=$(cli --port "$PORT" clay_scroll_text_box_check)
echo "scroll_text_box = $SCROLL_TEXT"
read AT_BOT NOT_BOT OVERFLOW <<EOF
$(bun -e "const j=JSON.parse(process.argv[1]); console.log([j.at_bottom_prev_pos,j.not_at_bottom_prev_pos,j.at_bottom_overflow_prev_pos].join(' '))" "$SCROLL_TEXT")
EOF
assert_eq "at_bottom_prev_pos" "$AT_BOT" "1"
assert_eq "not_at_bottom_prev_pos" "$NOT_BOT" "0"
assert_eq "at_bottom_overflow_prev_pos" "$OVERFLOW" "6"

TEXT_INPUT=$(cli --port "$PORT" clay_text_input_check)
echo "text_input = $TEXT_INPUT"
read ENTER_SUBMITS TEXT_SUBMITS PW_LEN TAIL_LEN TAIL_OK <<EOF
$(bun -e "const j=JSON.parse(process.argv[1]); console.log([j.submit_actions_for_enter,j.submit_actions_for_text,j.password_mask_applied_len,j.overflow_tail_applied_len,j.overflow_tail_matches].join(' '))" "$TEXT_INPUT")
EOF
assert_eq "submit_actions_for_enter" "$ENTER_SUBMITS" "1"
assert_eq "submit_actions_for_text" "$TEXT_SUBMITS" "0"
assert_eq "password_mask_applied_len" "$PW_LEN" "8"
assert_eq "overflow_tail_applied_len" "$TAIL_LEN" "4"
assert_eq "overflow_tail_matches" "$TAIL_OK" "1"

if [ "$FAILED" != "0" ]; then
  exit 1
fi

echo "PASS clay_ui_checks"
