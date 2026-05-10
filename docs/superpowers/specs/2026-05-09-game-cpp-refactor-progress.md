# game.cpp refactor — progress tracker

**Design:** [`2026-05-09-game-cpp-refactor-design.md`](./2026-05-09-game-cpp-refactor-design.md)
**Branch:** `refactor/game-cpp` (worktree at `.worktrees/refactor-game-cpp/`)

> **Picking this up?** Start at the **Phase 4 — Mechanical splits**
> checklist. The first item (`events.cpp`) is the smallest and validates
> the pattern — pure file moves with declarations staying in `game.h`. Run
> `bash tests/cli-agent/run.sh` after each commit; `30_lobby_login`
> exercises the most surface end-to-end. Memory note: the user prefers
> editing `docs/plans/` and this doc directly — skip the
> brainstorming/writing-plans skill machinery.

---

## Status

**Phases 0–3 done.** Every menu surface (MainMenu, Options × 4,
LobbyConnect, Update, Lobby + 6 panels, modals, MissionSummary) is a
Screen/Panel/Modal class. `Game::Create*Interface` /
`Process*Interface` and the legacy lobby pump are deleted; the lobby
chrome and pump now live on `LobbyScreen`. `MapDownloader` and
`AmbienceMixer` are extracted. `MessageModal` + `PasswordModal` push
onto the screen stack.

Sizes: `game.cpp` ≈ 2555 lines, `game.h` ≈ 192 lines. Design-doc
targets are 250 / 170; the gap is Phase 4 (mechanical splits of SDL
events, in-game lifecycle, headless glue, and the gameplay-state
`Tick` cases).

E2E: `00_ping`, `10_navigate`, `20_screenshot`, `30_lobby_login`. The
last drives the full login flow against a locally-spawned lobby —
regressions in `LobbyScreen` get caught automatically.

---

## Verification protocol (every checkbox)

1. **Build:** `cmake --build clients/silencer/build` — clean compile, no new warnings (pre-existing `C4804` at `IsLiveMultiplayer` is fine; `sprintf` deprecation noise is fine).
2. **Unity build:** `cmake -B clients/silencer/build-unity -DSILENCER_UNITY_BUILD=ON && cmake --build clients/silencer/build-unity`. Re-run `cmake -B build-unity` whenever you add a new `.cpp` file so `GLOB_RECURSE` picks it up.
3. **E2E suite:** `bash tests/cli-agent/run.sh`.
4. **Adjacent surface check** — if you migrated one screen, click through one neighbor.

---

## Phase 4 — Mechanical splits (file moves only)

Pure file moves. Each commit moves a group of `Game::` member
implementations into a new `.cpp` while declarations stay in `game.h`.
Behavior unchanged.

- [ ] **`src/game/events.cpp`** — `HandleSDLEvents`, `OnScancodeDown/Up`, `UpdateInputState`, `OpenFirstGamepad`, `PollGamepadState`. Smoke: keyboard nav on every screen.
- [ ] **`src/game/ingame.cpp`** — `LoadMap`, `UnloadGame`, `JoinGame`, `GiveDefaultItems`, `ShowDeployMessage`, `CheckForQuit/EndOfGame/ConnectionLost`, `ProcessInGameInterfaces`, `ShowTeamOverlays`. Smoke: start a game, play to end-of-mission.
- [ ] **`src/game/headless.cpp`** — `DrainControlQueue`, `PostFrameReplies`. Smoke: full E2E suite.
- [ ] **Split `Game::Tick`'s gameplay-state cases.** Switch dispatcher stays in `game.cpp`; each `case` body moves into a per-state method in `src/game/tick/tick_<state>.cpp`.
  - [ ] `tick_misc.cpp` — `FADEOUT` (smallest, validates the pattern).
  - [ ] `tick_replay.cpp` — `REPLAYGAME`.
  - [ ] `tick_hostjoin.cpp` — `HOSTGAME`, `JOINGAME`, `TESTGAME`.
  - [ ] `tick_singleplayer.cpp` — `SINGLEPLAYERGAME` (~391 lines).
  - [ ] `tick_ingame.cpp` — `INGAME` (~228 lines).

---

## Phase 5 — Cleanup

