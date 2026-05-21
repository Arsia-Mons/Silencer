#!/usr/bin/env bash
# Build one annotated side-by-side comparison sheet from either:
#   --left ref.png --right current.png
# or:
#   --left ref_dir --right current_dir
# In directory mode, files are paired by identical basename.

set -euo pipefail

LEFT=""
RIGHT=""
OUT=""
GLOB="*.png"
LEFT_LABEL="reference"
RIGHT_LABEL="current"
SIDE_WIDTH="640"

usage() {
  cat >&2 <<'EOF'
usage: stitch_compare.sh --left PATH --right PATH --out FILE [options]

options:
  --glob PATTERN          directory mode filename glob (default: *.png)
  --left-label TEXT       label over left image (default: reference)
  --right-label TEXT      label over right image (default: current)
  --side-width PX         resize each side to this width (default: 640)

examples:
  stitch_compare.sh --left refs --right current --out compare.png
  stitch_compare.sh --left ref.png --right actual.png --out pair.png
EOF
  exit 2
}

while [ $# -gt 0 ]; do
  case "$1" in
    --left) LEFT="${2:-}"; shift 2 ;;
    --right) RIGHT="${2:-}"; shift 2 ;;
    --out) OUT="${2:-}"; shift 2 ;;
    --glob) GLOB="${2:-}"; shift 2 ;;
    --left-label) LEFT_LABEL="${2:-}"; shift 2 ;;
    --right-label) RIGHT_LABEL="${2:-}"; shift 2 ;;
    --side-width) SIDE_WIDTH="${2:-}"; shift 2 ;;
    -h|--help) usage ;;
    *) echo "unknown argument: $1" >&2; usage ;;
  esac
done

[ -n "$LEFT" ] && [ -n "$RIGHT" ] && [ -n "$OUT" ] || usage
command -v magick >/dev/null 2>&1 || {
  echo "ImageMagick 'magick' is required. Install with: brew install imagemagick" >&2
  exit 1
}

FONT="/System/Library/Fonts/Supplemental/Arial.ttf"
[ -f "$FONT" ] || FONT="/System/Library/Fonts/Helvetica.ttc"

TMP="$(mktemp -d)"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

mkdir -p "$(dirname "$OUT")"

make_row() {
  local left_file="$1"
  local right_file="$2"
  local name="$3"
  local row="$4"

  magick \
    \( "$left_file" -resize "${SIDE_WIDTH}x" -gravity North -background black -splice 0x30 -font "$FONT" -fill white -pointsize 16 -annotate +0+8 "$LEFT_LABEL" \) \
    \( "$right_file" -resize "${SIDE_WIDTH}x" -gravity North -background black -splice 0x30 -font "$FONT" -fill white -pointsize 16 -annotate +0+8 "$RIGHT_LABEL" \) \
    +append \
    -gravity North -background "#202020" -splice 0x34 \
    -font "$FONT" -fill white -pointsize 18 -annotate +0+8 "$name" \
    "$row"
}

declare -a rows=()

if [ -f "$LEFT" ] && [ -f "$RIGHT" ]; then
  row="$TMP/row_0000.png"
  make_row "$LEFT" "$RIGHT" "$(basename "$LEFT")" "$row"
  rows+=("$row")
elif [ -d "$LEFT" ] && [ -d "$RIGHT" ]; then
  count=0
  while IFS= read -r left_file; do
    base="$(basename "$left_file")"
    right_file="$RIGHT/$base"
    if [ ! -f "$right_file" ]; then
      echo "warning: missing right image for $base" >&2
      continue
    fi
    printf -v row '%s/row_%04d.png' "$TMP" "$count"
    make_row "$left_file" "$right_file" "$base" "$row"
    rows+=("$row")
    count=$((count + 1))
  done < <(find "$LEFT" -maxdepth 1 -type f -name "$GLOB" | sort)
else
  echo "--left and --right must both be files or both be directories" >&2
  exit 2
fi

if [ ${#rows[@]} -eq 0 ]; then
  echo "no comparable image pairs found" >&2
  exit 1
fi

magick "${rows[@]}" -append "$OUT"
echo "$OUT"
