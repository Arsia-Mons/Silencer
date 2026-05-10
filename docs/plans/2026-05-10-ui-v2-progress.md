# UI v2 — progress + handoff

**Companion design doc:** [2026-05-10-ui-v2-declarative-layout.md](2026-05-10-ui-v2-declarative-layout.md).

This doc is **mutable**. Each session updates the checklist below and
rewrites the "Handoff prompt" section at the bottom so the next
session can pick up cold.

---

## Status

- **Current branch:** `hv/layout` (worktree at `.worktrees/hv-layout/`)
- **PR in flight:** PR #1 — foundation + MainMenu pixel-parity-verified
- **Last verified state:** v2 vs legacy MainMenu byte-identical PPM dump

## Phases

Phase ordering is a proposal, not a commitment — reorder freely.

### PR #1 — foundation + MainMenu parity (in progress, uncommitted)

- [x] New `clients/silencer/src/ui/v2/` directory
- [x] `node.h` — `Node` + `NodeKind` + `ButtonType` + factories
      (`Group`, `Background`, `Sprite`, `Label`, `Button`)
- [x] `context.h` — `Context { Resources&, logical_w, logical_h, scale, version }`
- [x] `render.h` / `render.cpp` — tree-walking render pass
- [x] `screens/main_menu.{h,cpp}` — declarative `BuildMainMenu`
- [x] `preview.cpp` — `Game::RunPreview()` harness
- [x] `Renderer::DrawSpriteAt` public helper (only engine touch)
- [x] CLI flags on `Silencer.exe`:
      `--preview-screen NAME --preview-impl v2|legacy --dump-ppm PATH --preview-scale N`
- [x] CMakeLists.txt include paths for `src/ui/v2{,/screens}`
- [x] `CMAKE_EXPORT_COMPILE_COMMANDS ON` for clangd
- [x] PPM dump capability (P6, palette-indexed → RGB)
- [x] `clients/silencer/src/ui/v2/CLAUDE.md` + `AGENTS.md` (stub)
- [x] Byte-identical PPM verification: v2 and legacy match
      (legacy needs 61 `world.TickObjects()` calls so the logo
      animation `state_i` reaches frame 60)
- [ ] Commit + push + open PR
- [ ] In-session code review (spec + style passes via subagents)

### PR #2 — wire v2 MainMenu into the live engine

- [ ] Decide input-dispatch shape for v2 (hit-test → handler)
- [ ] Mouse / keyboard / gamepad routing for v2 Node trees
- [ ] Replace `MAINMENU` state-handler `PushScreen(MainMenuScreen)`
      with v2 path; live game renders via v2
- [ ] Delete `MainMenuScreen` class + files
- [ ] Update PPM-diff suite to test v2 against a *committed reference*
      PPM (legacy gone for this screen)

### PR #3+ — migrate remaining screens

One PR per screen; each ships byte-identical against its legacy
counterpart before the legacy class gets deleted.

- [ ] `OptionsScreen` (the sub-router)
- [ ] `OptionsDisplayScreen`
- [ ] `OptionsAudioScreen`
- [ ] `OptionsControlsScreen`
- [ ] `LobbyConnectScreen`
- [ ] `MissionSummaryScreen`
- [ ] `UpdateScreen`
- [ ] `LobbyScreen` + the five panels under `screens/lobby/panels/`
- [ ] `MessageModal` / `PasswordModal`

### PR N — first container + Yoga

Triggered by the first screen that wants a real stack / grid / centered
layout. Drops the absolute-positioned-only constraint.

- [ ] CMake `FetchContent` for Yoga (pinned tag)
- [ ] `VStack` / `HStack` / `Center` / `Padding` Node kinds
- [ ] Layout pass uses Yoga (builds parallel YGNode tree, reads back
      computed rects)
- [ ] Migrate a screen that exercises containers (likely the lobby
      panels)

### PR N+1 — in-game UI (chat / buy / tech)

- [ ] Migrate chat input out of `Player::Tick` (`actors/player.cpp`)
- [ ] Migrate buy menu
- [ ] Migrate tech menu
- [ ] Remove `Game::ProcessInGameInterfaces`
- [ ] Strip `Player::chatinterfaceid` / `buyinterfaceid` / `techinterfaceid`

### PR N+2 — Path B (resolution-flexible)

- [ ] `ui_scale = ChooseScale(window_h)` in renderer
- [ ] `Context.logical_{w,h} = window / scale`; `Context.scale` plumbed
- [ ] Renderer blits sprites at integer scale
- [ ] Hit-test divides input coords by `ui_scale`
- [ ] Drop the fixed-640×480 SDL3 GPU upscale path for the UI
      subsystem (world scene may keep it)
- [ ] Decide fate of `cfg.scalefilter` ("Smooth Scaling" toggle)

### Cleanup PR (after all screens migrated)

- [ ] Delete `clients/silencer/src/ui/components/`
- [ ] Delete `clients/silencer/src/ui/panels/`
- [ ] Delete `clients/silencer/src/ui/modals/`
- [ ] Delete `clients/silencer/src/ui/screens/` (legacy implementations)
- [ ] Delete widget-type `switch` arms in `interface.cpp` and
      `renderer.cpp`
