# Clay UI Visual Regression Audit

Date: 2026-05-14
Branch audited: `hv/clay-ui-migration` at `6ad8de0`
Baseline: `origin/main` at `6e2a99c`

Captures and side-by-side composites live in
[`2026-05-14-clay-ui-visual-regression/`](./2026-05-14-clay-ui-visual-regression/).

## Method

Used the new
[`e2e-visual-regression`](../../shared/skills/visual-regression-journeys/SKILL.md)
skill to drive both branches via the silencer CLI, capture every reachable
UI surface, and pixdiff each pair. Both branches built locally;
`origin/main` was checked out into a git worktree at `/tmp/silencer-main`.

Branches covered:

- 7 comparable pairs (main menu, options root, options controls,
  options display, options audio, lobby connect, in-game HUD).
- 8 branch-only captures (1280×720 reflows + in-game overlay modes via
  `ingame_ui_mode` — origin/main lacks both ops).

## Findings

### Real regression: Options Controls text rendering

[`composite`](./2026-05-14-clay-ui-visual-regression/03_options_controls_640x480.png).
pixdiff 34.6%.

Origin/main renders the keybind list cleanly:
`Move Up: [Up]`, `Move Down: [Down]`, `Move Left: [Left]`, etc., with the
`Preset:` dropdown showing `Default`.

The branch renders garbage in both columns:

| Row label (expected)         | Row label (rendered) | Binding (expected) | Binding (rendered) |
|------------------------------|----------------------|--------------------|--------------------|
| Move Up:                     | ÍòÀ                  | Up                 | X                  |
| Move Down:                   | Move Dow è           | Down               | XxH                |
| Move Left:                   | (missing)            | Left               | XxH                |
| Move Right:                  | m;Ð                  | Right              | XxH                |
| Aim Up/Left:                 | ým;Ð                 | Up                 | X                  |
| Preset:                      | Preset:              | Default            | -                  |

The garbled text persists through a 5-second post-state settle, ruling out
fade-in race. Free-floating `Up Down` labels appear at the bottom of the
panel where no widget should exist.

Strong signal of a **string-arena lifetime bug**: Clay primitives store
text by pointer; if a label string is freed or its arena is reset before
the render pass reads it, the next allocation overwrites that memory with
the bytes that show up on screen. Most likely introduced by one of:

- Move 1's `UiFrameContext` consolidation (per-frame arena reset
  ordering changed).
- Move 4's `controls_keybind_list` / `controls_rebind_capture` extraction
  (label-builder code moved into a new TU; row-label strings may now have
  shorter lifetimes than the Clay text payloads referencing them).

This regression was missed by:

- 19/19 unit tests (`build/tests/silencer_tests`).
- 8/8 CLI E2E scripts including `11_keyboard_navigation`, `12_controls_scroll`,
  `13_password_modal`, `14_directional_navigation`.
- `60_ui_architecture_boundaries.sh`.
- An independent auditor reading the code and the spec acceptance criteria.

Visual regression is the only gate that caught it. Fixing this needs a
debug pass through the keybind-row builder to find the dangling pointer.

### Design-call divergence: Main menu button styling

[`composite`](./2026-05-14-clay-ui-visual-regression/01_mainmenu_640x480.png).
pixdiff 41.1%.

Origin/main: classic Silencer oval buttons (Tutorial / Connect To Lobby /
Options / Exit) spread vertically with breathing room. Same legacy oval
appears on every Options sub-screen.

Branch: rectangular Clay buttons stacked tightly together with no
vertical padding between them. Function identical; visual identity
diverged.

The architecture goal said "Preserve recognizable menu/lobby chrome,
button feel" and "Modernize the implementation, not the game's identity."
This change is closer to "modernize the identity" than "modernize the
implementation." Not a hard regression — it's a product decision.

This pattern repeats across every menu surface (Options Root, Display,
Audio, Lobby Connect): wherever the legacy oval `Button` widget appeared,
the Clay rewrite produced a tighter rectangle.

### Clean: Lobby connect, In-Game HUD, Options Display/Audio

| Surface          | pixdiff | Verdict                                   |
|------------------|---------|-------------------------------------------|
| In-Game HUD      | 1.3%    | Visually identical. World + HUD pixel-clean. |
| Options Audio    | 5.6%    | Content identical; only button shape diverges. |
| Lobby Connect    | 5.9%    | Structurally identical. Tiny offsets in chrome borders. |
| Options Display  | 7.6%    | Content identical; only button shape diverges. |

### Unreachable from Tutorial: in-game overlay modes

[`branch-only`](./2026-05-14-clay-ui-visual-regression/) captures
21–24 (player list, buy menu, tech menu, chat overlay) show the in-game
HUD with tutorial text overlaid — the targeted overlay never appears.
Tutorial mode either suppresses the overlay activation or lacks the
world stations (inventory, tech) those overlays bind to. The
`ingame_ui_mode --mode X` CLI op probably returns success but the
overlay isn't visible.

These four surfaces need re-validation against a real multiplayer
lobby (or a non-tutorial single-player scenario). The
`51_ingame_ui_overlays.sh` E2E that exercises these modes uses
`ingame_ui_mode --mode status` to assert overlay activation via JSON
rather than visual confirmation, so it can't tell us whether the
overlay actually rendered.

## Recommended follow-ups, in order

1. **Fix the Options Controls text-garble.** Hard regression. Trace the
   row-label string lifetime through `controls_keybind_list.cpp` and the
   row-label arenas to find the dangling pointer. The visual evidence
   is unambiguous.
2. **Decide on the main-menu button styling.** Product call. Either
   restore the oval `Button` look (which would need a sprite-backed
   Clay primitive matching the legacy panel art), or accept the
   rectangular Clay shape and update the architecture-goal language.
3. **Re-validate the in-game overlay modes against a real lobby.** The
   tutorial-mode captures don't tell us whether buy / tech / chat /
   player list overlays still render correctly after Move 3's HUD
   decomposition. The `51_ingame_ui_overlays.sh` E2E gating is JSON
   state checks, not pixel checks.
4. **Re-run** `bash shared/skills/visual-regression-journeys/run.sh`
   after each fix to verify the diff drops.

## Why this audit exists

The Clay UI migration was declared "Faithful" by an independent code
auditor and passed every gate. Visual regression caught a real string
bug invisible to all those gates. Add visual regression to the standard
gate set going forward — see the
[`e2e-visual-regression`](../../shared/skills/visual-regression-journeys/SKILL.md)
skill for the orchestrator.
