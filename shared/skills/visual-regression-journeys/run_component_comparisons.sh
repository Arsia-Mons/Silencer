#!/usr/bin/env bash
# End-to-end semantic component visual comparison:
#   1. Capture current branch and origin/main scene screenshots via run.sh.
#   2. Crop semantic components from each side using component_manifest.tsv.
#   3. Stitch per-component side-by-side comparisons.
#   4. Build category/all-component contact sheets.
#   5. Optionally DM the contact sheets.
#
# This intentionally reuses the full-scene journey captures so every component
# comparison is backed by a real rendered client screenshot from both refs.

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASELINE_REF="${BASELINE_REF:-origin/main}"
CURRENT_DIR="${CURRENT_DIR:-/tmp/silencer-component-scenes-current}"
BASE_DIR="${BASE_DIR:-/tmp/silencer-component-scenes-baseline}"
SCENE_COMP_DIR="${SCENE_COMP_DIR:-/tmp/silencer-component-scenes-composite}"
COMPONENT_DIR="${COMPONENT_DIR:-/tmp/silencer-component-comparisons}"
WORKTREE="${WORKTREE:-/tmp/silencer-origin-main-component-visual}"
PIXDIFF_BIN="${PIXDIFF_BIN:-tools/pixdiff/build/pixdiff}"

CURRENT_LABEL="${CURRENT_LABEL:-$(git rev-parse --abbrev-ref HEAD)@$(git rev-parse --short HEAD)}"
BASELINE_LABEL="${BASELINE_LABEL:-$BASELINE_REF}"

echo ">>> Capturing scene screenshots for component comparison"
SEND_DISCORD=0 \
BASELINE_REF="$BASELINE_REF" \
CURRENT_DIR="$CURRENT_DIR" \
BASE_DIR="$BASE_DIR" \
COMP_DIR="$SCENE_COMP_DIR" \
WORKTREE="$WORKTREE" \
PIXDIFF_BIN="$PIXDIFF_BIN" \
BUILD_CURRENT="${BUILD_CURRENT:-1}" \
BUILD_BASELINE="${BUILD_BASELINE:-1}" \
FETCH_BASELINE="${FETCH_BASELINE:-1}" \
SKIP_CURRENT="${SKIP_CURRENT:-0}" \
SKIP_BASELINE="${SKIP_BASELINE:-0}" \
bash "$SCRIPT_DIR/run.sh"

echo ">>> Building semantic component comparisons"
BASE_DIR="$BASE_DIR" \
CURRENT_DIR="$CURRENT_DIR" \
OUT_DIR="$COMPONENT_DIR" \
PIXDIFF_BIN="$PIXDIFF_BIN" \
BASELINE_LABEL="$BASELINE_LABEL" \
CURRENT_LABEL="$CURRENT_LABEL" \
SEND_DISCORD="${SEND_DISCORD:-1}" \
DISCORD_DM_SEND_SCRIPT="${DISCORD_DM_SEND_SCRIPT:-}" \
bash "$SCRIPT_DIR/build_component_comparisons.sh"

echo ""
echo ">>> Component visual workflow complete."
echo "    scene composites:     $SCENE_COMP_DIR"
echo "    component composites: $COMPONENT_DIR/components"
echo "    component sheets:     $COMPONENT_DIR/contact-sheets"
