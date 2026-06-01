#!/usr/bin/env bash
# Builds side-by-side composite PNGs comparing baseline vs current for
# every screenshot pair found in BASE_DIR and CURRENT_DIR. Annotates each
# composite with the journey label and the pixdiff %.
#
# Required env:
#   BASE_DIR     — directory of baseline captures
#   CURRENT_DIR  — directory of current-branch captures
#   OUT_DIR      — directory for composite output (created if missing)
#   PIXDIFF_BIN  — path to the pixdiff binary (default: tools/pixdiff/build/pixdiff)
#
# Each pair is named after the file shared between the two dirs. Files
# present in only one directory are noted in the summary but no composite
# is built for them (single-side coverage).

set -uo pipefail

: "${BASE_DIR:?must be set}"
: "${CURRENT_DIR:?must be set}"
: "${OUT_DIR:?must be set}"
PIXDIFF_BIN="${PIXDIFF_BIN:-tools/pixdiff/build/pixdiff}"
BASELINE_LABEL="${BASELINE_LABEL:-baseline}"
CURRENT_LABEL="${CURRENT_LABEL:-current}"
mkdir -p "$OUT_DIR"
shopt -s nullglob

FONT="/System/Library/Fonts/Supplemental/Arial.ttf"
[ -f "$FONT" ] || FONT="/System/Library/Fonts/Helvetica.ttc"

# Human-friendly labels keyed by canonical filename
label_for() {
  case "$1" in
    01_mainmenu_640x480)        echo "Main Menu" ;;
    02_options_root_640x480)    echo "Options Root" ;;
    03_options_controls_640x480) echo "Options Controls" ;;
    04_options_display_640x480) echo "Options Display" ;;
    05_options_audio_640x480)   echo "Options Audio" ;;
    06_lobby_connect_640x480)   echo "Lobby Connect" ;;
    11_mainmenu_1280x720)       echo "Main Menu (1280x720)" ;;
    12_options_root_1280x720)   echo "Options Root (1280x720)" ;;
    13_options_controls_1280x720) echo "Options Controls (1280x720)" ;;
    20_ingame_hud_640x480)      echo "In-Game HUD" ;;
    21_ingame_playerlist_640x480) echo "In-Game Player List" ;;
    22_ingame_buy_640x480)      echo "In-Game Buy Menu" ;;
    23_ingame_tech_640x480)     echo "In-Game Tech Menu" ;;
    24_ingame_chat_640x480)     echo "In-Game Chat Overlay" ;;
    25_ingame_hud_1280x720)     echo "In-Game HUD (1280x720)" ;;
    *) echo "$1" ;;
  esac
}

build_one() {
  local name="$1"
  local label
  label="$(label_for "$name")"
  local diff_pct
  diff_pct=$("$PIXDIFF_BIN" "$BASE_DIR/$name.png" "$CURRENT_DIR/$name.png" 2>/dev/null)
  magick \
    \( "$BASE_DIR/$name.png" -gravity North -background black -splice 0x30 -font "$FONT" -fill white -pointsize 14 -annotate +0+8 "$BASELINE_LABEL" \) \
    \( "$CURRENT_DIR/$name.png" -gravity North -background black -splice 0x30 -font "$FONT" -fill white -pointsize 14 -annotate +0+8 "$CURRENT_LABEL" \) \
    +append \
    -gravity North -background "#222222" -splice 0x40 \
    -font "$FONT" -fill white -pointsize 18 -annotate +0+10 "${label}   pixdiff: ${diff_pct}%" \
    "$OUT_DIR/$name.png"
  echo "$diff_pct $name"
}

declare -a diff_lines=()
echo "--- pairs ---"
for f in "$BASE_DIR"/*.png; do
  [ -f "$f" ] || continue
  name="$(basename "$f" .png)"
  if [ -f "$CURRENT_DIR/$name.png" ]; then
    line=$(build_one "$name")
    echo "  $line"
    diff_lines+=("$line")
  fi
done

# Sort by diff value, highest first — surfaces real regressions
echo ""
echo "=== composites sorted by pixdiff (highest first) ==="
if [ ${#diff_lines[@]} -gt 0 ]; then
  printf '%s\n' "${diff_lines[@]}" | sort -rn
else
  echo "  (no pairs found — check that BASE_DIR and CURRENT_DIR share filenames)"
fi

echo ""
echo "=== one-sided coverage ==="
for f in "$BASE_DIR"/*.png; do
  name="$(basename "$f" .png)"
  [ -f "$CURRENT_DIR/$name.png" ] || echo "  baseline only: $name"
done
for f in "$CURRENT_DIR"/*.png; do
  name="$(basename "$f" .png)"
  [ -f "$BASE_DIR/$name.png" ] || echo "  current only: $name"
done
