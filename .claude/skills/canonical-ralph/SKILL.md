---
name: canonical-ralph
description: Use when setting up an autonomous Claude Code loop to grind through a structured backlog (PRD-style) until every item passes — a "Ralph" loop. Triggers include "set up a ralph loop", "run claude in a loop on this list", "autonomous iteration over user stories", or any time you want a bash-driven for-loop calling `claude --print` against a stable instruction file with append-only memory and a structured pass/fail backlog.
---

# Canonical Ralph

A "Ralph" loop is an autonomous Claude Code loop with four artifacts and one rule:

| Artifact | Role | Mutates? |
| --- | --- | --- |
| `ralph.sh` | Driver — bash for-loop that re-invokes Claude each iteration | Never |
| `RALPH.md` | Instruction prompt fed to Claude on stdin every iteration | Never (during the run) |
| `prd.json` | Structured backlog of items with `passes: true\|false` | Yes — agent flips one flag per iteration |
| `progress.txt` | Append-only memory across iterations + "Codebase Patterns" header | Yes — agent appends per iteration |

**The rule:** one item per iteration, one commit per iteration, append (never replace) progress, agent emits a sentinel when fully done.

This is "canonical" because the same shape ships in `llm-gateway/docs/ralph-perf-run/` and `terraform-town/`. Both work. Don't reinvent the shape.

## When to use

- You have a list of N independent-ish tasks (user stories, perf properties, screens to validate, schemas to migrate) and want Claude to grind through them unattended.
- Each task has an objective pass condition you can encode as `passes: true/false`.
- You can run `claude --dangerously-skip-permissions --print` (you trust the workspace, this is a worktree, etc.).

## When NOT to use

- The task list isn't enumerable up front (use a normal interactive session).
- Each task needs human judgment to know when it's done (no objective pass condition → no stop signal).
- You want plan-then-execute (use `superpowers:writing-plans` + `superpowers:executing-plans`).

## The four artifacts

### `ralph.sh` — the driver

```bash
#!/bin/bash
# Ralph — autonomous Claude Code loop
# Runs until every item in prd.json has passes: true (or MAX_ITERATIONS hit)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RALPH_MD="$SCRIPT_DIR/RALPH.md"
MAX_ITERATIONS=20
LOG_FILE="$SCRIPT_DIR/ralph.log"

echo "Starting Ralph loop..."
echo "Worktree: $SCRIPT_DIR"
echo "Max iterations: $MAX_ITERATIONS"
echo ""

for i in $(seq 1 $MAX_ITERATIONS); do
  echo "==============================================================="
  echo "  Ralph — Iteration $i of $MAX_ITERATIONS  $(date '+%F %T')"
  echo "==============================================================="

  # Show current backlog status (handy for tailing)
  if command -v jq &>/dev/null; then
    jq -r '.items[] | "  \(.id) [\(if .passes then "PASS" else "TODO" end)] \(.name)"' \
       "$SCRIPT_DIR/prd.json"
  fi
  echo ""

  # Re-invoke Claude with the SAME instructions every time.
  # tee /dev/stderr so the operator sees live output AND we can grep OUTPUT.
  OUTPUT=$(cd "$SCRIPT_DIR" && claude --dangerously-skip-permissions --print < "$RALPH_MD" 2>&1 | tee /dev/stderr) || true

  # Append a tail of the iteration to a persistent log
  {
    echo "--- Iteration $i $(date '+%F %T') ---"
    echo "$OUTPUT" | tail -20
    echo ""
  } >> "$LOG_FILE"

  # Stop signal: agent must emit this exact tag when all items pass.
  if echo "$OUTPUT" | grep -q "<promise>COMPLETE</promise>"; then
    echo "Ralph completed all items at iteration $i."
    exit 0
  fi

  echo "Iteration $i complete. Continuing in 2 seconds..."
  sleep 2
done

echo "Ralph reached max iterations ($MAX_ITERATIONS). Inspect prd.json + progress.txt."
exit 1
```

Five non-negotiable details:

1. **`--dangerously-skip-permissions --print`** — `--print` is what makes the invocation non-interactive (one-shot, exit-on-EOF). `--dangerously-skip-permissions` is what makes the iteration unattended. Run inside a worktree.
2. **`< RALPH.md`** — the prompt is piped on stdin, not passed as an arg. This keeps the prompt arbitrarily large and version-controlled separately from the driver.
3. **`tee /dev/stderr`** — the operator wants live output (visibility) AND the script needs the captured text (for the sentinel grep). Don't choose between them.
4. **`|| true`** after the `claude` invocation — non-zero exit must NOT kill the loop. The agent is allowed to fail an iteration; the next iteration retries.
5. **`MAX_ITERATIONS`** — bounded, not `while true`. A bug in the agent's stop logic must not bill forever.

### `RALPH.md` — the instruction prompt

Stable for the entire run. Re-invoked verbatim every iteration. Skeleton:

```markdown
# Ralph Agent Instructions

You are an autonomous coding agent doing <X>.

## Your task each iteration

1. Read the backlog at `prd.json`.
2. Read `progress.txt`. **Read the `## Codebase Patterns` section at the top first** — prior iterations have already learned things, don't re-derive them.
3. Pick the **single highest-priority** item where `passes: false`.
4. Implement that ONE item. Do not opportunistically fix other items.
5. Run quality gates: <project-specific test + format commands>.
6. If gates pass, commit ALL changes with: `<conventional message format>`.
7. Set `passes: true` for that item in `prd.json`.
8. APPEND your iteration entry to `progress.txt` (never overwrite).

## Progress entry format

