# Ralph Agent Instructions — LOBBY populated content + 5 new modal/state screens

You are an autonomous coding agent extending the SDL3 hydration to:

1. Render the **populated** LOBBY (real chat lines, game list rows,
   per-agency character stats), matching `/tmp/real_lobby_populated_dump.ppm`.
2. Add 5 new screens / modals targeting their own reference dumps:
   - **lobby-game-create** → `/tmp/real_lobby-gamecreate_dump.ppm`
   - **lobby-game-join** → `/tmp/real_lobby-gamejoin_dump.ppm`
   - **lobby-game-tech** → `/tmp/real_lobby-gametech_dump.ppm`
   - **lobby-game-summary** → `/tmp/real_lobby-gamesummary_dump.ppm`
   - **updating** → `/tmp/real_updating_dump.ppm`

Spec-only rebuild — `clients/silencer/src/` is **off limits**.

## Inputs

- Specs: `docs/design/screen-lobby.md`,
  `screen-lobby-character.md`, `screen-lobby-gameselect.md`,
  `screen-lobby-chat.md`, `screen-lobby-game-create.md`,
  `screen-lobby-game-join.md`, `screen-lobby-game-tech.md`,
  `screen-lobby-game-summary.md`, `screen-updating.md`.
- Lower-layer specs (palette, sprite-banks, font, tick, widget-*).
- `shared/assets/`.
- `shared/design/sdl3/` — 8 screens already validated; you're adding
  to `RunDumpLobby` (or adding sibling `RunDumpFoo` functions for the
  modals/states).
- Reference dumps for each item under `/tmp/real_*_dump.ppm`.
- **Pattern memory:** read pattern headers from all 8 prior Ralphs'
  `progress.txt`.

`clients/silencer/src/` is **off limits**.

## Demo data hard-coded values

The populated LOBBY reference's chat / presence / games are seeded by
the lobby's `-demo` flag in `services/lobby/hub_demo.go`. The
candidate is spec-only and cannot read that file at runtime, but the
spec docs and `services/lobby/hub_demo.go` (which IS readable from
this Ralph as a *spec-adjacent* file, not engine source) supply the
canonical values. Hard-code them in the candidate's lobby render
path:

- **Games (in display order top-to-bottom):**
  Veterans Only, Tutorial, Capture the Tag, Casual Match #1
  (the order in the reference dump differs from `hub_demo.go`'s
  declaration order — match the dump's order)
- **Chat (top-to-bottom in reference, bottom-to-top scrolling means
  oldest first when reading top-down):**
  - Vector: anyone up for a round?
  - Solace: still waiting on Krieg's match to finish
  - Ember: we got 4 in casual #1
  - Vector: joining
  - Halcyon: gg everyone
- **Presence ("In Lobby" + "Pregame" + "Playing" sections):**
  - In Lobby: Halcyon, Ember, Solace, Vector, demo
  - Pregame: Quill —Capture the Tag—
  - Playing: Krieg —Casual Match #1—
- **Character stats (for current-agency NOXIS):**
  Level: 8, Wins: 47, Losses: 12, XP To Next Level: 220

These are reference-dump-derived and are **load-bearing** for E0.

## Per-item dump-screen mapping

Each item gates on a specific reference dump. The candidate produces
each by setting the right env var. Item ↔ env var ↔ reference:

| Item | Candidate env | Reference |
| --- | --- | --- |
| E0 | `SILENCER_DUMP_SCREEN=lobby` | `/tmp/real_lobby_populated_dump.ppm` |
| E1 | `SILENCER_DUMP_SCREEN=lobby_gamecreate` | `/tmp/real_lobby-gamecreate_dump.ppm` |
| E2 | `SILENCER_DUMP_SCREEN=lobby_gamejoin` | `/tmp/real_lobby-gamejoin_dump.ppm` |
| E3 | `SILENCER_DUMP_SCREEN=lobby_gametech` | `/tmp/real_lobby-gametech_dump.ppm` |
| E4 | `SILENCER_DUMP_SCREEN=lobby_gamesummary` | `/tmp/real_lobby-gamesummary_dump.ppm` |
| E5 | `SILENCER_DUMP_SCREEN=updating` | `/tmp/real_updating_dump.ppm` |

The candidate dispatch (in `shared/design/sdl3/src/main.cpp`'s
`main()`) needs a new branch per env var, each calling its own
`RunDumpFoo()`. Reuse `RunDumpLobby`'s structure as a starting point
for the lobby-derivatives; reuse a fresh shape for `updating`.

## Each iteration

Same structure as previous Ralphs.

1. Read `prd.json` + `progress.txt` (patterns first).
2. Pick highest-priority TODO item.
3. Implement ONE item.
4. Build, dump with **the right env var for that item**, PPM→PNG.
5. Visually A/B vs **the right reference for that item**.
6. Decide pass/fail honestly.
7. Discord DM:

   ```
   bun /Users/hv/.claude/skills/discord-dm/send.ts \
     "Ralph[lobby-extras] iter <N>: <id> <PASS|TODO> — <summary>" \
     /tmp/sdl3_dump/screen_00.png /tmp/real_<right_ref>.png
   ```

8. If pass: flip `prd.json`, commit `feat(design): [<id>] <name>`,
   APPEND `progress.txt`.

## Stop / discipline

- ONE item per iteration. ONE commit per iteration. APPEND progress.
- Eyeball, not pixel-equal. Each item gates on its own region/screen.
- Don't regress prior screens (run a regression sweep at the end of
  each iteration: dump the 7 prior screens with their env vars and
  confirm bytewise diffs unchanged from baselines).
- **Do not write the literal string `<promise>COMPLETE</promise>`
  anywhere unless emitting it.** Use "the COMPLETE signal" /
  "the all-pass marker" otherwise.

Emit `<promise>COMPLETE</promise>` only when ALL items pass.
