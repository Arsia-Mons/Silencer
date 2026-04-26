# Ralph Agent Instructions — OPTIONSCONTROLS spec-only rebuild

You are an autonomous coding agent extending the SDL3 hydration to
render the **Configure Controls** screen (state `OPTIONSCONTROLS`),
matching `/tmp/real_optionscontrols_dump.ppm`. This is the most
structurally complex options screen — it adds two widget types not
yet validated by prior Ralphs: the **bank-7 idx-7 frame panel** and
the **scrollbar**. Spec-only rebuild — `clients/silencer/src/` is
**off limits**.

## Inputs

- `docs/design/screen-options-controls.md` — the spec.
- `docs/design/{palette,sprite-banks,font,tick,widget-overlay,widget-button,widget-interface}.md`
- `shared/assets/`
- `shared/design/sdl3/` — main_menu, options, options_audio,
  options_display screens are all complete and validated. Reuse all
  pieces. The new structural surface for this Ralph is:
  - bank 7 idx 7 frame panel (new sprite, may use tile-mode RLE)
  - B112x33 narrower button variant (new sprite)
  - BNONE button (text-only, no chrome)
  - scrollbar widget (new — see screen-options-controls.md for fields
    and the spec gap re: widget-scrollbar.md not yet existing)
  - 6-row form pattern (label + B112x33 + BNONE + B112x33 per row)
- Reference dump: `/tmp/real_optionscontrols_dump.ppm` and `.png`.
- **Pattern memory:** read pattern headers from
  `.ralph/{main-menu,options,options-audio,options-display}/progress.txt`
  before starting.

`clients/silencer/src/` is **off limits**. Spec gaps go in
`progress.txt` — flag freely; `widget-scrollbar.md` is already
known-missing.

## Each iteration

Same structure as previous Ralphs.

1. Read `prd.json` + `progress.txt` (patterns first).
2. Pick highest-priority TODO item.
3. Implement ONE item.
4. Build, dump via `SILENCER_DUMP_SCREEN=options_controls`.
5. PPM→PNG.
6. Visually A/B vs `/tmp/real_optionscontrols_dump.png`.
7. Decide pass/fail honestly.
8. Discord DM:

   ```
   bun /Users/hv/.claude/skills/discord-dm/send.ts \
     "Ralph[controls] iter <N>: <id> <PASS|TODO> — <summary>" \
     /tmp/sdl3_dump/screen_00.png /tmp/real_optionscontrols_dump.png
   ```

9. If pass: flip `prd.json`, commit `feat(design): [<id>] <name>`,
   APPEND `progress.txt`.

## Stop / discipline

- ONE item per iteration. ONE commit per iteration. APPEND progress.
- Eyeball, not pixel-equal. <1% diff acceptable.
- Don't regress prior screens (main_menu, options, options_audio,
  options_display).
- Read pattern headers from all four prior Ralphs' `progress.txt`.
- If a widget needs a new spec doc (widget-scrollbar.md, widget-frame.md),
  flag in progress.txt — don't write spec docs from this Ralph; that's
  the orchestrator's call.
- Emit `<promise>COMPLETE</promise>` only when ALL items pass.
