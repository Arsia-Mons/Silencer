#!/bin/bash
# Ralph — Clay chrome pivot loop.
# See docs/plans/2026-05-11-lobby-clay-refactor.md → "Next milestone".
# Runs until every item in prd.json has passes: true (or MAX_ITERATIONS).
#
# This is the SECOND Ralph run on this worktree. The first one (lobby
# byte-identical migration to Clay) is complete and gone — only its
# commits in git history remain. This run replaces the baked rectangle
# outlines in the LobbyBG sprite with Clay-drawn rectangles + flex
# layout. The byte-identical pixdiff gate is GONE; verification is
# visual review + functional e2e.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RALPH_MD="$SCRIPT_DIR/RALPH.md"
PRD_JSON="$SCRIPT_DIR/prd.json"
MAX_ITERATIONS=40
LOG_FILE="$SCRIPT_DIR/ralph.log"

echo "Starting chrome-pivot Ralph loop..."
echo "Worktree: $REPO_ROOT"
echo "Max iterations: $MAX_ITERATIONS"
echo ""

for i in $(seq 1 $MAX_ITERATIONS); do
  echo "==============================================================="
  echo "  Chrome Ralph — Iteration $i of $MAX_ITERATIONS  $(date '+%F %T')"
  echo "==============================================================="

  if command -v jq &>/dev/null; then
    jq -r '.items[] | "  \(.id) [\(if .passes then "PASS" else "TODO" end)] \(.name)"' "$PRD_JSON" || true
  fi
  echo ""

  if command -v jq &>/dev/null && jq -e '.items | all(.passes == true)' "$PRD_JSON" >/dev/null; then
    echo "All items already pass — exiting."
    bun /Users/hv/.claude/skills/discord-dm/send.ts \
      "Chrome Ralph: all items pass at iteration $i." || true
    exit 0
  fi

  OUTPUT=$(cd "$REPO_ROOT" && claude --dangerously-skip-permissions --print < "$RALPH_MD" 2>&1 | tee /dev/stderr) || true

  {
    echo "--- Iteration $i $(date '+%F %T') ---"
    echo "$OUTPUT" | tail -30
    echo ""
  } >> "$LOG_FILE"

  if command -v jq &>/dev/null && jq -e '.items | all(.passes == true)' "$PRD_JSON" >/dev/null; then
    echo "Chrome Ralph completed all items at iteration $i."
    bun /Users/hv/.claude/skills/discord-dm/send.ts \
      "Chrome Ralph completed all $i iterations — every prd item passes." || true
    exit 0
  fi

  echo "Iteration $i complete. Continuing in 2 seconds..."
  sleep 2
done

echo "Chrome Ralph reached max iterations ($MAX_ITERATIONS). Inspect prd.json + progress.txt."
bun /Users/hv/.claude/skills/discord-dm/send.ts \
  "Chrome Ralph hit MAX_ITERATIONS=$MAX_ITERATIONS — needs human review." || true
exit 1