Append to `progress.txt`:
\`\`\`
## [ISO timestamp] - [Item ID]
- What was implemented
- Files changed
- **Learnings for future iterations:**
  - Patterns / gotchas
---
\`\`\`

## Codebase Patterns header

If you discover a reusable pattern, also add a one-liner to the
`## Codebase Patterns` section at the TOP of `progress.txt`.

## Stop condition

After flipping the flag, check: do ALL items in `prd.json` have `passes: true`?

- If yes → reply with exactly: `<promise>COMPLETE</promise>`
- If no  → end your response normally (the next iteration picks up).

## Important

- ONE item per iteration. Resist scope creep.
- Commit per iteration.
- Quality gates must be green before flipping the flag.
- Read the patterns section first, every iteration.
```

### `prd.json` — the backlog

A flat array of items. Each item has at minimum `id`, `name`, `priority`, `passes`, plus whatever spec fields the work needs. Example shape:

```json
{
  "items": [
    {
      "id": "P0",
      "name": "Set up benchmark harness",
      "priority": 0,
      "passes": false,
      "spec": "<freeform — referenced from RALPH.md>"
    },
    {
      "id": "P1",
      "name": "Sublinear degradation",
      "priority": 1,
      "passes": false,
      "spec": "..."
    }
  ]
}
```

The driver greps the JSON with `jq` for visibility; the agent reads it as the source of truth. The agent flips `passes: true`. **No other authority decides whether an item is done** — if the human disagrees, the human edits `prd.json` and adds an item.

### `progress.txt` — append-only memory

Two regions:

```
## Codebase Patterns
- <one-liner>: <one-liner>
- <one-liner>: <one-liner>

## 2026-04-26T18:21:03Z - P1
- Implemented incremental projection
- Files: packages/.../repl.ts, .../reducer.ts
- **Learnings:**
  - Effect's Either is overkill for the inner hot loop; use a Result tagged union
---
## 2026-04-26T18:24:55Z - P2
...
```

The patterns section at the top is the cross-iteration brain. The append-only timeline is the audit trail. Combined, they let iteration N+1 pick up where iteration N left off without re-reading the entire codebase.

## The stop signal

The driver greps the agent's output for the literal string `<promise>COMPLETE</promise>`. That's the ONLY way the loop exits early.

This works because:

- The agent decides locally whether all flags are true. Cheap check.
- The driver's grep is dumb and reliable — no JSON parsing, no log scraping.
- The angle-bracket sentinel never appears in normal commit messages, code, or chatter, so false positives don't happen.

If you need a different sentinel for a specific Ralph (e.g., parallel Ralphs), pick a different unique string, but keep the shape: `<X>VERB</X>` where `X` is unique.

## Run discipline

- **Run in a git worktree.** Ralph commits per iteration directly to the branch. Don't run on a branch where someone else is working.
- **Iteration timing.** Each iteration is one Claude session. Sessions can be 30s or 30min. `MAX_ITERATIONS=20` is a tight ceiling; `200` is normal for big backlogs.
- **Sleep between iterations.** 2s is enough — it's there so a human can `Ctrl+C` between iterations cleanly.
- **Leave the terminal attached** for the first few iterations, even with `--dangerously-skip-permissions`, so you catch a wedged loop early. Detach once you've seen 2-3 successful iterations.
- **Read `progress.txt` between checks**, not the diff. The diff for a 20-iteration run is hundreds of files; `progress.txt` is the executive summary the agent wrote for itself.

## Common mistakes

| Mistake | Fix |
| --- | --- |
| Running `claude` without `--print` | The session never exits; the for-loop hangs on iteration 1 forever. Use `--print`. |
| Replacing `progress.txt` instead of appending | Memory across iterations dies. The agent re-derives the same patterns every time, slowly. The RALPH.md must say "APPEND, never replace." |
| Multiple items per iteration | One commit per logical unit dies; bisect dies; the diff at iteration N is unbounded. Hard rule: one item, one commit. |
| No sentinel — loop only stops on MAX_ITERATIONS | You burn a budget you didn't need to. Add `<promise>COMPLETE</promise>`. |
| Sentinel that can appear in code (e.g., `DONE`) | False-positive stops. Use angle-bracket tags. |
| Writing `RALPH.md` each iteration to "improve" it | The instruction file is stable. If you must edit, stop the loop, edit, restart. |
| No `\|\| true` after `claude` | One bad iteration kills the run. Iteration must be allowed to fail. |
| `MAX_ITERATIONS` unset / `while true` | Runaway loops on a credit card. Always bounded. |
| `prd.json` items with no objective pass condition | Agent flips flags arbitrarily. Each item must be checkable. |
| Running outside a worktree | Commits land on the branch a human is using. Always a worktree. |

## Variants

- **Multi-Ralph on one repo.** `RALPH-editor.md` + `prd-editor.json` + `ralph-editor.sh`, alongside `RALPH-visualization.md` + `prd-visualization.json` + `ralph-visualization.sh`. Same shape per Ralph. Different sentinel per Ralph if running concurrently.
- **Visual-regression Ralph.** The `passes` check is "the dump matches the reference". The quality gate is the in-process PPM dump from `superpowers:visual-regression-testing`, not a unit test. The agent dumps, compares, decides.
- **Schema-migration Ralph.** Items are migrations; `passes` is "applies cleanly forward, then reverses cleanly backward".

## Real-world references in this repo's neighborhood

- `~/repos/llm-gateway/docs/ralph-perf-run/ralph-perf.sh` + `RALPH-PERF.md` — the perf flavor. Look at this first.
- `~/repos/terraform-town/scripts/ralph.sh` + `RALPH.md` + `prd.json` + `progress.txt` — the simplest possible canonical Ralph. Look at this if the perf one feels overloaded.

Both ship the same four-artifact shape. That's not a coincidence; that's the canon.
