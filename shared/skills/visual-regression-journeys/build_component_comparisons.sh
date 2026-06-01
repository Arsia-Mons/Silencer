#!/usr/bin/env bash
# Builds semantic component-level visual comparisons from existing full-scene
# captures. Each component is a manifest row that maps the semantic component
# to its origin/main and current screenshot crop.
#
# Required env:
#   BASE_DIR      - directory of origin/main captures
#   CURRENT_DIR   - directory of current-branch captures
#   OUT_DIR       - directory for component composites
#
# Optional env:
#   MANIFEST      - component TSV manifest (default: component_manifest.tsv)
#   PIXDIFF_BIN   - path to pixdiff
#   BASELINE_LABEL, CURRENT_LABEL
#   SEND_DISCORD=1 - send contact sheets to Discord DM

set -euo pipefail

: "${BASE_DIR:?must be set}"
: "${CURRENT_DIR:?must be set}"
: "${OUT_DIR:?must be set}"

cd "$(git rev-parse --show-toplevel)"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MANIFEST="${MANIFEST:-$SCRIPT_DIR/component_manifest.tsv}"
PIXDIFF_BIN="${PIXDIFF_BIN:-tools/pixdiff/build/pixdiff}"
BASELINE_LABEL="${BASELINE_LABEL:-origin/main}"
CURRENT_LABEL="${CURRENT_LABEL:-current}"
FONT="/System/Library/Fonts/Supplemental/Arial.ttf"
[ -f "$FONT" ] || FONT="/System/Library/Fonts/Helvetica.ttc"

if [ ! -f "$MANIFEST" ]; then
  echo "FAIL: component manifest missing: $MANIFEST" >&2
  exit 1
fi
if ! command -v magick >/dev/null 2>&1; then
  echo "FAIL: ImageMagick 'magick' not on PATH. Install with: brew install imagemagick" >&2
  exit 1
fi
if [ ! -x "$PIXDIFF_BIN" ]; then
  echo "FAIL: $PIXDIFF_BIN missing. Build with:" >&2
  echo "  cmake -B tools/pixdiff/build -S tools/pixdiff && cmake --build tools/pixdiff/build" >&2
  exit 1
fi

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR/crops" "$OUT_DIR/components" "$OUT_DIR/contact-sheets"

summary="$OUT_DIR/summary.tsv"
missing="$OUT_DIR/missing.txt"
: > "$summary"
: > "$missing"
printf 'category\tname\tscene\tpixdiff\tnote\n' > "$summary"

sanitize() {
  printf '%s' "$1" | tr -c '[:alnum:]_.-' '_'
}

component_label() {
  local category="$1"
  local name="$2"
  local scene="$3"
  local note="$4"
  printf '%s / %s\nscene: %s\n%s' "$category" "$name" "$scene" "$note"
}

build_component() {
  local index="$1"
  local category="$2"
  local name="$3"
  local scene="$4"
  local baseline_crop="$5"
  local current_crop="$6"
  local note="$7"

  local base_img="$BASE_DIR/$scene.png"
  local current_img="$CURRENT_DIR/$scene.png"
  if [ ! -f "$base_img" ] || [ ! -f "$current_img" ]; then
    printf '%s\t%s\t%s\tmissing scene capture\n' "$category" "$name" "$scene" >> "$missing"
    return 0
  fi

  local safe_category safe_name out_name base_crop_img current_crop_img
  safe_category="$(sanitize "$category")"
  safe_name="$(sanitize "$name")"
  out_name="$(printf '%03d_%s_%s' "$index" "$safe_category" "$safe_name")"
  base_crop_img="$OUT_DIR/crops/${out_name}_baseline.png"
  current_crop_img="$OUT_DIR/crops/${out_name}_current.png"

  magick "$base_img" -crop "$baseline_crop" +repage "$base_crop_img"
  magick "$current_img" -crop "$current_crop" +repage "$current_crop_img"

  local diff_pct
  diff_pct="$("$PIXDIFF_BIN" "$base_crop_img" "$current_crop_img" 2>/dev/null || printf 'n/a')"

  local label
  label="$(component_label "$category" "$name" "$scene" "$note")"
  magick \
    \( "$base_crop_img" -gravity North -background black -splice 0x34 \
       -font "$FONT" -fill white -pointsize 14 -annotate +0+8 "$BASELINE_LABEL" \) \
    \( "$current_crop_img" -gravity North -background black -splice 0x34 \
       -font "$FONT" -fill white -pointsize 14 -annotate +0+8 "$CURRENT_LABEL" \) \
    +append \
    -gravity Center -background "#111111" -extent '900x<' \
    -gravity North -background "#202020" -splice 0x78 \
    -font "$FONT" -fill white -pointsize 16 -annotate +0+8 "$label"$'\n'"pixdiff: ${diff_pct}%" \
    "$OUT_DIR/components/${out_name}.png"

  printf '%s\t%s\t%s\t%s\t%s\n' "$category" "$name" "$scene" "$diff_pct" "$note" >> "$summary"
  printf '  %s/%s %s%%\n' "$category" "$name" "$diff_pct"
}

echo ">>> Building component comparisons"
index=0
while IFS=$'\t' read -r category name scene baseline_crop current_crop note; do
  case "${category:-}" in
    ''|'#'*) continue ;;
  esac
  index=$((index + 1))
  build_component "$index" "$category" "$name" "$scene" "$baseline_crop" "$current_crop" "${note:-}"
