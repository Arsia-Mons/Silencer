#!/bin/bash
# Ralph — autonomous main-menu spec-only rebuild loop.
# Runs until every item in prd.json has passes: true (or MAX_ITERATIONS).

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKTREE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RALPH_MD="$SCRIPT_DIR/RALPH.md"
PRD="$SCRIPT_DIR/prd.json"
LOG_FILE="$SCRIPT_DIR/ralph.log"
MAX_ITERATIONS=30

echo "Starting Ralph (main-menu spec-only rebuild)..."
echo "Worktree: $WORKTREE_ROOT"
echo "Ralph dir: $SCRIPT_DIR"
echo "Max iterations: $MAX_ITERATIONS"
echo "Log: $LOG_FILE"
echo ""

cd "$WORKTREE_ROOT"

for i in $(seq 1 $MAX_ITERATIONS); do
  echo "==============================================================="
  echo "  Ralph — Iteration $i of $MAX_ITERATIONS  $(date '+%F %T')"
  echo "==============================================================="

  if command -v jq &>/dev/null; then
    echo "Backlog:"
    jq -r '.items[] | "  \(.id) [\(if .passes then "PASS" else "TODO" end)] \(.name)"' "$PRD"
    echo ""
  fi

  OUTPUT=$(claude --dangerously-skip-permissions --print < "$RALPH_MD" 2>&1 | tee /dev/stderr) || true

  {
    echo "--- Iteration $i $(date '+%F %T') ---"
    echo "$OUTPUT" | tail -20
    echo ""
  } >> "$LOG_FILE"

  if echo "$OUTPUT" | grep -q "<promise>COMPLETE</promise>"; then
    echo ""
    echo "==============================================================="
    echo "  Ralph completed all items at iteration $i."
    echo "==============================================================="
    exit 0
  fi

  echo ""
  echo "Iteration $i complete. Continuing in 2 seconds..."
  sleep 2
done

echo ""
echo "Ralph reached MAX_ITERATIONS ($MAX_ITERATIONS). Inspect prd.json + progress.txt."
exit 1