- [ ] Retire `Game::currentinterface` global if no remaining use
- [ ] Rename `src/ui/v2/` → `src/ui/`

## Discovered quirks (worth remembering)

- **`PALETTE.BIN` palette 1 index 114 = (0, 0, 0).** Top-of-screen
  pixels in MainMenu are space (black) — *not* a missing render. The
  bg is space + Mars planet art; the orange you see is the Mars
  surface lower down. Don't mistake pixel-(0, 0)=black for "bg
  isn't rendering."
- **Logo animation state.** Bank 208 frame 0 has stale `.enabled` /
  `www.won.net` text baked in (pre-animation asset slot). v2 renders
  index 60 (the steady-state frame). Legacy needs ≥ 61 `Object::Tick`
  calls so `Overlay::Tick` for bank 208 advances `state_i` past the
  ramp.
- **Button defaults need a Tick.** `Button::Tick` sets the type-default
  `res_index` (e.g. 7 for B196x33). Without it, buttons render with
  `Sprite::res_index = 0` (the bg sprite slot) and look very wrong.
  `world.TickObjects()` once is enough for buttons specifically.
- **Renderer::Rect is private** but `BlitSurface` takes it. Added
  `Renderer::DrawSpriteAt(target, bank, index, anchor_x, anchor_y)`
  as the public entry point; v2's render path calls only that +
  `DrawText`.

## Build & verify cheat-sheet

```powershell
# From clients/silencer/
cmake --preset win-ninja-unity            # first time only
cmake --build --preset win-ninja-unity    # ~5-15 s incremental

# Interactive preview
cd build-unity
.\Silencer.exe --preview-screen main_menu --preview-impl v2

# Pixel-diff verification (must show "no differences")
.\Silencer.exe --headless --preview-screen main_menu --preview-impl v2     --dump-ppm v2.ppm
.\Silencer.exe --headless --preview-screen main_menu --preview-impl legacy --dump-ppm legacy.ppm
fc /b v2.ppm legacy.ppm
```

## Open decisions waiting on the user

Mirror of the design doc's "Open questions" — listed here too so the
checklist is self-contained:

- Bake sprite anchor offsets out of asset pipeline? (Affects layout
  primitives' coordinate system. Not blocking PR #1 / #2.)
- Hot-reload mechanism beyond restart-on-rebuild? (`dlopen` swap of
  the screen as a shared lib is on the table if iteration feels slow.)
- v2 animation support — per-node Tick callbacks, or external state
  via `Context`?
- When to migrate in-game UI (chat / buy / tech)?

---

## Handoff prompt — paste into a new session to continue

> *(Rewrite this block every session before stopping. The next session
> reads only this block + the linked design doc to pick up cold.)*

I'm continuing work on the **UI v2 declarative layout rewrite** for
the Silencer client. Read the design doc at
`docs/plans/2026-05-10-ui-v2-declarative-layout.md` and the progress
doc at `docs/plans/2026-05-10-ui-v2-progress.md` (this file). The
"Phases" section is the checklist; "Discovered quirks" is gotcha
context that will save you investigation time.

**Current state (end of session 2026-05-10):**

- PR #1 is code-complete in working tree, **uncommitted**, on branch
  `hv/layout` (worktree at `.worktrees/hv-layout/`).
- v2 MainMenu produces byte-identical PPM dumps against the legacy
  `MainMenuScreen` path. Verified locally via:
  ```
  cd clients/silencer/build-unity
  ./Silencer.exe --headless --preview-screen main_menu --preview-impl v2     --dump-ppm v2.ppm
  ./Silencer.exe --headless --preview-screen main_menu --preview-impl legacy --dump-ppm legacy.ppm
  fc /b v2.ppm legacy.ppm   # "no differences encountered"
  ```
- The new code is **dead code at runtime** — `BuildMainMenu` exists
  but nothing in the game's state machine calls it. The live engine
  still uses `MainMenuScreen`.
- Files changed: see `git status` — `src/ui/v2/` new dir,
  `src/render/renderer.{h,cpp}` (added `DrawSpriteAt`),
  `src/game/game.{h,cpp}` (preview CLI flags + `RunPreview()`),
  `src/main.cpp` (dispatch into preview),
  `CMakeLists.txt` (include paths + `CMAKE_EXPORT_COMPILE_COMMANDS`).

**Next chunk:** finish PR #1 by committing + pushing + opening the PR,
then run in-session subagent review (per the user's "PR before
in-session review" preference). After that, start PR #2 — wire v2
MainMenu into the live game state machine and design the input
dispatch path. See the design doc's "Open questions" for what needs
the user's call before PR #2 can move.

**Quirks you'll trip on if you don't read first:** logo bank 208
animation frame, palette 1 index 114 = black is intentional, buttons
need a Tick to set `res_index`. See "Discovered quirks" in this doc.

**User preferences (from memory):** terse responses, no narrating
internal deliberation, lightweight plans during refactors, PR before
review, don't invent constraints. Build via `cmake --preset
win-ninja-unity` (fastest). VS Build Tools dev env is set up via
`VsDevCmd.bat` (PowerShell trick documented in `Build & verify
cheat-sheet` above — needs `VCPKG_ROOT=C:\Users\Space Command\vcpkg`).
