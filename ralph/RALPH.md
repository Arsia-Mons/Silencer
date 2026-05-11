# Ralph Agent Instructions — Silencer lobby → Clay refactor

You are an autonomous coding agent executing the Silencer **lobby → Clay**
refactor described in `docs/plans/2026-05-11-lobby-clay-refactor.md`.
Read it first, every iteration. It is short and load-bearing.

You are running in a git worktree at the repo root. Commit per iteration.

## Required reading every iteration

1. `docs/plans/2026-05-11-lobby-clay-refactor.md` — the design + the
   five "primitive design rules". Re-anchor on the rules every time.
2. `ralph/progress.txt` — start with `## Codebase Patterns` at the
   top. Prior iterations have already learned things — do not
   re-derive them.
3. `ralph/prd.json` — the backlog. Source of truth for what is done.
4. `.claude/skills/clay-ui/` (the canonical Clay idioms skill) — if
   you are touching Clay code. Loaded via `/Users/hv/.claude/skills/clay-ui`.
5. `.claude/skills/using-silencer-cli` — if you need to screenshot or
   drive the running game.

## Your task each iteration

1. Read `prd.json`. Pick the **single lowest-`priority` number** item
   where `passes: false`. Do not skip ahead — earlier items unblock
   later ones.
2. Read `progress.txt` (patterns + latest 1-2 entries).
3. Implement that ONE item. Stay strictly in scope. Resist
   opportunistic refactors.
4. Run quality gates:
   - `cmake --build build -j` (configure first if needed:
     `cmake -B build -S clients/silencer`).
   - If the item touches Clay primitives or screens, run the item's
     own pass check (see `pass_check` field in the prd item).
