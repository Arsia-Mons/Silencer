# Ralph Agent Instructions — OPTIONSAUDIO spec-only rebuild

You are an autonomous coding agent extending the SDL3 hydration to
render the **Audio Options** screen (state `OPTIONSAUDIO`), matching
`/tmp/real_optionsaudio_dump.ppm`. Spec-only rebuild discipline applies
— `clients/silencer/src/` is **off limits**.

## Inputs

- `docs/design/screen-options-audio.md` — the spec for this screen.
- `docs/design/{palette,sprite-banks,font,tick,widget-overlay,widget-button,widget-interface}.md`
  — same lower-layer specs you've used.
- `shared/assets/` — binary game assets.
- `shared/design/sdl3/` — your candidate. Main-menu and OPTIONS hub
  hydrations are already complete (see HEAD git log: tags `[F0]`,
  `[O4]`). Reuse every piece — palette / sprite codec / font / widgets
  / harness `RunDumpFoo` pattern. Only the screen composition + the
  new widgets (title overlay, B220x33 button, toggle pill indicators)
  are new.
- Reference dump: `/tmp/real_optionsaudio_dump.ppm` and `.png`.
- **Pattern memory:** read `.ralph/main-menu/progress.txt` and
  `.ralph/options/progress.txt` (Codebase Patterns sections at top)
  before starting — the prior Ralphs already learned the
  free-flip-cascade pattern, the screen-switch env-var pattern
  (`SILENCER_DUMP_SCREEN`), and several sprite/font invariants.

`clients/silencer/src/` is **off limits**. Spec gaps go in
`progress.txt`, not silently filled.

## Candidate harness

The previous Ralph added `SILENCER_DUMP_SCREEN=options` to switch
screens. Extend that mechanism for `audio` (or pick another clean
pattern — your call). Suggested:

```
SILENCER_DUMP_SCREEN=options_audio \
  SILENCER_DUMP_DIR=/tmp/sdl3_dump \
  ./build/silencer_design <worktree>/shared/assets
```

Don't regress `main_menu` or `options`. Add a regression check to
your iteration: dump `main_menu` and `options` once with their env
vars and confirm sizes/diffs unchanged.

## Each iteration

Same structure as the OPTIONS Ralph. Brief:

1. Read `prd.json` + `progress.txt` (patterns first).
2. Pick the highest-priority `passes:false` item.
3. Implement ONE item.
4. Build, dump audio, PPM→PNG.
5. Visually A/B vs `/tmp/real_optionsaudio_dump.png`.
6. Decide pass/fail honestly.
7. Discord DM both PNGs:

   ```
   bun /Users/hv/.claude/skills/discord-dm/send.ts \
     "Ralph[audio] iter <N>: <id> <PASS|TODO> — <summary>" \
     /tmp/sdl3_dump/screen_00.png /tmp/real_optionsaudio_dump.png
   ```

8. If pass: flip `prd.json`, commit `feat(design): [<id>] <name>`,
   APPEND `progress.txt`.

## Stop condition

After flipping a flag, if all items pass: emit
`<promise>COMPLETE</promise>`. Otherwise end normally.

## Discipline

- ONE item per iteration. ONE commit per iteration. APPEND progress.
- Eyeball, don't pixel-equal. <1% diff acceptable as
  aliasing/blend-noise.
- Spec gaps are first-class output.
- Don't regress main_menu or options screens.
- Read pattern headers from BOTH prior Ralphs before starting.
