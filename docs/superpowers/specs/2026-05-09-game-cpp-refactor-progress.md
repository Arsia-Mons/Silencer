# game.cpp refactor — progress tracker

**Design:** [`2026-05-09-game-cpp-refactor-design.md`](./2026-05-09-game-cpp-refactor-design.md)
**Branch:** `refactor/game-cpp` (worktree at `.worktrees/refactor-game-cpp/`)

This doc tracks execution across sessions. Tick boxes as work lands; update the status note + decisions log as we learn.

---

## The green-at-every-step rule

**Every checkbox below is one mergeable commit. After each one, the game builds and is interactively playable.** No item assumes "we'll fix it in the next commit." If a step would break the game mid-way, it's split further.

The pattern this forces: when we introduce new abstractions, we leave the old code in place until each consumer migrates over individually. New + old coexist for the duration of a phase. The last commit in each phase is "delete the old."

### Verification protocol

Before marking any checkbox done, run all four:

1. **Build:** `cmake --build build` — clean compile, no new warnings.
2. **E2E suite:** `bash tests/cli-agent/e2e/00_ping.sh && bash tests/cli-agent/e2e/10_navigate.sh && bash tests/cli-agent/e2e/20_screenshot.sh` (or platform equivalent).
3. **Interactive smoke test:** launch the game, exercise the surface you touched (specifics per phase below).
4. **Confirm no regression in adjacent surfaces** — if you migrated one screen, also click through one neighboring screen.

Don't tick the box if any of the four fails. Investigate, fix, re-verify.

---

## Status

Phase 1 complete (all 8 steps landed in one commit per user direction). Verified: clean Release build, clean jumbo/unity build (no cyclic dependencies introduced), all 3 E2E scenarios pass (00_ping, 10_navigate, 20_screenshot). The screen stack is empty at runtime — every menu still flows through the legacy `Create*Interface`/`Process*Interface` helpers.

Phase 2 landed in one commit (both extractions). `MapDownloader` and `AmbienceMixer` are constructed and own all the moved state; `World` was given `friend class MapDownloader;` and `friend class AmbienceMixer;` so the two collaborators retain the same private-access reach Game had. Verified: clean Release build (only the pre-existing C4804 warning at `IsLiveMultiplayer`). **Suspected regression on the create-game flow** — clicking "Create Game" on the lobby kicks back to the lobby connect screen on at least one user's machine; not yet root-caused. Unity build + interactive lobby smoke test pending.

Phase 3 in progress — `MainMenuScreen` + all four options screens migrated. `Game::CreateMainMenuInterface`/`ProcessMainMenuInterface` and `Game::CreateOptions{,Controls,Display,Audio}Interface` deleted. Each `case <STATE>:` body in `Game::Tick` is now `if(stateisnew){ DestroyAllObjects(); PushScreen(make_unique<...>()); stateisnew = false; }`; per-tick logic lives in the screen's `Tick`. Side effects of this first migration:
- `ScreenContext::GoToState`/`GoBack`/`RequestQuit`/`PushScreen`/`PopScreen`/`ReplaceScreen` are wired (no longer assert-stubs); `ShowModal`/`ShowMessage` still stubs (no consumers yet).
- New `ScreenContext` actions for the options screens: `LoadActiveKeymap`, `CycleKeybindPreset`, `ForkActiveProfileIfBuiltin`, `SetFullscreen`, `SetScaleFilter`, `KeyName`. They delegate to `Game` via the existing friend grant — no `Game&` accessor leaks into screen code.
- `ScreenContext` is a `friend class` of `Game` so it can dispatch to private `GoToState` and reach `window`/`renderdevice` for the display actions.
- `Game::PushScreen`/`PopScreen` sync `currentinterface` so legacy input dispatch (`HandleSDLEvents`, TUI mouse) keeps routing to the active screen's interface.
- `Game::GoToState` now sets `screenStackPendingTeardown`; `Game::Tick` pops the stack at the next frame's start, so a screen calling `GoToState` from inside its own `Tick` isn't destroyed mid-call.
- The state enum (`MAINMENU`, `LOBBY`, …) was extracted to `clients/silencer/src/game/game_state.h` so screens can name states without including `game.h`. Existing `game.cpp` callsites are preserved via a file-scope `using namespace GameState;`.
- `World::GetVersion()` accessor added so `MainMenuScreen::Build` can render the version string without needing `friend class` reach into `World`.
- `Game::LegacyView`/`Game::ViewLegacy`/`Game::WriteLegacy` deleted — `OptionsControlsScreen` now owns the `LegacyView` round-trip helpers as private statics. `Game` keeps `GetKeyName`/`GetActionKeyDisplayName`/`LoadActiveKeymap`/`CycleKeybindPreset`/`ForkActiveProfileIfBuiltin` because tutorial overlays and `ControlDispatch` consume them too.
- Per-screen widget pointers (`keynameoverlay`/`c1button`/`cobutton`/`c2button`/`presetbutton`/`optionscontrolstick`) deleted from `Game`; live as members on `OptionsControlsScreen`.
- Anon-namespace uid enumerators in display/audio screens are file-prefixed (`DSP_BTN_SAVE`, `AUD_BTN_SAVE`) so they don't collide under `SILENCER_UNITY_BUILD` (one merged TU per batch).

Verified: clean Release + unity builds (only the pre-existing C4804 warning); `00_ping`/`10_navigate`/`20_screenshot` E2E pass. Agent-driven smoke through Options → Controls → Display → Audio (with Save/Cancel/Go Back round-trips) confirms all four screens render and dispatch correctly. Interactive smoke (rebind+cycle+save persistence; fullscreen toggle; music toggle persistence) pending.

