# Ralph Agent Instructions — OPTIONSDISPLAY spec-only rebuild

You are an autonomous coding agent extending the SDL3 hydration to
render the **Display Options** screen (state `OPTIONSDISPLAY`),
matching `/tmp/real_optionsdisplay_dump.ppm`. Spec-only rebuild —
`clients/silencer/src/` is **off limits**.

## Inputs

- `docs/design/screen-options-display.md` — the spec for this screen.
- `docs/design/{palette,sprite-banks,font,tick,widget-overlay,widget-button,widget-interface}.md`
- `shared/assets/`
- `shared/design/sdl3/` — main_menu, options, options_audio screens are
  already complete. `RunDumpOptionsAudio` already implements the
  title-overlay, B220x33 button widget, and on/off pill indicators
  (bank 6 idx 12 / **15** — not 14, see spec) used by Display.
  Reuse them. The only structurally new thing is **two** toggle rows
  instead of one, with row stride +53.
- Reference dump: `/tmp/real_optionsdisplay_dump.ppm` and `.png`.
- **Pattern memory:** read pattern headers from
  `.ralph/main-menu/progress.txt`, `.ralph/options/progress.txt`, and
  `.ralph/options-audio/progress.txt` before starting.

`clients/silencer/src/` is **off limits**. Spec gaps go in
`progress.txt`.

## Each iteration

Same structure as previous Ralphs.

1. Read `prd.json` + `progress.txt` (patterns first).
2. Pick highest-priority TODO item.
3. Implement ONE item.
4. Build, dump display via `SILENCER_DUMP_SCREEN=options_display`,
   PPM→PNG.
5. Visually A/B vs `/tmp/real_optionsdisplay_dump.png`.
6. Decide pass/fail honestly.
7. Discord DM:

   ```
   bun /Users/hv/.claude/skills/discord-dm/send.ts \
     "Ralph[display] iter <N>: <id> <PASS|TODO> — <summary>" \
     /tmp/sdl3_dump/screen_00.png /tmp/real_optionsdisplay_dump.png
   ```

8. If pass: flip `prd.json`, commit `feat(design): [<id>] <name>`,
   APPEND `progress.txt`.

## Stop / discipline

- ONE item per iteration. ONE commit per iteration. APPEND progress.
- Eyeball, not pixel-equal. <1% diff acceptable.
- Don't regress main_menu, options, options_audio.
- Read pattern headers from all prior Ralphs' `progress.txt` first.
- Emit `<promise>COMPLETE</promise>` only when ALL items pass.
