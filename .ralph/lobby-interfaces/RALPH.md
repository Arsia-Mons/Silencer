# Ralph Agent Instructions — LOBBY sub-interfaces spec-only rebuild

You are an autonomous coding agent extending the SDL3 hydration to
render the **three sub-interfaces inside the LOBBY screen**:

- CharacterInterface (left panel — player profile + agency toggles)
- GameSelectInterface (right panel — Active Games list + Create/Join)
- ChatInterface (bottom-left panel — channel chat + presence + input)

The reference for all three is the same dump:
`/tmp/real_lobby_dump.ppm`. This Ralph gates each sub-interface on
its own region of that dump.

Spec-only rebuild — `clients/silencer/src/` is **off limits**.

## Inputs

- `docs/design/screen-lobby-character.md`,
  `docs/design/screen-lobby-gameselect.md`,
  `docs/design/screen-lobby-chat.md` — the per-sub-interface specs.
- `docs/design/screen-lobby.md` — the parent (chrome already validated).
- `docs/design/{palette,sprite-banks,font,tick,widget-overlay,widget-button,widget-interface}.md`
- `shared/assets/`
- `shared/design/sdl3/` — seven prior screens validated end-to-end
  including `RunDumpLobby` chrome. The new work is filling in the
  three sub-interface regions inside `RunDumpLobby` (or via separate
  per-sub-interface render functions you decide to introduce).
- Reference dump: `/tmp/real_lobby_dump.ppm` and `.png`.
- **Pattern memory:** read pattern headers from all seven prior
  Ralphs' `progress.txt`.

`clients/silencer/src/` is **off limits**.

## Important: empty-data scope

The reference dump was captured with the engine's auth-bypass harness
and *no Go lobby server running*. Therefore:

- Username text = local config value (whatever you render is fine,
  position is what's gated)
- Level / Wins / Losses / Etc text = **empty**
- SelectBox (game list) = **empty**
- Chat textbox = **empty**
- Presence textbox = **empty**
- Channel name overlay = **empty** or default
- Chat input = **empty**
- Selected-game info overlays = **empty**

Gate each sub-interface on **structural rendering** — the panel
chrome borders, the bordered regions for empty content areas, the
scrollbar thumbs at scrollposition=0, the **action buttons present
and positioned correctly** (Create Game, Join Game, agency toggles).

## Each iteration

Same structure as previous Ralphs.

1. Read `prd.json` + `progress.txt` (patterns first).
2. Pick highest-priority TODO item.
3. Implement ONE item.
4. Build, dump via `SILENCER_DUMP_SCREEN=lobby`, PPM→PNG.
5. Visually A/B vs `/tmp/real_lobby_dump.png`. Each sub-interface item
   gates on its own region only.
6. Decide pass/fail honestly per the **empty-data scope**.
7. Discord DM:

   ```
   bun /Users/hv/.claude/skills/discord-dm/send.ts \
     "Ralph[lobby-interfaces] iter <N>: <id> <PASS|TODO> — <summary>" \
     /tmp/sdl3_dump/screen_00.png /tmp/real_lobby_dump.png
   ```

8. If pass: flip `prd.json`, commit `feat(design): [<id>] <name>`,
   APPEND `progress.txt`.

## Stop / discipline

- ONE item per iteration. ONE commit per iteration. APPEND progress.
- Eyeball, not pixel-equal. Diff budget per item is generous because
  empty-data interiors are non-gated; gate on structural chrome /
  widget positions / button outlines / sub-interface bbox borders.
- Don't regress prior screens.
- **Do not write the literal string `<promise>COMPLETE</promise>`
  anywhere unless emitting it.** Use "the COMPLETE signal" or
  paraphrase otherwise.

Emit `<promise>COMPLETE</promise>` only when ALL items pass.