5. Capture **any** visual artifact this iteration produces. Save it
   to `/tmp/ralph-shots/iter-<id>.png` (or `/tmp/ralph-shots/iter-<id>-N.png`
   if there are several). Sources, in priority order:
   - If the item has a `screenshot` field: drive the CLI harness
     (see "Taking a screenshot" below) and capture from the live
     game.
   - Otherwise, if the iteration produces a render-test, smoke-test,
     or baseline-capture PNG (P1's fixtures, P2's baselines, P3's
     bridge_smoke output, P4–P10's primitive render tests): use the
     freshly-produced PNG.
   - Otherwise (rare — only items that produce nothing visual,
     e.g., P0 vendor-only steps), skip and the DM goes out
     text-only.
6. If the item has a `pixdiff_baseline` field, compute byte diff
   against the named baseline using `tools/pixdiff/pixdiff`. The
   diff must be **below the `pixdiff_max_pct` threshold** to pass.
7. **Atomic commit + flag flip** — the commit for this iteration
   MUST contain BOTH the implementation changes AND the
   `prd.json` flip from `passes: false` → `passes: true` for this
   item. Do not flip the flag in a separate commit. Do not flip
   the flag without committing. Order of operations:

   a. Stage implementation changes (`git add <paths>`).
   b. Flip the flag in `prd.json`:
      ```bash
      jq --arg id "<ITEM_ID>" '(.items[] | select(.id == $id)).passes = true' \
          ralph/prd.json > ralph/prd.json.tmp && mv ralph/prd.json.tmp ralph/prd.json
      ```
   c. Stage `ralph/prd.json` and `ralph/progress.txt` (step 8).
   d. Commit with message `clay(<id>): <description>` + the
      project's `Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>`
      trailer.

   **Invariant:** after the commit, `git diff HEAD~1 HEAD -- ralph/prd.json`
   must show this iteration's flag flipped from false→true. If the
   commit failed (pre-commit hook, etc.), DO NOT flip the flag.
8. APPEND your iteration entry to `progress.txt` (NEVER overwrite).
   Format:
   ```
   ## [ISO timestamp] - [Item ID]
   - What was implemented
   - Files changed (paths)
   - **Learnings for future iterations:**
     - Patterns / gotchas
   ---
   ```
   If you discovered a reusable pattern, also add a one-liner to the
   `## Codebase Patterns` section at the TOP of `progress.txt`.
   Stage this file as part of the same atomic commit (step 7c).
9. **Send a Discord DM** summarizing this iteration. Always send,
   and **always attach every PNG produced in step 5** (use multiple
   trailing args — each is one attachment).

    ```bash
    SHOTS=$(ls /tmp/ralph-shots/iter-<id>*.png 2>/dev/null)
    bun /Users/hv/.claude/skills/discord-dm/send.ts \
      "Ralph iter N · <ID> <name> · pass=<true|false> · diff=<X%> · next=<next ID>" \
      $SHOTS 2>/dev/null || \
    bun /Users/hv/.claude/skills/discord-dm/send.ts \
      "Ralph iter N · <ID> <name> · pass=<true|false> · (attach failed)"
    ```
    DM is best-effort. If it fails, log it and proceed — do not block
    the loop on the DM.
10. Check stop condition (see below).

## Stop condition

After flipping the flag, check whether ALL items in `prd.json` have
`passes: true`:

```bash
jq -e '.items | all(.passes == true)' ralph/prd.json
```

- If yes → reply with **exactly** this literal as the last
  non-empty line of your output: `<promise>COMPLETE</promise>`
- If no → end your response normally. The next iteration picks up.

> Do not write the literal string `<promise>COMPLETE</promise>`
> anywhere in your iteration output unless you are actually emitting
> the stop signal because every item passes. If you need to talk
> about the stop condition, use a paraphrase like "the COMPLETE
> signal".

## Taking a screenshot

The CLI harness in `tests/cli-agent/e2e/lib.sh` boots a headless
binary and exposes a `cli` shell function. Typical flow for a
lobby-screen screenshot:

```bash
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
# Navigate to the lobby surface relevant to your item (see prd item's
# `screenshot_recipe` field if present)
cli --port "$PORT" screenshot --out /tmp/ralph-shots/iter-<id>.png
```

For pre-lobby items (vendor clay, pixdiff tool, renderer bridge
smoke test) there is no lobby state to screenshot. Skip the
screenshot step; the DM goes out text-only.

## Critical primitive design rules

(Repeated from the design doc — these are easy to violate under
schedule pressure.)

1. **No lobby vocabulary in any primitive.** A primitive that
   mentions "agency", "map list", "tech slot" is wrong. Talk in
   layout + content + behavior (`label`, `variant`, `onClick`).
2. **State passed in, mutations via callback.** Primitives do not
   own state.
3. **No `world`, `lobby`, `MapDownloader`, `Config` references**
   from primitives. Domain glue is in the *screen*, not the
   primitive.
4. **Variants are explicit enums, not strings.**
5. **Composition over options.** Three near-identical primitives
   beat one with 17 bool flags.

If you catch yourself adding a flag to a primitive to make it work
for a specific lobby case, stop. The lobby's job is to *compose*
primitives, not to reshape them.

## Important

- ONE prd item per iteration. Resist scope creep.
- One commit per iteration.
- Quality gates green before flipping the flag. If a build breaks,
  fix it in the same iteration — do not flip the flag with a broken
  build.
- Read the `## Codebase Patterns` header in `progress.txt` first,
  every iteration.
- Pixel-diff thresholds are HARD caps — do not flip the flag if a
  panel comes back at 6% diff. Either fix the rendering or document
  why the panel cannot reach <5% and ask for human input by leaving
  the item at `passes: false` and writing the question into
  `progress.txt`.
- If you cannot make progress on an item after one honest attempt
  (build broken, blocked on missing infra, ambiguous spec), leave
  `passes: false`, write the obstacle into `progress.txt`, and end
  the iteration. The next iteration retries with that context.
- Do not edit `RALPH.md`, `prd.json` schema (only `passes`),
  baselines under `tests/lobby-clay/baselines/`, or
  `docs/plans/2026-05-11-lobby-clay-refactor.md`. These are
  stable for the run.
