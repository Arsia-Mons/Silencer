# Ralph Agent Instructions — LOBBYCONNECT spec-only rebuild

You are an autonomous coding agent extending the SDL3 hydration to
render the **Lobby Connect** screen (state `LOBBYCONNECT`),
matching `/tmp/real_lobbyconnect_dump.ppm`. Spec-only rebuild —
`clients/silencer/src/` is **off limits**.

This is the first screen using **sub-palette 2** (lobby palette).
Every prior screen used sub-palette 1 (menu palette). The candidate
likely needs a way to switch palettes per-screen.

## Inputs

- `docs/design/screen-lobby-connect.md` — the spec.
- `docs/design/{palette,sprite-banks,font,tick,widget-overlay,widget-button,widget-interface}.md`
- `shared/assets/`
- `shared/design/sdl3/` — main_menu, OPTIONS hub, OPTIONSAUDIO,
  OPTIONSDISPLAY, OPTIONSCONTROLS already validated. The
  screen-switch pattern (`SILENCER_DUMP_SCREEN`) is established.
  New widget surface: B52x21 button, TextBox, TextInput, sub-palette 2.
- Reference dump: `/tmp/real_lobbyconnect_dump.ppm` and `.png`.
- **Pattern memory:** read pattern headers from all five prior Ralphs'
  `progress.txt`.

`clients/silencer/src/` is **off limits**. Spec gaps go in
`progress.txt`.

## Important note about the canonical dump

The reference dump captures the screen with a *failed* lobby
connection — the textbox shows "Connecting to 127.0.0.1: 517 /
Hostname resolved / Connection failed" because no lobby server was
running when the dump was captured. **Textbox content is non-structural
runtime data.** The candidate should render the textbox structure
(border, scrollbar lane); exact text content is not gated.

Similarly: the active TextInput (Username) may or may not show a
blinking cursor depending on tick alignment. Cursor presence is
non-structural.

## Each iteration

Same structure as previous Ralphs.

1. Read `prd.json` + `progress.txt` (patterns first).
2. Pick highest-priority TODO item.
3. Implement ONE item.
4. Build, dump via `SILENCER_DUMP_SCREEN=lobby_connect`, PPM→PNG.
5. Visually A/B vs `/tmp/real_lobbyconnect_dump.png`.
6. Decide pass/fail honestly.
7. Discord DM:

   ```
   bun /Users/hv/.claude/skills/discord-dm/send.ts \
     "Ralph[lobby-connect] iter <N>: <id> <PASS|TODO> — <summary>" \
     /tmp/sdl3_dump/screen_00.png /tmp/real_lobbyconnect_dump.png
   ```

8. If pass: flip `prd.json`, commit `feat(design): [<id>] <name>`,
   APPEND `progress.txt`.

## Stop / discipline

- ONE item per iteration. ONE commit per iteration. APPEND progress.
- Eyeball, not pixel-equal. <1% diff acceptable. **Cursor / textbox
  content differences are non-structural and do NOT count toward the
  diff budget.** Categorize them explicitly.
- Don't regress prior screens.
- Read pattern headers from all five prior Ralphs' `progress.txt`.

## CRITICAL: do not quote the stop sentinel

Per the canonical-ralph skill update (CONTROLS Ralph hit a
false-positive stop in iter 4 by writing the literal sentinel inside
a negation): **never write the literal string
`<promise>COMPLETE</promise>` anywhere in your iteration output
unless you are actually emitting the stop signal because every item
in `prd.json` has `passes: true`.** If you need to discuss the stop
condition, paraphrase: "the COMPLETE signal", "the stop sentinel",
"the all-pass marker". The literal string is reserved for the
positive-emission case only.

Emit `<promise>COMPLETE</promise>` only when ALL items pass.
