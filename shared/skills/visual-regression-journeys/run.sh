#!/usr/bin/env bash
# Orchestrates the full visual-regression-journeys flow:
#  1. Verify pre-flight (binary built, pixdiff built, ImageMagick present).
#  2. Capture current branch into $CURRENT_DIR.
#  3. Create/refresh baseline worktree at $BASELINE_REF.
#  4. Build the baseline silencer if needed.
#  5. Capture baseline into $BASE_DIR.
#  6. Build composites into $COMP_DIR.
#  7. Print a sorted regression summary on stdout.
#
# Configurable via env:
#   BASELINE_REF        — git ref to diff against (default: origin/main)
#   CURRENT_DIR         — capture dir for current branch (default: /tmp/journeys-current)
#   BASE_DIR            — capture dir for baseline ref (default: /tmp/journeys-baseline)
#   COMP_DIR            — composite output dir (default: /tmp/journeys-composite)
#   WORKTREE            — baseline worktree path (default: /tmp/silencer-baseline)
#   SKIP_CURRENT=1      — re-use existing $CURRENT_DIR
#   SKIP_BASELINE=1     — re-use existing $BASE_DIR (skips checkout+build entirely)
#   PIXDIFF_BIN         — path to pixdiff (default: tools/pixdiff/build/pixdiff)

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

BASELINE_REF="${BASELINE_REF:-origin/main}"
CURRENT_DIR="${CURRENT_DIR:-/tmp/journeys-current}"
BASE_DIR="${BASE_DIR:-/tmp/journeys-baseline}"
COMP_DIR="${COMP_DIR:-/tmp/journeys-composite}"
WORKTREE="${WORKTREE:-/tmp/silencer-baseline}"
PIXDIFF_BIN="${PIXDIFF_BIN:-tools/pixdiff/build/pixdiff}"
SKILL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ---------- 1. Pre-flight ----------
echo ">>> Pre-flight"

if [ ! -x "build/Silencer.app/Contents/MacOS/Silencer" ] && [ ! -x "build/silencer" ]; then
  echo "FAIL: current branch silencer binary missing. Run 'cmake --build build --target silencer -j 8' first." >&2
  exit 1
fi

if [ ! -x "$PIXDIFF_BIN" ]; then
  echo "FAIL: $PIXDIFF_BIN missing. Build with:" >&2
  echo "  cmake -B tools/pixdiff/build -S tools/pixdiff && cmake --build tools/pixdiff/build" >&2
  exit 1
fi

if ! command -v magick >/dev/null 2>&1; then
  echo "FAIL: ImageMagick 'magick' not on PATH. Install with: brew install imagemagick" >&2
  exit 1
fi

if ! command -v rg >/dev/null 2>&1; then
  echo "WARN: ripgrep not installed; some E2E side-effects (boundary checks) will silently no-op." >&2
fi

echo "  OK"

# ---------- 2. Capture current ----------
if [ "${SKIP_CURRENT:-0}" = "1" ] && [ -d "$CURRENT_DIR" ]; then
  echo ">>> Reusing existing $CURRENT_DIR ($(ls "$CURRENT_DIR" | wc -l | tr -d ' ') files)"
else
  echo ">>> Capturing current branch -> $CURRENT_DIR"
  rm -rf "$CURRENT_DIR"
  OUT_DIR="$CURRENT_DIR" bash "$SKILL_DIR/capture_current.sh"
fi

# ---------- 3-5. Baseline ----------
if [ "${SKIP_BASELINE:-0}" = "1" ] && [ -d "$BASE_DIR" ]; then
  echo ">>> Reusing existing $BASE_DIR ($(ls "$BASE_DIR" | wc -l | tr -d ' ') files)"
else
  echo ">>> Preparing baseline worktree at $WORKTREE (ref $BASELINE_REF)"
  if [ -d "$WORKTREE" ]; then
    echo "  worktree exists; will rebuild in place"
  else
    git worktree add "$WORKTREE" "$BASELINE_REF" >/dev/null
  fi

  echo ">>> Building baseline silencer"
  (
    cd "$WORKTREE"
    cmake -B build -S clients/silencer >/dev/null 2>&1
    cmake --build build --target silencer -j 8 2>&1 | tail -2
  )

  echo ">>> Capturing baseline -> $BASE_DIR"
  rm -rf "$BASE_DIR"
  WORKTREE="$WORKTREE" OUT_DIR="$BASE_DIR" bash "$SKILL_DIR/capture_baseline.sh"
fi

# ---------- 6-7. Composites + summary ----------
echo ">>> Building composites -> $COMP_DIR"
rm -rf "$COMP_DIR"
mkdir -p "$COMP_DIR"
BASE_DIR="$BASE_DIR" CURRENT_DIR="$CURRENT_DIR" OUT_DIR="$COMP_DIR" PIXDIFF_BIN="$PIXDIFF_BIN" \
  bash "$SKILL_DIR/build_composites.sh"

echo ""
echo ">>> Visual regression run complete."
echo "    composites: $COMP_DIR"
echo "    baseline captures: $BASE_DIR"
echo "    current captures:  $CURRENT_DIR"
echo ""
echo "    Eyeball every composite. pixdiff alone is not a verdict — fade-in"
echo "    screens and animated logos can hit 40%+ without a real regression,"
echo "    and a real text-garble bug can hit <40%. The composites make both"
echo "    visible at a glance."
