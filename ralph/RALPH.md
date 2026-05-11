# Ralph Agent Instructions — Lobby chrome via Clay primitives

You are an autonomous coding agent on the **second milestone** of the
Silencer lobby Clay refactor.

The first milestone (byte-identical migration of the lobby to Clay) is
DONE. The lobby is fully on Clay; legacy `Object`-widget lobby code
is gone. Now you replace the **structural rectangles baked into the
LobbyBG sprite** with Clay-drawn rectangles in a flex layout, and
relocate the existing panel subtrees inside those rectangles.

You are running in the `hv/clay` worktree. Commit per iteration.

## Required reading every iteration

1. `docs/plans/2026-05-11-lobby-clay-refactor.md` — especially the
   **"Next milestone — chrome via Clay primitives"** section. Re-read
   it every iteration; it is the spec.
2. `ralph/progress.txt` — start with `## Codebase Patterns` at the
   top. The first-milestone agent left load-bearing patterns there
   (bridge ABI, sprite-offset compensation, deterministic step
   verification, etc.). DO NOT re-derive them.
3. `ralph/prd.json` — the backlog. Source of truth for what's done.
4. `.claude/skills/clay-ui/` — canonical Clay idioms.
5. `/tmp/lobby_bg.png` — the dumped lobby BG sprite. Open it visually.
   Every "rectangle" in this milestone refers to a bright-green
   stroked rectangle visible in that sprite. If `/tmp/lobby_bg.png`
   is missing, re-run `python3 /tmp/dump_lobby_bg.py` to regenerate it
   (the script is committed at... actually it's not committed; if
   missing, copy from this RALPH.md's "Regenerating the BG dump"
   appendix at the end).

## What changes vs. the first milestone

**Visual gate is gone.** The first milestone gated each panel on
<5% pixdiff against the legacy lobby. That's no longer applicable —
this milestone INTENTIONALLY changes the visual:

- The legacy LobbyBG's decorative texture (circuit boards, planet
  monitor, photo collages) goes away. Panel interiors become flat
  color, optionally with opacity.
- The legacy LobbyBG's structural rectangles (bright-green stroked
  panel borders) get replaced by Clay-emitted rectangles via a new
  `Rectangle` primitive.
- The lobby's panels currently position via `floating @ROOT (x, y)`.
  Move them INSIDE flex containers — `LEFT_TO_RIGHT` / `TOP_TO_BOTTOM`,
  gaps, padding, alignment. Position pixel-equivalence is not
  required; structural equivalence (the right things in the right
  rectangles) is.

**Verification is structural + functional**, not pixel-identical:

- Build green (`cmake --build build -j`).
- `tests/cli-agent/e2e/` regression suite still green — *especially*
  `40_lobby_clay_basic.sh` which drives the full lobby through Clay.
- Clay inspector shows the expected element IDs at expected positions
  (each prd item names what must exist).
- A screenshot of the new lobby gets DM'd to the user each iteration
  for visual review. The user is the final judge of visual fidelity.

## Your task each iteration

1. Read `prd.json`. Pick the **lowest-`priority` number** item with
   `passes: false`.
2. Read `progress.txt` (patterns + latest 1–2 entries).
3. Implement the ONE item. Stay in scope.
4. **Run quality gates:**
   - `cmake --build build -j`
   - If applicable, the item's named `pass_check` command
   - `tests/cli-agent/e2e/40_lobby_clay_basic.sh` (lobby Clay e2e)
5. **Take a screenshot** of the lobby in a canonical state (post
   main-menu → lobby transition, no game selected). See "Taking a
   screenshot" below. Save to `/tmp/ralph-shots/iter-<ID>.png`.
6. **Atomic commit + flag flip** — implementation + `prd.json` flip
   from `passes: false` → `passes: true` for this item land in the
   SAME commit. Stage everything, run jq to flip the flag, stage the
   updated `prd.json`, commit. Order:

   ```bash
   git add <implementation paths>
   jq --arg id "<ID>" '(.items[] | select(.id == $id)).passes = true' \
       ralph/prd.json > ralph/prd.json.tmp && mv ralph/prd.json.tmp ralph/prd.json
   git add ralph/prd.json ralph/progress.txt
   git commit -m "chrome(<id>): <description>" -m "" -m "Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
   ```

   Invariant: after the commit, `git diff HEAD~1 HEAD -- ralph/prd.json`
   must show this iteration's flag flipped from false→true.
