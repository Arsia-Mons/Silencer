#!/bin/bash
# Ralph — autonomous Claude Code loop for the lobby → Clay refactor.
# See docs/plans/2026-05-11-lobby-clay-refactor.md for the design.
# Runs until every item in prd.json has passes: true (or MAX_ITERATIONS hit).

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RALPH_MD="$SCRIPT_DIR/RALPH.md"
PRD_JSON="$SCRIPT_DIR/prd.json"
MAX_ITERATIONS=60
LOG_FILE="$SCRIPT_DIR/ralph.log"

echo "Starting Ralph loop..."
echo "Worktree: $REPO_ROOT"
echo "Max iterations: $MAX_ITERATIONS"
echo ""

for i in $(seq 1 $MAX_ITERATIONS); do
  echo "==============================================================="
  echo "  Ralph — Iteration $i of $MAX_ITERATIONS  $(date '+%F %T')"
  echo "==============================================================="

  if command -v jq &>/dev/null; then
    jq -r '.items[] | "  \(.id) [\(if .passes then "PASS" else "TODO" end)] \(.name)"' "$PRD_JSON" || true
  fi
  echo ""

  # Pre-iteration check: are we already done?
  if command -v jq &>/dev/null && jq -e '.items | all(.passes == true)' "$PRD_JSON" >/dev/null; then
    echo "All items already pass — exiting."
    exit 0
  fi

  # Run from the REPO ROOT so build / cli paths work as-is.
  OUTPUT=$(cd "$REPO_ROOT" && claude --dangerously-skip-permissions --print < "$RALPH_MD" 2>&1 | tee /dev/stderr) || true

  {
    echo "--- Iteration $i $(date '+%F %T') ---"
    echo "$OUTPUT" | tail -30
    echo ""
  } >> "$LOG_FILE"

  # Robust stop signal: re-validate against prd.json — can't be tricked by
  # output-stream content. Fast-path: also accept the literal sentinel as the
  # last non-empty line.
  if command -v jq &>/dev/null && jq -e '.items | all(.passes == true)' "$PRD_JSON" >/dev/null; then
    echo "Ralph completed all items at iteration $i."
    # Final all-green DM.
    bun /Users/hv/.claude/skills/discord-dm/send.ts \
      "Ralph (lobby→Clay) completed all $i iterations — every prd item passes." || true
    exit 0
  fi
  LAST=$(echo "$OUTPUT" | tr -d '\r' | awk 'NF{l=$0} END{print l}')
  if [ "$(echo "$LAST" | tr -d '[:space:]')" = "<promise>COMPLETE</promise>" ]; then
    echo "Stop sentinel seen but prd.json not all-green — continuing (false positive guard)."
  fi

  echo "Iteration $i complete. Continuing in 2 seconds..."
  sleep 2
done

echo "Ralph reached max iterations ($MAX_ITERATIONS). Inspect prd.json + progress.txt."
bun /Users/hv/.claude/skills/discord-dm/send.ts \
  "Ralph (lobby→Clay) hit MAX_ITERATIONS=$MAX_ITERATIONS — needs human review." || true
exit 1
