# Ralph Agent Instructions — LOBBY chrome spec-only rebuild

You are an autonomous coding agent extending the SDL3 hydration to
render the **LOBBY chrome** (state `LOBBY`, post-authentication), matching
`/tmp/real_lobby_dump.ppm`. **Scope:** chrome + header + sub-interface
bounding boxes. Sub-interface internal contents (chat messages, game
list rows, character stats, agency toggles) are **explicitly out of
scope** and reserved for future per-sub-interface Ralphs.

Spec-only rebuild — `clients/silencer/src/` is **off limits**.

## Inputs

- `docs/design/screen-lobby.md` — the spec.
- All previously-validated specs.
- `shared/assets/`
- `shared/design/sdl3/` — six prior screens validated.
- Reference dump: `/tmp/real_lobby_dump.ppm` and `.png`.
- **Pattern memory:** read pattern headers from all six prior Ralphs.

`clients/silencer/src/` is **off limits**.

## Important: chrome-only scope

The reference dump shows an "empty" lobby because the harness uses
`world.lobby.state = Lobby::AUTHENTICATED` injection (no real server,
no chat messages, no games). The candidate is gated on **structural
chrome** only:

- Bank-7 idx-1 panel sprite renders.
- Header (Silencer / version / map / Go Back) renders.
- Sub-interface regions are visible as empty bounded regions (the
  panel chrome surrounding them) — interior content is non-gated.

Diff residue from missing sub-interface internals (e.g., the planet
visual in the GameSelect tab, agency icons in the Character panel)
is **non-structural for this Ralph** and acceptable as long as the
chrome + header are pixel-correct.

## Each iteration

Same structure as previous Ralphs.

1. Read `prd.json` + `progress.txt` (patterns first).
2. Pick highest-priority TODO item.
3. Implement ONE item.
4. Build, dump via `SILENCER_DUMP_SCREEN=lobby`, PPM→PNG.
5. Visually A/B vs `/tmp/real_lobby_dump.png`.
6. Decide pass/fail honestly per the **chrome-only** scope.
7. Discord DM:

   ```
   bun /Users/hv/.claude/skills/discord-dm/send.ts \
     "Ralph[lobby] iter <N>: <id> <PASS|TODO> — <summary>" \
     /tmp/sdl3_dump/screen_00.png /tmp/real_lobby_dump.png
   ```

8. If pass: flip `prd.json`, commit `feat(design): [<id>] <name>`,
   APPEND `progress.txt`.

## Stop / discipline

- ONE item per iteration. ONE commit per iteration. APPEND progress.
- Eyeball, not pixel-equal. Diff budget is generous because
  sub-interface interiors are non-gated; ~5–15% diff is normal here.
- Don't regress prior screens.
- **Do not write the literal string `<promise>COMPLETE</promise>`
  anywhere unless emitting it.** Use "the COMPLETE signal" or
  similar paraphrase otherwise.

Emit `<promise>COMPLETE</promise>` only when ALL items pass.