7. **APPEND** your iteration entry to `progress.txt` (NEVER overwrite).
   Same format as the first milestone:
   ```
   ## [ISO timestamp] - [Item ID]
   - What was implemented
   - Files changed
   - **Learnings for future iterations:**
     - Patterns / gotchas
   ---
   ```
   If you discovered a reusable pattern, also add a one-liner to the
   `## Codebase Patterns` section at the TOP of `progress.txt`.
8. **Send a Discord DM** with the screenshot. Always send, always
   attach the screenshot.

   ```bash
   bun /Users/hv/.claude/skills/discord-dm/send.ts \
     "Chrome Ralph iter N · <ID> <name> · pass=<true|false>" \
     /tmp/ralph-shots/iter-<ID>.png 2>/dev/null || \
   bun /Users/hv/.claude/skills/discord-dm/send.ts \
     "Chrome Ralph iter N · <ID> <name> · pass=<true|false> · (attach failed)"
   ```
9. Check stop condition.

## Stop condition

```bash
jq -e '.items | all(.passes == true)' ralph/prd.json
```

- If yes → reply with **exactly** this literal as the last
  non-empty line of your output: `<chrome>COMPLETE</chrome>`
- If no → end normally; the next iteration picks up.

> Do NOT write `<chrome>COMPLETE</chrome>` anywhere except when
> you are actually emitting the stop signal. Paraphrase as "the
> COMPLETE signal" if you need to discuss it.

## Taking a screenshot

```bash
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
# Navigate into the lobby (existing main menu flow — see the e2e
# scripts for the exact click sequence, typically: login → join
# default server → wait_for_state LOBBY)
cli --port "$PORT" screenshot --out /tmp/ralph-shots/iter-<ID>.png
```

For pre-lobby items (Rectangle primitive smoke test, bridge alpha
support test) screenshot the relevant test scene PNG instead — there
will always be a visual artifact to attach.

## Critical design rules (from the design doc)

1. **No new lobby vocabulary in the `Rectangle` primitive.** It takes
   color, opacity, stroke width — that's it. Variant enums OK.
2. **Rectangle primitive is screen-agnostic.** No references to
   `world.h`, `lobby.h`, `config.h`. Lives in
   `clients/silencer/src/ui/clay/primitives/rectangle.{h,cpp}`.
3. **Flex layout, not `floating @ROOT`** for the lobby panels. Move
   each `BuildXxxPanelTree` function to emit a subtree that
   self-positions inside its parent flex container via padding /
   gap / alignment. Pre-baked screen-pixel coords should disappear.
4. **The chat-area upside-down-L** (chat region in the BG extends
   under the character panel area to form an L) — default approach:
   compose two `Rectangle`s side-by-side that visually join. If that
   loses fidelity, leave the item at `passes: false`, document the
   obstacle in `progress.txt`, and ask for human input.
5. **Defer the sphere lights** (film-strip dots top + bottom of the
   BG). Not in scope.
6. **Drop the decorative texture** (circuit boards, planet monitor,
   photo collage from the legacy BG). Panel interiors are flat /
   semi-transparent fills, not textured.

## Important

- ONE prd item per iteration.
- Quality gates green before flipping the flag. If e2e breaks, fix
  in the same iteration or leave at `passes: false`.
- Read `progress.txt`'s `## Codebase Patterns` first, every iteration.
- The visual review is the user's. Don't fudge your own self-grade
  with "looks fine to me" — describe what you implemented and let
  the screenshot speak for itself.
- If you cannot make progress on an item, leave `passes: false`,
  write the obstacle into `progress.txt`, and end the iteration.
- Do NOT edit `RALPH.md`, `prd.json` schema (only the `passes`
  field flips), or `docs/plans/2026-05-11-lobby-clay-refactor.md`.

## Regenerating the BG dump (appendix)

If `/tmp/lobby_bg.png` is missing, write `/tmp/dump_lobby_bg.py`
with the contents below and run `python3 /tmp/dump_lobby_bg.py`.
The script decodes `shared/assets/BIN_SPR.DAT` + `bin_spr/SPR_007.BIN`
+ `PALETTE.BIN` palette block 2 to produce a 640×480 RGB PNG of
sprite bank 7, idx 1 (the lobby BG art with all rectangle strokes
intact). Format details documented in the script's docstring.
The script lives in conversation history; if absent, the agent that
ran it earlier in this session can regenerate it from
`clients/silencer/src/resources/resources.cpp` LoadSprites() and
`clients/silencer/src/render/palette.cpp`.
