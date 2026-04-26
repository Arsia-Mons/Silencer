#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKTREE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RALPH_MD="$SCRIPT_DIR/RALPH.md"
PRD="$SCRIPT_DIR/prd.json"
LOG_FILE="$SCRIPT_DIR/ralph.log"
MAX_ITERATIONS=25
echo "Starting Ralph (OPTIONSCONTROLS spec-only rebuild)..."
cd "$WORKTREE_ROOT"
for i in $(seq 1 $MAX_ITERATIONS); do
  echo "==============================================================="
  echo "  Ralph[controls] — Iteration $i of $MAX_ITERATIONS  $(date '+%F %T')"
  echo "==============================================================="
  if command -v jq &>/dev/null; then
    echo "Backlog:"
    jq -r '.items[] | "  \(.id) [\(if .passes then "PASS" else "TODO" end)] \(.name)"' "$PRD"
    echo ""
  fi
  OUTPUT=$(claude --dangerously-skip-permissions --print < "$RALPH_MD" 2>&1 | tee /dev/stderr) || true
  { echo "--- Iteration $i $(date '+%F %T') ---"; echo "$OUTPUT" | tail -20; echo ""; } >> "$LOG_FILE"
  if echo "$OUTPUT" | grep -q "<promise>COMPLETE</promise>"; then
    echo "Ralph[controls] completed all items at iteration $i."
    exit 0
  fi
  echo "Iteration $i complete. Continuing in 2 seconds..."
  sleep 2
done
echo "Ralph[controls] reached MAX_ITERATIONS ($MAX_ITERATIONS)."
exit 1