- [ ] **Delete dead members from `Game`.** Anything no longer referenced (old interface IDs absorbed by panels).
- [ ] **Verify size targets.** `game.cpp` ≈ 250, `game.h` ≈ 170, no UI file >300 except `lobby_screen.cpp` (~300). Split or accept overruns.
- [ ] **Update `clients/silencer/CLAUDE.md`** "Where to look" — currently points at `src/game.cpp`.
- [ ] **Final full smoke test** — main menu → options (each) → lobby connect → lobby → character → chat → game create → join → in-game → end-of-mission → back to lobby → quit. Every modal en route.

---

## Decisions log

- **Three-tier UI model.** `Screen` (top-level, stack-managed) / `Panel` (sub-`Interface` owned by a Screen as a member) / `Modal` (overlay-on-top with callback). Matches how the existing UI composes (lobby has co-visible sub-panels via `iface->AddObject(...)`).
- **Co-location.** Things used by exactly one screen live inside that screen's folder. Things used across screens live at the top of `ui/`.
- **`ScreenContext` is curated.** Only subsystem refs (World, Renderer, Lobby, KeyMap, Updater, AmbienceMixer, MapDownloader, window/renderdevice) and state-machine actions (GoToState, GoBack, RequestQuit, Push/Pop/Replace, ShowModal, ShowMessage). Not a junk drawer of per-screen accessors. Screens reach Game directly via `ctx.game.X` when they need raw Game state.
- **Gameplay-state `Tick` bodies stay on `Game`.** `INGAME`, `SINGLEPLAYERGAME`, `HOSTGAME`, `JOINGAME`, `TESTGAME`, `REPLAYGAME` aren't UI surfaces — they're the actual game running. Phase 4 splits them mechanically into `src/game/tick/tick_*.cpp`, not into Screens.
- **`Modal` is a `Screen` subclass; `IsOverlay()` lives on `Screen`.** The screen stack ticks the topmost non-overlay plus every overlay above it, so a modal blocks input (currentinterface flips to it) without freezing the screen beneath.
- **Green at every commit.** Old paths stay until each consumer migrates; last commit in a sequence is "delete the old."

---

## Conventions to keep

- **Anon-namespace uid enumerators** in panel `.cpp` files must be file-prefixed (`GCRT_BTN_*`, `GSEL_OVL_*`, `GJN_BTN_*`, `GTECH_OVL_*`, `LBY_INPUT_*`, …). Otherwise unity builds collide when sibling panels merge into one TU.
- **Panel constructors** take `LobbyScreen & owner` so they can call `owner.ShowXxx(ctx)` for swaps.
- **`World` friend grants for panels are temporary scaffolding, not architecture.** When tempted, prefer a narrow public accessor (`World::IsConnected()`, `World::IsIdle()`); only friend if the call site will move out in a later cleanup.
- **`LobbyScreen::Tick` order: panels first, then lobby pump.** Panel handlers must see fresh `button->clicked` / `textinput->enterpressed` before the rest of the tick clears them. Don't flip this back.
- **Public `Game` scaffolding** gets a comment naming the consumer or `// Removed in stage <X>.` so cleanup is a search-and-delete pass.

---

## CLI / headless verification

- The CLI now drives `TEXTINPUT` widgets via `set_text --uid N --text ...` (uid is the developer-assigned identifier visible in `inspect`). The login flow is fully exercisable headlessly.
- The `state` op exposes `lobby_state` (Lobby state machine: `IDLE`/`WAITING`/.../`AUTHENTICATING`/...). Use it to wait for `AUTHENTICATING` before clicking Login — earlier clicks are silently consumed.
- `--lobby-host HOST` / `--lobby-port N` flags on silencer override the compile-time default and the on-disk config (in-memory only — won't touch the dev's saved config).
- For E2E, set a fresh `HOME` so the silencer's data dir + Config writes land in a tmpdir.

---

## Open issues

- **Phase-2 "Create Game" regression** (unresolved): "Create Game" button kicked back to LobbyConnect on at least one user's machine; never root-caused. The flow now runs through `LobbyScreen::Tick`'s deferred-create state machine and is reachable headlessly via the new `30_lobby_login` E2E + `silencer-cli lobby` helpers — worth a fresh look.
- **`Game::GoBack` after `LOBBYCONNECT` → `MAINMENU` timing race**: multi-frame gap between `wait_for_state MAINMENU` resolving and the case body running `PushScreen`. CLI scripts hitting that path need `wait_frames --n 10` or the test's `wait_for_iface` helper. Pre-existing — not introduced by this refactor.
