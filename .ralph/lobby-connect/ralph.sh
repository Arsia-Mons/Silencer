#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKTREE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RALPH_MD="$SCRIPT_DIR/RALPH.md"
PRD="$SCRIPT_DIR/prd.json"
LOG_FILE="$SCRIPT_DIR/ralph.log"
MAX_ITERATIONS=25
echo "Starting Ralph (LOBBYCONNECT spec-only rebuild)..."
cd "$WORKTREE_ROOT"
for i in $(seq 1 $MAX_ITERATIONS); do
  echo "==============================================================="
  echo "  Ralph[lobby-connect] — Iteration $i of $MAX_ITERATIONS  $(date '+%F %T')"
  echo "==============================================================="
  if command -v jq &>/dev/null; then
    echo "Backlog:"
    jq -r '.items[] | "  \(.id) [\(if .passes then "PASS" else "TODO" end)] \(.name)"' "$PRD"
    echo ""
  fi
  OUTPUT=$(claude --dangerously-skip-permissions --print < "$RALPH_MD" 2>&1 | tee /dev/stderr) || true
  { echo "--- Iteration $i $(date '+%F %T') ---"; echo "$OUTPUT" | tail -20; echo ""; } >> "$LOG_FILE"

  # Belt-and-braces stop check: jq against prd.json (robust to agent
  # output content) plus tail-grep for the canonical sentinel.
  if jq -e '.items | all(.passes == true)' "$PRD" >/dev/null 2>&1; then
    echo "Ralph[lobby-connect] all items pass at iteration $i (jq-validated)."
    exit 0
  fi
  if [ "$(echo "$OUTPUT" | tail -n 1 | tr -d '[:space:]')" = "<promise>COMPLETE</promise>" ]; then
    echo "Ralph[lobby-connect] sentinel emitted at iteration $i."
    exit 0
  fi

  echo "Iteration $i complete. Continuing in 2 seconds..."
  sleep 2
done
echo "Ralph[lobby-connect] reached MAX_ITERATIONS ($MAX_ITERATIONS)."
exit 1
