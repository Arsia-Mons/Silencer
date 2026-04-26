# Ralph Agent Instructions — OPTIONS hub spec-only rebuild

You are an autonomous coding agent extending the SDL3 hydration of the
Silencer UI to render the **Options menu hub** (state `OPTIONS`),
matching `/tmp/real_options_dump.ppm`. Spec-only rebuild discipline
applies — the engine source under `clients/silencer/src/` is **off
limits**.

## Inputs

- `docs/design/screen-options.md` — the spec for this screen.
- `docs/design/{palette,sprite-banks,font,tick,widget-overlay,widget-button,widget-interface}.md`
  — the same lower-layer specs you used for main menu.
- `shared/assets/` — binary game assets.
- `shared/design/sdl3/` — your candidate. The main-menu hydration is
  already complete and visually equivalent to its reference; reuse
  every piece of the pipeline (palette decoder, sprite codec, font
  blit, widget Button/Overlay/Interface, harness tick loop). Only the
  **screen composition** is new.
- Reference dump: `/tmp/real_options_dump.ppm` (and `.png`).

`clients/silencer/src/` is **off limits**. If `docs/design/` is
missing a fact you need, **document it as a spec gap** in
`progress.txt` and make your best inference; do not silently fill the
gap from engine source.

## Candidate harness extension (your call to design)

The candidate currently renders `main_menu` and dumps
`screen_00.ppm`. Pick a clean way to switch screens — env var,
CLI arg, separate binary, whatever you justify. Suggested:

```
SILENCER_DUMP_SCREEN=options \
  SILENCER_DUMP_DIR=/tmp/sdl3_dump \
  ./build/silencer_design <worktree>/shared/assets
```

→ writes `/tmp/sdl3_dump/screen_00.ppm` showing the OPTIONS hub.

Keep main-menu pathway working — don't regress it.

## Each iteration

1. Read `prd.json` (next to this file).
2. Read `progress.txt` — `## Codebase Patterns` first.
3. Pick the **single highest-priority** item where `passes: false`.
4. Implement that ONE item.
5. Build: `cmake --build shared/design/sdl3/build`.
6. Dump (your screen-switch mechanism): produces a PPM at
   `/tmp/sdl3_dump/screen_00.ppm`.
7. PPM→PNG:
   `sips -s format png /tmp/sdl3_dump/screen_00.ppm --out /tmp/sdl3_dump/screen_00.png`
8. Visually A/B candidate (`/tmp/sdl3_dump/screen_00.png`) vs
   reference (`/tmp/real_options_dump.png`). Categorize divergences
   per the visual-regression-testing skill's diff table.
9. Decide whether the targeted item now `passes: true`. **Honest
   self-judgment** — the user authorized this, but false positives
   hurt the audit trail.
10. Discord DM both PNGs + a one-line status:

    ```
    bun /Users/hv/.claude/skills/discord-dm/send.ts \
      "Ralph[options] iter <N>: <id> <PASS|TODO> — <summary>" \
      /tmp/sdl3_dump/screen_00.png /tmp/real_options_dump.png
    ```

11. If passes, set `passes: true` in `prd.json`, commit ALL changes:
    `git commit -m "feat(design): [<id>] <name>"`.
12. APPEND your iteration entry to `progress.txt` (never replace).

## Progress entry format

```
## <ISO timestamp> — <item-id>
- What was implemented
- Files touched
- Visual diff summary: <category>
- Spec gaps: <list, or "none">
- **Learnings:**
  - <pattern>
---
```

Reusable patterns also one-line at the top under `## Codebase Patterns`.

## Stop condition

After flipping a flag, check: do ALL items in `prd.json` have
`passes: true`?

- Yes → reply with EXACTLY: `<promise>COMPLETE</promise>`
- No  → end your response normally.

## Discipline

- ONE item per iteration. ONE commit per iteration.
- APPEND `progress.txt`, never overwrite.
- Eyeball, don't pixel-equal. <1% diff ascribed to aliasing /
  blend-noise / animation-phase is acceptable.
- Spec gaps are first-class output.
- Read patterns first, every iteration.
- Don't regress the main-menu pathway — keep
  `SILENCER_DUMP_DIR=/tmp/sdl3_dump ./build/silencer_design …` (no
  `SILENCER_DUMP_SCREEN`) producing the main-menu PPM.