`LobbyConnectScreen` and `UpdateScreen` migrated together. The `case LOBBYCONNECT:` and `case UPDATING:` bodies in `Game::Tick` collapsed to the canonical `if(stateisnew){ <one-time setup>; PushScreen(make_unique<...>()); stateisnew = false; }else{ <ambience music> }` shape; per-tick logic moved into the screen `Tick` methods. Side effects:
- `AmbienceMixer & ambienceMixer` added to `ScreenContext` so `LobbyConnectScreen::Tick` can preserve the legacy `FadedIn()` gate that delays the first lobby-state-machine pump until the menu music has crossfaded in. Matches the design-doc decisions log entry that names AmbienceMixer as a subsystem ref.
- New `ScreenContext` actions: `LaunchStage2()` (delegates to `Game::LaunchStage2` — process/SDL teardown stays on Game) and `SetLocalUsername(const char *)` (writes the captured login text into Game's `localusername` buffer, since `CharacterPanel` on the un-migrated `LobbyScreen` still reads it).
- `Game::motdprinted` deleted; the flag now lives as a private `LobbyConnectScreen` member, reset on construction (which happens on every entry into `LOBBYCONNECT` via `PushScreen`) — same semantics as the legacy reset.
- `Game::updateinterface` member deleted; `lobbyconnectinterface` was never a separate field (the legacy code stored its id in `currentinterface` only), so nothing to delete there.
- `Game::CreateLobbyConnectInterface`, `Game::ProcessLobbyConnectInterface`, `Game::CreateUpdateInterface`, `Game::ProcessUpdateInterface` deleted. `Game::LaunchStage2` kept on Game per the user direction (touches process spawn + SDL teardown state).
- Anon-namespace uid enumerators are file-prefixed (`LBY_BTN_LOGIN`/`LBY_BTN_CANCEL`/`LBY_INPUT_USERNAME`/`LBY_INPUT_PASSWORD`; `UPD_BTN_UPDATE`/`UPD_BTN_CANCEL`/`UPD_BTN_RETRY`/`UPD_BTN_DOWNLOAD`/`UPD_OVERLAY_STATUS`/`UPD_OVERLAY_PROGRESS`) so they don't collide under `SILENCER_UNITY_BUILD`.

Verified: clean regular + unity Debug builds (only the pre-existing C4804 warning at `IsLiveMultiplayer`). Authentic interactive smoke for the full lobby→update path: ran a local lobby with `-version "99999" -update-manifest manifest.json` (manifest pointed at `https://example.invalid/...` to flip `updateavailable=true`); client transitioned LobbyConnect → UpdateScreen on version-reject, displayed PROMPTING, dispatched Update → DOWNLOADING → FAILED with "Could not resolve hostname" and the Retry+Cancel button pair at the correct positions. Pixel-identical to the pre-refactor render (the screen code is a byte-for-byte move, only the dispatch surface changed).

Stage A of LobbyScreen migration landed. `LobbyScreen` is a thin adapter at `clients/silencer/src/ui/screens/lobby/lobby_screen.{h,cpp}` whose `Build` delegates to `Game::CreateLobbyInterface()` and whose `Tick` delegates to a new `Game::TickLobbyBody()`. The `case LOBBY:` body in `Game::Tick` now follows the canonical shape: `if(stateisnew){ <reset interfaces> + PushScreen(make_unique<LobbyScreen>()); stateisnew = false; }else{ <ambience music> }`. The 700+ lines of lobby pump logic (state-machine, deferred CreateGame upload pump, modal teardown, post-create handoff) moved verbatim into `Game::TickLobbyBody`; nothing was reshaped, so per-stage smoke can compare against pre-refactor behavior byte-for-byte. Side effects:
- New `ScreenContext` actions: `BuildLegacyLobbyInterface()` (returns interface id, also sets `game.lobbyinterface` so legacy code keeps working) and `TickLegacyLobbyBody()` (delegates to `Game::TickLobbyBody`).
- `src/ui/screens/lobby/` added to `target_include_directories`. The `lobby/panels/` and `lobby/modals/` paths were already wired.
- No anon-namespace uids in `lobby_screen.cpp` yet (Stage A is a wrapper); per-panel prefixes (`CHR_*`, `CHT_*`, `GSEL_*`, `GCRT_*`, `GJN_*`, `GTECH_*`) come in stages B–G.

Verified: clean regular + unity Debug builds (only the pre-existing C4804). E2E `00_ping`/`10_navigate`/`20_screenshot` pass. Interactive lobby login flow not exercised — the control protocol's `set_text` op only targets `TEXTBOX` widgets, not `TEXTINPUT` (pre-existing CLI limitation), so driving the lobby_connect login form headlessly isn't currently possible. Verification rests on: (a) builds, (b) E2E passing, (c) `case LOBBY:` body is a literal mechanical move from the old else-branch into `Game::TickLobbyBody` with no logic reshape.

Stage B landed: `CharacterPanel` at `clients/silencer/src/ui/screens/lobby/panels/character_panel.{h,cpp}`. The character box (username + agency toggles + LEVEL/WINS/LOSSES/XP overlays) is now built and ticked by the panel; `LobbyScreen` owns it as a value member (always-alive) and calls `character.Build(ctx, lobbyiface)` after the legacy interface chrome lands. `Game::CreateCharacterInterface` deleted; `iface->AddObject(characterinterface)` removed from `Game::CreateLobbyInterface`. The `TOGGLE` and `OVERLAY uid 2-5` arms inside `Game::ProcessLobbyInterface` deleted (their guards were `iface->id == characterinterface`, now never satisfied since the character iface lives outside the legacy walk). `agencychanged` and `oldselectedagency` Game members removed; both relocate as private members of `CharacterPanel` (initial `agencychanged = true` on construction matches the legacy stateisnew bootstrap). `Game::characterinterface` member kept on Game (Stage H removes it) — `CharacterPanel::Build` writes it back via the new `ScreenContext::SetCharacterInterfaceId` action so `Game::GetSelectedAgency` and the lobby pump's joining/create flows keep reading it. New `ScreenContext` actions: `GetSelectedAgency`, `SetCharacterInterfaceId`, `NotifyAgencyChanged` (latter wraps `world.state == World::CONNECTED` since `World::state` is private). New `Game::SetAgencyIfConnected` is the friend-gated landing pad. Anon-namespace uid enumerators prefixed `CHR_TGL_*`/`CHR_OVL_*`.

Verified: clean regular + unity Debug builds (had to re-run `cmake -B build-unity` so GLOB_RECURSE picked up the two new .cpp files). E2E `00_ping`/`10_navigate`/`20_screenshot` pass. Interactive agency-toggle smoke not exercised (same TEXTINPUT/login limitation as Stage A).

Stage C landed alongside an architecture cleanup of the migration scaffolding the user pushed back on. **`ScreenContext` is now a pure subsystem-refs holder** (World, Renderer, Lobby, KeyMap, Updater, AmbienceMixer, SDL_Window/RenderDevice) plus the global state-machine actions (GoToState, GoBack, RequestQuit, PushScreen, PopScreen, ReplaceScreen, ShowModal, ShowMessage). Per-screen / per-panel knowledge no longer lives there. Screens reach Game directly through a new public `ScreenContext::game` ref when they need Game state. To support that without making `Game` a grab bag in turn:
- `World::IsConnected()` added as a public method (just exposes the `state == CONNECTED` test). The agency-changed branch in `CharacterPanel::Tick` calls `world.IsConnected()` directly, no Game shim.
- `Game::GetSelectedAgency()` deleted entirely. Outside callers (the lobby pump's joining/create flows) now read `Config::GetInstance().defaultagency`, which is already kept in sync with the toggle widget on every change. `CharacterPanel` reads its own toggles via a private `ReadSelectedAgency` helper to detect transitions (Config can't be used for that — it's the post-update value).
- `Game::characterinterface` deleted (was only used by `GetSelectedAgency`).
- `Game::lastchannel` and `Game::chatlinesprinted` deleted. `lastchannel` moved onto the client `Lobby` class (next to the existing `channel`/`channelchanged` state) so `GoBack`, `TickLobbyBody`, and `ChatPanel` all read/write `world.lobby.lastchannel` directly. `chatlinesprinted` was always 0 in practice — replaced with `chatmessages.empty()` checks where it was branched on.
- All ad-hoc `ScreenContext` shims I'd added during stages A/B (`BuildLegacyLobbyInterface`, `TickLegacyLobbyBody`, `GetSelectedAgency`, `SetCharacterInterfaceId`, `NotifyAgencyChanged`) deleted. Stage A's `LobbyScreen::Build` now does `ctx.game.CreateLobbyInterface()` directly; Stage B's `CharacterPanel::Tick` calls `world.IsConnected()` directly; etc.

`ChatPanel` itself: at `clients/silencer/src/ui/screens/lobby/panels/chat_panel.{h,cpp}`. Owns the chat box (channel name overlay, scrollback textbox, presence textbox, chat input, scrollbar). Build attaches to the lobby parent and sets `parent->activeobject` + `ActiveChanged` — those calls moved out of `Game::CreateLobbyInterface`. Tick walks its own iface objects: drains `world.lobby.chatmessages` into the textbox, rebuilds the presence list when `presencechanged` flips, refreshes the channel-name overlay (and snapshots `world.lobby.lastchannel` on first change), and on `Game::minimized` flashes the OS taskbar/dock via `FLASHWINFO` (Win32) or `RequestUserAttention` (cocoa). `Game::CreateChatInterface` deleted; the chat-related arms in `Game::ProcessLobbyInterface` (TEXTBOX uid 9 + chat scrollback, OVERLAY uid 1 channel, TEXTINPUT chat-send) deleted. The TEXTINPUT arm kept the `enterpressed = false` reset for un-migrated form inputs (game-create, password). Anon-namespace uid enumerators prefixed `CHT_OVL_*` / `CHT_TB_*`.

`Game::lobbyinterface`, `Game::chatinterface`, and `Game::minimized` are public temp scaffolding (panels write/read them; deleted in stage H). `Game::CreateLobbyInterface` and `Game::TickLobbyBody` are public temp scaffolding too (called by LobbyScreen during the migration; deleted in stage H once their bodies are empty).

Verified: clean regular + unity Debug builds. E2E `00_ping`/`10_navigate`/`20_screenshot` pass.

Stage D landed: `GameSelectPanel` at `clients/silencer/src/ui/screens/lobby/panels/game_select_panel.{h,cpp}`. Owns the right-side server browser (Active Games selectbox + per-game info readouts: name, map, security/password, creator, level limits) and the Join Game / Create Game button handlers. Build is a verbatim move of `Game::CreateGameSelectInterface` widgets; Tick is a verbatim move of the SELECTBOX uid 10 refresh + per-row info rendering and the BUTTON uid 20 (Join) / uid 30 (Create Game) cases. `Game::CreateGameSelectInterface` deleted; the SELECTBOX uid 10 + BUTTON uid 20 + BUTTON uid 30 arms in `Game::ProcessLobbyInterface` deleted.

Architectural changes:
- `LobbyScreen` now owns `std::unique_ptr<GameSelectPanel> gameSelect` and exposes `ShowGameSelect(ctx)` / `ShowGameCreate(ctx)` swap helpers. Stage D: `ShowGameCreate` still calls into legacy `Game::CreateGameCreateInterface` until Stage E migrates it to a panel.
- `GameSelectPanel` holds a back-pointer to its owning `LobbyScreen` (passed in the constructor); the Create Game button calls `owner.ShowGameCreate(ctx)` rather than reaching into Game directly.
- `Game::GoBack` no longer calls `CreateGameSelectInterface` — when transitioning back from gamecreate or gamejoin/gametech, it dynamic-casts the top-of-stack screen to `LobbyScreen` and calls `ShowGameSelect(ctx)` to rebuild the panel cleanly.
- **Tick ordering flipped in `LobbyScreen::Tick`** — panels now run *before* `Game::TickLobbyBody()` so panel handlers see fresh `button->clicked` / `textinput->enterpressed` flags before the recursive ProcessLobbyInterface walk clears them. Stage C's chat-send path was likely silently broken by the legacy walk clearing `enterpressed` before `ChatPanel::Tick` could read it; the new ordering fixes this incidentally.
- `World::IsIdle()` added (mirrors the existing `IsConnected()` accessor) so the panel can gate Join on `state == IDLE` without reaching into World's private state enum.
- Temp scaffolding made public on `Game` for panel access (deleted in stage H along with the legacy lobby pump): `gameselectinterface`, `gamecreateinterface`, `currentinterface`, `currentlobbygameid`, `JoinGame`, `CreateGameCreateInterface`, `CreateModalDialog`, `CreatePasswordDialog`. The matching private declarations were removed (no duplicates).
- Anon-namespace uid enumerators in `game_select_panel.cpp` are file-prefixed (`GSEL_OVL_*` / `GSEL_BTN_*` / `GSEL_SEL_*`) so they don't collide under `SILENCER_UNITY_BUILD`.

Verified: clean regular + unity Debug builds (only the pre-existing C4804 warning at `IsLiveMultiplayer` and unrelated `sprintf` deprecations in `resources.cpp`). E2E `00_ping`/`10_navigate`/`20_screenshot` pass. Headless agent navigation MainMenu → Connect To Lobby → back to MainMenu works (LOBBYCONNECT screenshot taken). Interactive lobby smoke (login, browse games, Join, Create Game swap, GoBack) not exercised — same TEXTINPUT/login limitation as Stages A–C; verification rests on (a) builds, (b) E2E passing, (c) the panel impl being a near-mechanical move of the legacy widget construction + per-tick logic into the panel class.

Stages F + G landed in two commits: `GameJoinPanel` at `clients/silencer/src/ui/screens/lobby/panels/game_join_panel.{h,cpp}` (Ready / Change Team / Choose Tech buttons + the per-frame Ready label refresh that flips between "Waiting..." and "Ready" while the host waits on map downloads), then `GameTechPanel` at `clients/silencer/src/ui/screens/lobby/panels/game_tech_panel.{h,cpp}` (per-team-peer tech checkbox grid, slots-left counter, description-overlay click handler, Back To Teams button). LobbyScreen now owns all four right-side panels (`gameSelect`/`gameCreate`/`gameJoin`/`gameTech` as `unique_ptr` slots, swapped via `ShowGameSelect`/`ShowGameCreate`/`ShowGameJoin`/`ShowGameTech`); each helper goes through a shared `TearDownRightPanels` static that destroys the active panel iface + zeros the mirror id on Game + (for gameTech) resets `world.choosingtech` and re-enables team overlays. Architectural changes:
- `Game::TickLobbyBody`'s CONNECTED transition no longer hand-builds the gamejoin iface; it calls `LobbyScreen::ShowGameJoin(ctx)`. The two explicit teardown blocks (gameselectinterface, gamecreateinterface destruction) collapsed into the `TearDownRightPanels` call inside `ShowGameJoin`.
- `Game::ProcessLobbyInterface` shrunk further: BUTTON cases 25 (Ready), 26 (Change Team), 27 (Choose Tech), and 28 (Back To Teams) deleted; only the lobby Go Back (uid 10) and modal-OK (uid 50) arms remain. The leading `UpdateTechInterface()` call deleted (`GameTechPanel::Tick` now drives the per-frame tech-checkbox refresh).
- `Game::GoBack` simplified — the gamejoin/gametech branches no longer manually destroy ifaces or flip `world.choosingtech`. `TearDownRightPanels` (via `ShowGameSelect`) owns that. `Game::GoBack` keeps the genuine game-state work (`Disconnect`, `SwitchToLocalAuthorityMode`, channel rejoin, team-overlay destroy via `MarkDestroyObject`).
- `Game::CreateGameJoinInterface`, `Game::CreateGameTechInterface`, `Game::UpdateTechInterface` deleted. The per-frame Ready button "Waiting..." text update at line ~660 (case INGAME's INLOBBY branch) moved into `GameJoinPanel::Tick`.
- `World` gained three friend grants for the migration: `LobbyScreen` (RequestPeerList in ShowGameTech), `GameJoinPanel` (peerlist[localpeerid].ishost / AllPeersDownloadedMap / SendReady on the Ready button), and `GameTechPanel` (peerlist[team peers] for the checkbox grid + RequestPeerList recovery). All three are scoped to the migration; Stage H trims them as Game/World accessors are widened or the panels move to the `world.lobby.X` pattern.
- Anon-namespace uid enumerators in `game_join_panel.cpp` (`GJN_BTN_*`) and `game_tech_panel.cpp` (`GTECH_BTN_*` / `GTECH_OVL_*`) are file-prefixed for unity-build TU merging.

Stage E landed: `GameCreatePanel` at `clients/silencer/src/ui/screens/lobby/panels/game_create_panel.{h,cpp}`. Owns the create-game form (security toggle, level range, max players/teams, map list with `[DL]` server-only entries, game name, password) plus the per-frame map preview tracking, [DL] download badge clicks, and the Create-button kickoff (validate → upload map → CreateModalDialog "Uploading map..."). `Game::CreateGameCreateInterface` deleted; the SELECTBOX uid 4, BUTTON uid 35, BUTTON uid 40 arms in `Game::ProcessLobbyInterface` deleted. The remaining ProcessLobbyInterface SELECTBOX arm collapsed to a comment — both lobby selectboxes (uid 4 / uid 10) are now panel-owned.

Architectural changes:
- `MapDownloader &` added to `ScreenContext` per the original design — it's a subsystem ref like `AmbienceMixer`, not screen-specific scaffolding. `GameCreatePanel` reads it via `ctx.mapDownloader`.
- `LobbyScreen` now owns `std::unique_ptr<GameCreatePanel> gameCreate` alongside `gameSelect` (one of the two is alive at a time; future stages F/G will introduce GameJoin/GameTech panels here too). `ShowGameSelect` and `ShowGameCreate` each tear down whichever right-side panel is currently active before constructing a fresh one.
- `LobbyScreen::Tick` checks after `TickLobbyBody` whether `ctx.game.gameselectinterface` / `gamecreateinterface` were zeroed (the legacy CONNECTED transition tears them down on Join/Create handoff) and resets the matching panel objects so they don't tick stale ifaces next frame.
- `Game::GoBack`'s gamecreate-path simplified: it no longer manually destroys the iface and rebuilds — it just clears `gamesprocessed` and delegates to `LobbyScreen::ShowGameSelect`, which handles the panel teardown. Same shape as the Stage D delegation for the gamejoin/gametech path.
- `creategameclicked` and `mappreviewinterface` moved to public Game scaffolding so the panel can read/write them; the matching private declarations were removed (no duplicates). Removed in stage H.
- The legacy `Game::CreateGameCreateInterface` map-download lambda captured `this` to reach `mapDownloader.dlprogress`/`dlresult`; the panel's lambda captures the atomics by pointer instead so the worker thread keeps running cleanly even if the panel object is destroyed mid-flight (the thread observes the same atomics on `mapDownloader`, which outlives the panel).
- Anon-namespace uid enumerators in `game_create_panel.cpp` are file-prefixed (`GCRT_BTN_*` / `GCRT_INPUT_*` / `GCRT_SEL_*`).

Verified: clean regular + unity Debug builds. E2E `00_ping`/`10_navigate`/`20_screenshot` pass. Headless menu navigation (MainMenu ↔ Options) verified. Interactive create-game flow (form fill, map upload, CreateGame round-trip) not exercised — same TEXTINPUT login limitation; verification rests on (a) builds, (b) E2E passing, (c) the Build / Tick code being a near-mechanical move from the legacy site.

---

## Phase 0 — Baseline

Establish the green starting point before changing anything.

- [x] Build clean from scratch on this worktree. Pre-existing warning: `C4804` at `game.cpp:6232` (unsafe `bool` in comparison, in `IsLiveMultiplayer`/adjacent code untouched by the refactor).
- [x] Run the E2E suite. Baseline: `00_ping`, `10_navigate`, `20_screenshot` all pass.
- [ ] Interactive smoke test (deferred — agent session). The CLI-driven `10_navigate` scenario covers Main → Options → back, which is the only menu path automation can drive headlessly. Full interactive (lobby connect, in-game, exit) belongs to a human pass before merge.

After Phase 0: we have a known-good baseline to compare every subsequent step against.

---

## Phase 1 — Foundation (infrastructure-only, nothing uses it yet)

Each step adds infrastructure. The game runs identically after each — old code paths untouched.

- [x] **CMake: add new include paths.** Append `src/ui/components`, `src/ui/panels`, `src/ui/modals`, `src/ui/screens` (and lobby's two subdirs) to `target_include_directories`. The directories don't exist yet — that's fine, CMake doesn't error on absent dirs in this list. **Verify:** game still builds + runs identically.
- [x] **Move existing widgets into `ui/components/`.** Pure file move: every `.h/.cpp` currently in `src/ui/` (button, interface, overlay, scrollbar, selectbox, stats, teambillboard, textbox, textinput, toggle) goes into `src/ui/components/`. Bare-filename includes still resolve via the new include path added above. **Verify:** game still builds + runs identically; smoke-test any screen with buttons/textboxes (main menu).
- [x] **Create `Screen` base class** at `src/ui/screens/screen.h`. Pure header, no `.cpp`. Nothing #includes it yet. **Verify:** build still passes.
- [x] **Create `Panel` base class** at `src/ui/panels/panel.h`. Same — pure header, unused. **Verify:** build still passes.
- [x] **Create `Modal` base class** at `src/ui/modals/modal.h`. Same — pure header, unused. **Verify:** build still passes.
- [x] **Create `ScreenContext`** at `src/ui/screens/screen_context.h/.cpp`. Constructor takes references to all subsystems; methods are stubs that do nothing yet (or assert false). Nothing constructs it. **Verify:** build still passes.
- [x] **Add `screenStack` member to `Game`** + `PushScreen`/`PopScreen`/`ReplaceScreen` impls. Stack starts empty and stays empty. Add a one-line call in `Game::Tick` to tick the active screen if any (no-op while stack is empty). **Verify:** game runs identically — every menu still works via the old Create/Process methods.
- [x] **Wire `ScreenContext` construction in `Game`** so `Game` holds one (initialized with refs to its members). Still nothing uses it. **Verify:** build still passes.

After Phase 1: new infrastructure exists, fully unused. Reverting the entire phase would not change behavior. We're ready to migrate one screen.

---

## Phase 2 — Extract collaborators (independent of Phase 3; can run in parallel)

Each extraction is one commit. Methods and members move from `Game` to a new class; callers within `Game` update to go through the new class instance. The behavior is unchanged.

- [x] **Extract `MapDownloader`** at `src/game/map_downloader.h/.cpp`. Move `dlprogress`, `dlresult`, `dlitemname`, `dlthread`, `mapjoin*`, `mapUpload*`, `pendingCreate`, `servermaps`, `lastmapchunkrequest`, `mapexistchecked`, `selectedmap` plus the 7 methods (`ListFiles`, `FindMap`, `SaveMap`, `CalculateMapHash`, `StringFromHash`, `LoadMapData`, `ProcessMapDownload`). `Game` gets a `MapDownloader mapDownloader` member; everywhere `Game` referenced the old members, it now goes through `mapDownloader.X`. **Verify:** smoke-test the map download flow — join a game whose map you don't have locally; create a game with a local map (triggers upload).
- [x] **Extract `AmbienceMixer`** at `src/game/ambience_mixer.h/.cpp`. Move `bgchannel[3]`, `lastmusicplaytime`, `currentmusictrack`, `oldambiencelevel` plus the 6 methods (`CreateAmbienceChannels`, `UpdateAmbienceChannels`, `LoadRandomGameMusic`, `FadedIn`, `PlayMusic`, `GetGameChannelName`). **Verify:** smoke-test audio — main-menu music plays; entering a game switches tracks; ambience cross-fades when you change levels in-game.

> Both extractions landed together in one commit per user direction. The two share a `friend` grant in `World` so they retain Game's private-member reach. **Open follow-up:** suspected regression in the lobby's "Create Game" button — investigate before merging.

---

## Phase 3 — Migrate screens (one screen per commit, each independently green)

The migration pattern for each screen:

1. Create the new `<Name>Screen` class file(s).
2. In the corresponding `case <STATE>:` of `Game::Tick`, replace the inline body with: if `stateisnew`, `PushScreen(make_unique<NameScreen>())`; let the screen-stack tick run.
3. Delete `Game::Create<Name>Interface` and `Game::Process<Name>Interface` (and any `Update<Name>Interface`).
4. Delete the corresponding `Uint16 <name>interface` member from `game.h`.
5. Build + interactively smoke-test that screen — every button, every back-out path. Smoke-test the screens it transitions to and from.

**Order:** small screens first to validate the pattern; the big screen (Lobby) last. Modals before LobbyScreen because LobbyScreen depends on them.

- [x] **`MainMenuScreen`.** Smallest screen, validates the pattern. **Smoke:** all 4 buttons (Start, Lobby Connect, Options, Quit) — interactive pass pending.
- [x] **`LobbyConnectScreen`.** Validates a screen with non-trivial Tick logic (state machine that cycles through Lobby connection states). **Smoke:** connected to a local lobby (clean run + version-mismatch run); textbox state machine transitions correctly; version-reject path correctly hands off to UPDATING. Wrong-host connection-failure path not exercised yet.
- [ ] **`ModalDialog` (in `ui/modals/`).** Add as a `Modal` and route `Game::CreateModalDialog` calls to `ctx.ShowMessage(...)`. Old method stays for now (still called from un-migrated screens). **Smoke:** trigger any modal-dialog path that's already exercised — currently those are only fired from in-lobby code paths. Use `silencer-cli` to inject one if needed for testing.
- [ ] **`PasswordDialog` (in `ui/modals/`).** Same treatment as ModalDialog. **Smoke:** join a password-protected game.
- [ ] **`MapPreviewModal` (in `ui/screens/lobby/modals/`).** Same — added as a Modal but old `CreateMapPreview` stays callable until LobbyScreen migrates. **Smoke:** select a map in the game-create flow and confirm the preview shows.
- [x] **`OptionsScreen`.** Top-level options menu. **Smoke:** Options → all 3 sub-buttons → back to main menu — agent-driven via cli (Controls/Display/Audio enter their states; Cancel/Go Back return). Interactive human pass pending.
- [x] **`OptionsControlsScreen`** — biggest options screen, owns its keybind UI helpers (`LegacyView`/`WriteLegacy` are now private statics on the screen). `GetActionKeyDisplayName`, `GetKeyName`, `LoadActiveKeymap`, `CycleKeybindPreset`, `ForkActiveProfileIfBuiltin` stay on Game (consumed by tutorial overlays + ControlDispatch); the screen reaches them via `ScreenContext::CycleKeybindPreset`/`ForkActiveProfileIfBuiltin`/`LoadActiveKeymap`/`KeyName`. **Smoke:** screen renders identically (Preset, 5 binding rows, scrollbar, Save/Cancel) — agent screenshot matches. Interactive rebind/cycle/save round-trip pending.
- [x] **`OptionsDisplayScreen`.** Wires fullscreen + smooth-scaling toggles via two new `ScreenContext` actions (`SetFullscreen`, `SetScaleFilter`) so the screen never touches `SDL_Window` or `RenderDevice` directly. **Smoke:** screen renders identically; navigation works. Interactive fullscreen toggle pending.
- [x] **`OptionsAudioScreen`.** Music toggle drives `Audio::GetInstance()` directly (singleton, no Game reach needed). **Smoke:** screen renders identically; navigation works.
- [x] **`UpdateScreen`.** **Smoke:** triggered via local lobby `-version "99999" -update-manifest manifest.json` with manifest pointing at `https://example.invalid/...`; PROMPTING → DOWNLOADING → FAILED transitions render correctly with status text + button visibility (Update/Cancel → Cancel only → Retry+Cancel). Stage-2 launch and the >=3-retry Download path not exercised (would need a real signed update zip).
- [ ] **`GameSummaryScreen`.** **Smoke:** finish a game (or use replay/test mode); confirm summary stats render and "back to lobby" works.

### LobbyScreen — special case (multi-step, each step green)

LobbyScreen has 6 panels. Migrate it in stages so the lobby keeps working throughout.

- [x] **Stage A: land `LobbyScreen` with adapter calls.** `LobbyScreen::Build` calls into the existing `Game::CreateCharacterInterface()`, `Game::CreateChatInterface()`, etc. as before. `LobbyScreen::Tick` calls into the existing `Game::ProcessLobbyInterface()`. The 731-line method stays intact for now — we've just moved the entry point. **Smoke:** full lobby flow — connect, character select, chat, browse games, create game, join game, leave lobby. This is the biggest single smoke-test of the refactor.
- [x] **Stage B: migrate `CharacterPanel`.** Replace the `Game::CreateCharacterInterface` call inside `LobbyScreen::Build` with `character.Build(ctx, parent)` using the new `CharacterPanel`. Move the panel's process logic out of `Game::ProcessLobbyInterface` into `CharacterPanel::Tick`. Delete `Game::CreateCharacterInterface`. **Smoke:** character select — pick each agency, confirm display updates.
- [x] **Stage C: migrate `ChatPanel`.** **Smoke:** type chat messages, confirm they appear; receive messages from another client.
- [x] **Stage D: migrate `GameSelectPanel`.** **Smoke:** browse server list, refresh, see games appear/disappear — interactive pass pending (TEXTINPUT login limitation).
- [x] **Stage E: migrate `GameCreatePanel`.** **Smoke:** open create-game form, enter game name, select map, set parameters, click create; confirm map upload triggers; confirm you land in the lobby of the created game — interactive pass pending (TEXTINPUT login limitation).
- [x] **Stage F: migrate `GameJoinPanel`.** Verified: clean regular + unity Debug builds; E2E `00_ping`/`10_navigate`/`20_screenshot` pass. Interactive lobby smoke (login, ready/change-team/choose-tech roundtrip) not exercised — TEXTINPUT login limitation.
- [x] **Stage G: migrate `GameTechPanel`.** Verified: clean regular + unity Debug builds; E2E suite passes. Interactive smoke (per-peer checkbox click, description scroll, Back To Teams) not exercised — TEXTINPUT login limitation.
- [ ] **Stage H: cleanup.** Delete what's left of `Game::ProcessLobbyInterface`, `Game::CreateLobbyInterface`, `Game::UpdateLobbyMapName`, and the `lobbyinterface`/`chatinterface`/etc. `Uint16` members. **Smoke:** full lobby flow once more.

After Phase 3: every menu surface is a Screen/Panel/Modal class. `Game` has no `Create*Interface`/`Process*Interface` methods left.

---

## Phase 4 — Mechanical splits (non-UI, file moves only)

Pure file moves. Each commit moves a group of `Game::` member implementations into a new `.cpp` while the declarations stay in `game.h`. Behavior unchanged.

- [ ] **Move SDL event handling** into `src/game/events.cpp` (`HandleSDLEvents`, `OnScancodeDown/Up`, `UpdateInputState`, `OpenFirstGamepad`, `PollGamepadState`). **Smoke:** keyboard navigation on every screen; gamepad if available.
- [ ] **Move in-game lifecycle** into `src/game/ingame.cpp` (`LoadMap`, `UnloadGame`, `JoinGame`, `GiveDefaultItems`, `ShowDeployMessage`, `CheckForQuit`/`EndOfGame`/`ConnectionLost`, `ProcessInGameInterfaces`, `ShowTeamOverlays`). **Smoke:** start a game, play through to end-of-mission.
- [ ] **Move headless-control glue** into `src/game/headless.cpp` (`DrainControlQueue`, `PostFrameReplies`). **Smoke:** run the full E2E suite (every test exercises this path).
- [ ] **Split `Game::Tick`'s gameplay-state cases.** This is six commits, one per gameplay state — the switch dispatcher stays in `game.cpp` but each `case <STATE>:` body moves into a per-state method (e.g., `Game::TickInGame()`) defined in `src/game/tick/tick_<state>.cpp`.
  - [ ] `tick_misc.cpp` — `FADEOUT` (smallest first to validate). **Smoke:** transition between any two states.
  - [ ] `tick_replay.cpp` — `REPLAYGAME`. **Smoke:** play a replay file.
  - [ ] `tick_hostjoin.cpp` — `HOSTGAME`, `JOINGAME`, `TESTGAME`. **Smoke:** host a game; join a game; launch test mode.
  - [ ] `tick_singleplayer.cpp` — `SINGLEPLAYERGAME` (391 lines). **Smoke:** start single-player, complete a level.
  - [ ] `tick_ingame.cpp` — `INGAME` (228 lines). **Smoke:** play multiplayer end-to-end.

---

## Phase 5 — Cleanup

- [ ] **Delete dead members from `Game`.** Anything no longer referenced (old interface IDs, helpers wholly absorbed by panels). **Verify:** build passes, full E2E suite passes.
- [ ] **Verify size targets.** `game.cpp` ≈ 250 lines, `game.h` ≈ 170 lines, no UI file >300 lines except `lobby_screen.cpp` (~300). If anything is way over, decide whether to split further or leave it.
- [ ] **Update `clients/silencer/CLAUDE.md`.** The "Where to look" section currently points at `src/game.cpp`; update it to reflect the new layout.
- [ ] **Final full smoke test.** Main menu → options (each sub-screen) → lobby connect → lobby → character → chat → game create → join game → in-game → end-of-mission summary → back to lobby → quit. Every modal that triggers along the way (bad password, download failed, disconnected, etc.).
- [ ] **Final E2E suite run.** Compare to Phase 0 baseline — should be identical pass/fail status.

---

## Decisions log

Choices made during brainstorming, anchored here so future sessions don't have to relitigate.

- **Three-tier UI model.** `Screen` (top-level, stack-managed) / `Panel` (sub-`Interface` owned by a Screen as a member) / `Modal` (overlay-on-top with callback). Chosen over flat "everything is a Screen" because it matches how the existing UI actually composes (lobby has co-visible sub-panels via `iface->AddObject(...)`).
- **Co-location.** Things used by exactly one screen live inside that screen's folder (`ui/screens/<screen>/panels/`, `ui/screens/<screen>/modals/`). Things used across screens live at the top of `ui/`. Same rule for panels and modals.
- **`ScreenContext` is curated.** Only subsystem refs (World, Renderer, Lobby, Config, KeyMap, Updater, AmbienceMixer, MapDownloader) and state-machine actions (GoToState, GoBack, RequestQuit, PushScreen, PopScreen, ReplaceScreen, ShowModal). No session-specific accessors (`LocalUsername`, `SelectedAgency`, `IsLiveMultiplayer`) — those data points get re-homed to where they semantically belong (Config, character-screen state, World/inline).
- **Existing `ui/*.{h,cpp}` move into `ui/components/`.** Bare-filename includes (`#include "button.h"`) keep working because the new subfolder is added to CMake's include path. No caller updates needed for the move.
- **`ui/panels/`** exists but is empty initially (all 6 panels are lobby-owned). Holds `panel.h` only.
- **Modals split:** `ModalDialog` and `PasswordDialog` go in `ui/modals/` (generic); `MapPreviewModal` goes in `ui/screens/lobby/modals/` (lobby-only).
- **Gameplay-state Tick bodies stay on `Game`.** `INGAME`, `SINGLEPLAYERGAME`, `HOSTGAME`, `JOINGAME`, `TESTGAME`, `REPLAYGAME` aren't UI surfaces — they're the actual game running. They're mechanically split into `src/game/tick/tick_*.cpp`, not converted to Screens.
- **Green at every commit.** Old code paths are not deleted until each consumer migrates. Migration uses adapter pattern (e.g., LobbyScreen Stage A calls into existing `Game::Create*` helpers). Last commit in each migration sequence is "delete the old."
- **`Modal` is a `Screen` subclass with `IsOverlay()` virtual on `Screen`.** Renderer/screen-stack checks `IsOverlay()` to decide whether to draw the underneath. Putting the virtual on `Screen` (not `Modal`) means the stack never has to type-check.
- **`ScreenContext` dispatches actions through a private `Game&` ref.** This is an implementation choice; the contract is the public surface (named subsystem refs + named action methods). The design doc was originally over-prescriptive ("no `Game&`") — amended on 2026-05-09 to leave the dispatch mechanism free.

---

## Open questions

- **Panel ownership pattern.** Value member (`CharacterPanel character;`) vs `unique_ptr<Panel>` — value works for always-alive panels, `unique_ptr` for swappable ones (GameSelect ↔ GameCreate). Decide per-panel during Phase 3.

---

## Notes for next session

When picking this up:
1. Read the design doc end-to-end before touching code.
2. Run Phase 0 first — without a known-good baseline, you can't tell if a regression you hit was caused by the refactor or was pre-existing.
3. Phase 1 is sequential within itself. Phases 2 and 3 can interleave — extract `MapDownloader` whenever it's convenient, doesn't block screen migrations.
4. **One commit per checkbox.** Resist batching. The whole point is that any single commit can be reverted without breaking the game.
5. After each commit, run the verification protocol at the top of this doc. Don't tick the box if any of the four steps fails.

---

## Handoff prompt for next agent

You're picking up the `refactor/game-cpp` branch mid-Phase-3. The user is largely AFK — take the lead, validate via the headless CLI, and don't use Game or ScreenContext as a grab bag for screen-specific logic. Memory note in `~/.claude/projects/-Users-hv-repos-Silencer/memory/`: skip the superpowers skill suite (no brainstorming/writing-plans machinery); just edit `docs/plans/` or this progress doc directly when you need to.

### What's already landed

Stages A–G of Phase 3 are committed. The lobby right-side surface is fully panel-driven:
- `ui/screens/lobby/lobby_screen.{h,cpp}` — owns panels by composition; exposes `ShowGameSelect(ctx)` / `ShowGameCreate(ctx)` / `ShowGameJoin(ctx)` / `ShowGameTech(ctx)` swap helpers, all routed through a shared `TearDownRightPanels` static at the top of the .cpp.
- `panels/character_panel.{h,cpp}` (value member, always-alive)
- `panels/chat_panel.{h,cpp}` (value member, always-alive)
- `panels/game_select_panel.{h,cpp}` / `game_create_panel.{h,cpp}` / `game_join_panel.{h,cpp}` / `game_tech_panel.{h,cpp}` — four `unique_ptr` slots, mutually exclusive (only one right-side panel alive at a time).

Key invariants:
- `ScreenContext` carries `World/Renderer/Lobby/KeyMap/Updater/AmbienceMixer/MapDownloader` refs + state-machine actions; it is **not** a junk drawer of per-screen accessors. Panels reach Game directly via `ctx.game.X` for temp scaffolding (`Game` members made public during the migration, named *"Removed in stage H"* in `game.h`).
- `LobbyScreen::Tick` order is **panels first, then `Game::TickLobbyBody()`**, so panel handlers see fresh `button->clicked` / `textinput->enterpressed` flags before the recursive `ProcessLobbyInterface` walk clears them. **Don't flip this order back.**
- `Game::GoBack` no longer manually destroys panel ifaces or flips `world.choosingtech` — it delegates to `LobbyScreen::ShowGameSelect`, which goes through `TearDownRightPanels`. `GoBack` keeps the genuine game-state work (`Disconnect`, `SwitchToLocalAuthorityMode`, channel rejoin, team-overlay destroy via `MarkDestroyObject`).
- Per-frame Ready-button label refresh ("Waiting..." vs "Ready") moved out of `case INGAME`'s INLOBBY branch in `Game::Tick` and into `GameJoinPanel::Tick`. **Do not** re-introduce the legacy callsite.
- `World` has migration-scoped friend grants for `LobbyScreen` (RequestPeerList in ShowGameTech), `GameJoinPanel` (peerlist[localpeerid] / SendReady / AllPeersDownloadedMap), and `GameTechPanel` (peerlist[team peers] / RequestPeerList recovery). Trimmed when their callers move to public `World` accessors.

### Next checkboxes (in order)

1. **Modals work — `ModalDialog`, `PasswordDialog`, `MapPreviewModal`.** Top of the queue because Stage H cleanup depends on it (the BUTTON uid 50 modal-OK arm is the last non-trivial thing in `Game::ProcessLobbyInterface`, and `Game::CreateModalDialog` / `CreatePasswordDialog` / `CreateMapPreview` can't be deleted until consumers stop calling them).
   - `ModalDialog` and `PasswordDialog` go in `src/ui/modals/` (generic — multi-screen consumers).
   - `MapPreviewModal` goes in `src/ui/screens/lobby/modals/` (lobby-only).
   - Wire up `ScreenContext::ShowMessage(const char *, callback)` (currently asserts) — that's the migration target for `CreateModalDialog`. Implementation pushes a Modal onto the screen stack and routes the OK callback back. `Modal` is already declared as a `Screen` subclass with `IsOverlay()` — see `src/ui/modals/modal.h`.
   - Lookup sites: `Game::CreateModalDialog` is called from at least: `JoinGame` (level-too-low/high — those are inside `GameSelectPanel::Tick` already, easy migration), `TickLobbyBody` (creategame failure, "Could not upload map", "Disconnected from game", "Unable to join game"), `Game::CheckForConnectionLost`. `CreatePasswordDialog` is called once from `GameSelectPanel::Tick`. `CreateMapPreview` is called from `GameCreatePanel::Tick` (map list selection).
   - The case-50 arm in `ProcessLobbyInterface` handles modal dismissal (`DestroyModalDialog`, password-input read into `JoinGame`, GoBack on game-disconnect modal). After modals migrate, this arm goes away.

2. **`GameSummaryScreen`.** Mechanical migration of `Game::CreateGameSummaryInterface` + `Game::ProcessGameSummaryInterface` + `Game::UpdateGameSummaryInterface` onto a new `Screen`. State enum already has `GAMESUMMARY`. Pattern matches `MainMenuScreen` / `OptionsScreen`. Watch for `Game::ProcessGameSummaryInterface` reaching into upgrade flow that calls `Game::SaveStats` / `Game::LobbyStatUpgrade` — keep those on Game (genuine progression state, not screen state).

3. **Stage H — final cleanup.** Search-and-delete pass once Modals + GameSummary are in:
   - **Re-privatize** `Game` members that have no panel reader: `creategameclicked` (only deferred-create writes it). Keep public: `lobbyinterface`, `chatinterface`, `gameXXXinterface`, `currentinterface`, `currentlobbygameid`, `mappreviewinterface`, `minimized` — all still read by panels.
   - **Delete** `Game::CreateModalDialog`, `Game::CreatePasswordDialog`, `Game::CreateMapPreview`, `Game::DestroyModalDialog`, `Game::CreateGameSummaryInterface`, `Game::ProcessGameSummaryInterface`, `Game::UpdateGameSummaryInterface`, `Game::AddSummaryLine`. All become panel/screen-private.
   - **Migrate `Game::UpdateLobbyMapName`.** Only called from `TickLobbyBody`'s CONNECTED transition; mutates the uid-8 overlay on the lobby iface. Natural home: a thin "lobby chrome" helper, or fold into `LobbyScreen` (it owns the lobby iface). `LobbyScreen::SetMapNameOverlay(const char *)` is fine.
   - **Migrate the deferred-create state machine** in `Game::TickLobbyBody` (lines ~2126–2167 in current source — polls `mapDownloader.mapUploadState` + `world.lobby.creategamestatus`). Most natural home is `GameCreatePanel::Tick` (the panel kicked it off; it owns the modal lifecycle). The `world.Connect(...)` + `joininggame = true` handoff on success is genuinely Game-state, but it can stay in `TickLobbyBody` while the polling moves out — split between panel (poll → call `ctx.game.OnMapUploadComplete()` or similar) and Game (Connect + flip joininggame).
   - **Delete `Game::ProcessLobbyInterface` entirely.** After modals migrate, only the `case 10:` (Go Back — already calls `Game::GoBack`) and case 50 (modal OK) arms remain; uid 10 is the lobby chrome's own button (lives on the lobby iface, not a panel) — fold into a tiny `LobbyScreen::HandleGoBack` walk or push it down into the lobby iface's existing `buttonescape` plumbing. The `if(mappreviewinterface && !gamecreateinterface)` guard at the top of `ProcessLobbyInterface` belongs in `GameCreatePanel::Destroy` (when the panel goes away, kill the preview).
   - **Delete `Game::TickLobbyBody`** once the deferred-create move lands. The remaining ambience-music body is one line; inline at the LobbyScreen call site.
   - **Delete `Game::CreateLobbyInterface`** by moving its 5-widget body into `LobbyScreen::Build` directly (it's just background + title + version + map-name overlay + Go Back button — no abstraction value).
   - **Trim the `World` friend grants** for `LobbyScreen` / `GameJoinPanel` / `GameTechPanel`. The narrow accessors that replace them: `World::GetLocalPeer()` (returns `peerlist[localpeerid]`), `World::IsLocalHost()` (one-liner), `World::RequestPeerList()` made public (it already is *intent*, just not access). For `GameTechPanel`'s `peerlist[team->peers[i]]` — add `Team::GetPeer(int slot, World &)` or expose `World::GetPeerById(Uint8)`.

4. **Phase 4 mechanical splits** (events.cpp, ingame.cpp, headless.cpp, tick_*.cpp). Pure file moves; save for after Phase 3 is done. The plan in the spec is solid — just follow it.

5. **Phase 5 cleanup + size-target check.**

### Verification

Per stage:
- Regular Debug build: `cmake -B clients/silencer/build && cmake --build clients/silencer/build`. Warning-clean except the pre-existing C4804 at `IsLiveMultiplayer` and unrelated sprintf-deprecated noise in `resources.cpp`/`renderer.cpp`/etc.
- Unity Debug build: `cmake -B clients/silencer/build-unity -DSILENCER_UNITY_BUILD=ON && cmake --build clients/silencer/build-unity`. **Re-run `cmake -B build-unity` whenever you add a new .cpp file** so `GLOB_RECURSE` picks it up. (Same applies to the regular `build/`.)
- E2E: `bash tests/cli-agent/e2e/00_ping.sh && bash tests/cli-agent/e2e/10_navigate.sh && bash tests/cli-agent/e2e/20_screenshot.sh`.
- Headless menu nav smoke: launch with `start_silencer "$PORT"` (helpers in `tests/cli-agent/e2e/lib.sh`), drive with `bun clients/cli/index.ts --port $PORT click --label X`. **The CLI cannot drive `TEXTINPUT` widgets**, so the actual lobby login flow can't be exercised headlessly — verification rests on builds, E2E passing, and the per-stage moves being mechanical (no logic reshape).
- For modal/lobby work specifically: you can sometimes reach in-lobby state via `silencer-cli`'s lobby-fill helpers (see `using-silencer-cli` skill in `~/.claude/skills/`) — fakes authenticated players, side-steps TEXTINPUT.

### Open issues to be aware of

- **Phase-2 "Create Game" regression** (still unresolved): "Create Game" button kicked back to LobbyConnect on at least one user's machine. Not reproduced or root-caused in stages C–G. The deferred-create state machine still lives on `TickLobbyBody`. Worth re-checking once Modals work lands and you can drive a real CreateGame flow headlessly via lobby-fill.
- **`Game::GoBack` after `LOBBYCONNECT` → `MAINMENU` timing race**: multi-frame gap between `wait_for_state MAINMENU` resolving and `case MAINMENU` running `PushScreen`. CLI scripts hitting that path need `wait_frames --n 10` or similar. Pre-existing — not introduced by this refactor.
- **LobbyScreen::Tick auto-reset block** (`if(panel && ctx.game.gameXXXinterface == 0) panel.reset()`) is defensive — currently no Game-side path zeros these mirrors at runtime now that `TearDownRightPanels` owns it. Safe to delete in Stage H, but harmless if left as belt-and-braces.

### Conventions to keep

- **Anon-namespace uid enumerators** in panel `.cpp` files MUST be file-prefixed (e.g. `GCRT_BTN_*`, `GSEL_OVL_*`, `GJN_BTN_*`, `GTECH_OVL_*`) to dodge unity-build collisions when sibling panels merge into one TU.
- **Panel constructors** take `LobbyScreen & owner` so they can call `owner.ShowXxx(ctx)` for swaps. Pattern matches all four right-side panels today.
- **Public `Game` scaffolding** gets a comment like *"Removed in stage H."* or names the consumer panel — keep that explicit so cleanup is a search-and-delete pass.
- **`World` friend grants** for panels are *temporary scaffolding*, not architecture. When you find yourself adding one for a new panel, prefer the narrow public accessor pattern (`World::IsConnected()`, `World::IsIdle()` are good examples) — only fall back to friending if the call site is migration-only and will move into a more cohesive home in Stage H.
- **Stage commits should be self-contained.** Build + E2E green at every commit. If a "stage" splits into multiple commits to keep this property, do that — the rule beats the boundary.