done < "$MANIFEST"

if [ -s "$missing" ]; then
  echo "WARN: missing component inputs:"
  cat "$missing"
fi

echo ">>> Building contact sheets"
shopt -s nullglob
declare -a component_files=("$OUT_DIR"/components/*.png)
if [ "${#component_files[@]}" -eq 0 ]; then
  echo "FAIL: no component composites were generated" >&2
  exit 1
fi

magick montage "${component_files[@]}" \
  -font "$FONT" -thumbnail 1200x900 -tile 1x -geometry +0+18 \
  -background "#111111" "$OUT_DIR/contact-sheets/all-components.png"

cut -f1 "$summary" | tail -n +2 | sort -u | while read -r category; do
  [ -n "$category" ] || continue
  safe_category="$(sanitize "$category")"
  files=("$OUT_DIR"/components/*_"$safe_category"_*.png)
  [ "${#files[@]}" -gt 0 ] || continue
  magick montage "${files[@]}" \
    -font "$FONT" -thumbnail 1200x900 -tile 1x -geometry +0+18 \
    -background "#111111" "$OUT_DIR/contact-sheets/${safe_category}.png"
done

cat > "$OUT_DIR/manifest.txt" <<EOF
Silencer component visual comparison
baseline: $BASELINE_LABEL
current:  $CURRENT_LABEL
baseline captures: $BASE_DIR
current captures:  $CURRENT_DIR
manifest: $MANIFEST
output: $OUT_DIR
EOF

resolve_discord_send_script() {
  if [ -n "${DISCORD_DM_SEND_SCRIPT:-}" ] && [ -f "$DISCORD_DM_SEND_SCRIPT" ]; then
    printf '%s\n' "$DISCORD_DM_SEND_SCRIPT"
    return 0
  fi
  if [ -n "${CLAUDE_PLUGIN_ROOT:-}" ] && [ -f "$CLAUDE_PLUGIN_ROOT/skills/discord-dm/send.ts" ]; then
    printf '%s\n' "$CLAUDE_PLUGIN_ROOT/skills/discord-dm/send.ts"
    return 0
  fi
  if [ -f "/Users/hv/repos/hv-skills/discord-dm/skills/discord-dm/send.ts" ]; then
    printf '%s\n' "/Users/hv/repos/hv-skills/discord-dm/skills/discord-dm/send.ts"
    return 0
  fi
  return 1
}

send_discord_batches() {
  local send_script="$1"
  local delivery_log="$OUT_DIR/discord_delivery.txt"
  local max_bytes="${DISCORD_MAX_ATTACHMENT_BYTES:-23000000}"
  local max_files="${DISCORD_MAX_FILES_PER_MESSAGE:-8}"
  local -a files=()
  if [ "${DISCORD_INCLUDE_ALL_COMPONENTS:-0}" = "1" ]; then
    while IFS= read -r file; do
      files+=("$file")
    done < <(find "$OUT_DIR/contact-sheets" -maxdepth 1 -type f -name '*.png' | sort)
  else
    while IFS= read -r file; do
      files+=("$file")
    done < <(find "$OUT_DIR/contact-sheets" -maxdepth 1 -type f -name '*.png' ! -name 'all-components.png' | sort)
  fi

  if [ "${#files[@]}" -eq 0 ]; then
    echo "FAIL: no component contact sheets to DM from $OUT_DIR/contact-sheets" >&2
    exit 1
  fi

  : > "$delivery_log"
  local -a batch=()
  local batch_bytes=0
  local batch_index=1
  local file size

  flush_batch() {
    [ "${#batch[@]}" -gt 0 ] || return 0
    local message
    message="Silencer component visual comparison batch ${batch_index}: ${CURRENT_LABEL} vs ${BASELINE_LABEL}. Contact sheets attached."
    echo "  DM batch ${batch_index}: ${#batch[@]} files" | tee -a "$delivery_log"
    bun "$send_script" "$message" "${batch[@]}" 2>&1 | tee -a "$delivery_log"
    batch=()
    batch_bytes=0
    batch_index=$((batch_index + 1))
  }

  for file in "${files[@]}"; do
    size="$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file")"
    if [ "$size" -gt "$max_bytes" ]; then
      echo "  SKIP $(basename "$file"): ${size} bytes exceeds ${max_bytes}" | tee -a "$delivery_log"
      continue
    fi
    if [ "${#batch[@]}" -gt 0 ] && {
      [ $((batch_bytes + size)) -gt "$max_bytes" ] || [ "${#batch[@]}" -ge "$max_files" ];
    }; then
      flush_batch
    fi
    batch+=("$file")
    batch_bytes=$((batch_bytes + size))
  done
  flush_batch
}

if [ "${SEND_DISCORD:-0}" = "1" ]; then
  echo ">>> Sending component contact sheets to Discord DM"
  if ! send_script="$(resolve_discord_send_script)"; then
    echo "FAIL: Discord DM send script not found. Set DISCORD_DM_SEND_SCRIPT or CLAUDE_PLUGIN_ROOT." >&2
    exit 1
  fi
  send_discord_batches "$send_script"
else
  echo ">>> Skipping Discord DM delivery (SEND_DISCORD=0)"
fi

echo ""
echo ">>> Component comparison complete."
echo "    components:     $OUT_DIR/components"
echo "    contact sheets: $OUT_DIR/contact-sheets"
echo "    summary:        $summary"
