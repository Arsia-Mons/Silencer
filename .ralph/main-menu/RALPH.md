# Ralph Agent Instructions — main-menu spec-only rebuild

You are an autonomous coding agent iterating on the SDL3 hydration of
the Silencer main-menu screen. Your goal: make the candidate's PPM
dump visually equivalent to the canonical reference dump from the real
client, **using only the design spec and binary assets — never the
real client's source.**

## Spec-only rebuild — non-negotiable

You MUST work from these inputs only:

- `docs/design/` — the design spec (README, palette, sprite-banks,
  font, tick, widget-overlay, widget-button, widget-interface,
  screen-main-menu).
- `shared/assets/` — binary game assets (PALETTE.BIN, BIN_SPR.DAT,
  BIN_TIL.DAT, sprite banks, fonts).
- `shared/design/sdl3/` — your candidate; this is what you edit.
- `clients/silencer/CLAUDE.md` — read once for top-level orientation
  ONLY. Do **not** open any source under `clients/silencer/src/`.

Do **NOT** open `clients/silencer/src/`, ever. Reading it would defeat
the falsifiability goal — the spec is being validated by your ability
to rebuild from it. If a fact you need isn't in `docs/design/`, it is
a **spec gap**, and you MUST: (a) document the gap in
`progress.txt`, (b) make your best inference, (c) flag it in your
progress entry. Do not silently fill the gap from the engine source.

This rule has no exceptions.

## Reference dump

The canonical reference is at `/tmp/real_dump.ppm` (and
`/tmp/real_dump.png`). 640×480 P6 PPM, dumped from the real client at
the moment the bank-208 logo overlay's `res_index == 60`.

Do not regenerate it. Do not modify it.

## The candidate

Build from `shared/design/sdl3/`:

```
cd shared/design/sdl3
cmake -B build
cmake --build build
SILENCER_DUMP_DIR=/tmp/sdl3_dump \
  ./build/silencer_design <worktree-root>/shared/assets
```

Output: `/tmp/sdl3_dump/screen_00.ppm`.

If there's no main_menu screen yet, no widget composition, etc. — that
is exactly what you're building, per `docs/design/`.

## Your task each iteration

1. Read `prd.json` (next to this file).
2. Read `progress.txt`. **Read `## Codebase Patterns` at the top first**
   — prior iterations have already discovered things; don't re-derive.
3. Pick the **single highest-priority** item where `passes: false`.
4. Implement that ONE item. Stay scoped — one diff category per the
   visual-regression-testing skill (composition / palette / sprite
   codec / tile arithmetic / anchor / animation timing).
5. Build the candidate cleanly: `cmake --build shared/design/sdl3/build`.
6. Run dump mode (command above). Confirm the PPM exists.
7. Convert both PPMs to PNG for Discord:

   ```
   sips -s format png /tmp/sdl3_dump/screen_00.ppm \
     --out /tmp/sdl3_dump/screen_00.png
   sips -s format png /tmp/real_dump.ppm \
     --out /tmp/real_dump.png  # only if missing
   ```

8. **Visually A/B** the two PNGs (yes — actually open and compare them;
   you have multimodal vision). Categorize any divergence per the
   visual-regression-testing skill's diff table. Decide whether the
   item you targeted now `passes: true`. Be honest — false positives
   are worse than slow progress.
9. **Send a Discord DM with both PNGs and a short status line:**

   ```
   bun /Users/hv/.claude/skills/discord-dm/send.ts \
     "Ralph iter <N>: <item-id> <PASS|TODO> — <one-line summary>" \
     /tmp/sdl3_dump/screen_00.png /tmp/real_dump.png
   ```

   Send this **every iteration**, regardless of pass/fail.
10. If the item passes, set `passes: true` in `prd.json` for that
    item (and only that item). Commit ALL changes:

    ```
    git add -A && git commit -m "feat(design): [<item-id>] <name>"
    ```

11. APPEND your iteration entry to `progress.txt` (never replace).

## Progress entry format

```
## <ISO-8601 timestamp> — <item-id>
- What was implemented / changed
- Files touched
- Visual diff summary: <category from skill table>
- Spec gaps encountered: <list, or "none">
- **Learnings:**
  - <pattern / gotcha>
---
```

If you discover a reusable pattern, also one-line it at the top under
`## Codebase Patterns`.

## Stop condition

After flipping a flag, check: do ALL items in `prd.json` have
`passes: true`?

- Yes → reply with EXACTLY: `<promise>COMPLETE</promise>`
- No  → end your response normally; the next iteration picks up.

## Discipline reminders

- **One item per iteration.** Resist scope creep; opportunistic side
  fixes break the audit trail and bisect.
- **One commit per iteration.** Per item, with the `[<item-id>]` tag.
- **APPEND `progress.txt`.** Never overwrite.
- **Eyeball, don't pixel-equal.** Two PPMs will differ in subpixel
  ways; the skill's iteration loop stops at "looks the same",
  pixel-equal is a final gate not an iteration gate.
- **Spec gaps are first-class output.** If `docs/design/` is missing
  something, document it. The Ralph's value is partly that gap list.
- **Read patterns first**, every iteration.
- **Self-judging is OK** (the user authorized it). Be honest.
