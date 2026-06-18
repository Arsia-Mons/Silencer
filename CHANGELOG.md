# Changelog

All notable changes to Silencer are documented here.

## [Unreleased]

## [v00060] — 2026-06-17

### Game client

- Migrated the entire client UI from the Clay layout engine to the cppx
  retained-mode UI engine (#267). Every screen was rebuilt on the new engine:
  the main menu, lobby connect, the lobby, character create, options, and the
  in-game HUD.

## [v00059] — 2026-06-11

### Game client

#### Black Rose poison — guards and civilians (#135, #273)

- Black Rose agents can now poison guards and civilians via the `INV_POISON` item (proximity use), not just enemy players.
- Poisoned NPCs take damage over time and die silently — no alarm triggered, no knockback, fitting the stealth identity.
- Guard poison death credits the poisoner's `guardskilled` stat and drops ammo; civilian poison death credits `civilianskilled`.
- `ACTOR_DAMAGED` and `ACTOR_KILLED` triggers fire correctly on poison ticks and deaths.
- GAS params `poisonTickCycle` and `poisonDamagePerTick` added to `EnemyDef` for per-NPC-type tuning.

#### Stability (#277)

- Fixed a crash (`SIGSEGV`) in `Bipedal::FollowGround` and `DistanceToEnd` caused by `std::map::operator[]` inserting null entries for unknown platform IDs. Stale platform IDs now cause the actor to fall rather than crash.

## [v00058] — 2026-05-31

### Game client

#### Lobby connect and UI (#264)

- The lobby connect primary action now reads `Login/Create`, with auto-width chrome buttons and a patched legacy backdrop so the wider label no longer clips or overlaps `Cancel`.

#### Physics materials and footsteps (#229)

- Added data-driven physics materials for platforms, including friction/speed multipliers and material-driven footstep cue resolution.
- Player and NPC footsteps now use frame-event tags with per-material actor overrides, and map data can persist platform material assignments.

### Admin web and API

- Added the Physics Materials editor and API route for editing per-material movement and sound cue data (#229).
- Actor footstep override controls now use searchable cue pickers, and expired admin sessions redirect back to login instead of leaving pages in a broken offline state (#229).
- Admin API can drive a Discord live-stats presence from live player/game counts when `DISCORD_TOKEN` is configured (#262).

### Lobby and compatibility

- Added the WON.net replacement DLL client plus lobby compatibility endpoints for legacy login, account creation, and profile requests (#260).
- Fixed the WON DLL Windows socket include path so its x86 Windows build uses the expected Winsock declarations.

### Docs

- Added extracted original beta reference files and game-reference docs for the WON compatibility work (#260).

## [v00057] — 2026-05-25

### Game client

- macOS startup now removes a stale `Silencer.app.old` sibling left by a successful auto-update once the new app has relaunched (#253).

## [v00056] — 2026-05-25

### Game client

#### Character names and migrated renames (#242, #246)

- Lobby and in-game player displays now use the selected character name instead of the account name.
- Migrated agency-named characters can be renamed once, with lobby protocol support and Select Agent UI affordances that label the action as `Rename Once` and the modal as `ONE-TIME RENAME`.
- The lobby character panel now shows XP directly above the `Agents` button so character progress stays visible in the main panel layout.

## [v00055] — 2026-05-25

### Game client

#### macOS updater (#247)

- macOS release builds now package `updater-stage-2` as a nested, signed helper at `Silencer.app/Contents/Helpers/updater-stage-2`, so Gatekeeper validates it as part of the app bundle instead of rejecting a copied runtime executable as damaged.
- The updater no longer stages a temporary `.app` around `updater-stage-2`; it resolves and launches the helper from the installed bundle, then keeps the actual app replacement flow in the signed helper.
- Release signing now signs and verifies the helper inside-out, and the macOS bundle verifier enforces that the helper exists and only links against system libraries.

#### Release note

- Existing macOS installs that already lost or quarantined their stage-2 helper may need one manual DMG install. After that, future auto-updates use the signed nested helper.

### Infrastructure

- Removed the disposable AWS smoke-test environment: automatic staging deploys, staging SSM seed entries, the retired plan documentation, and the staging Terraform module are gone after the approved destroy and cleanup (#235, #240).

## [v00054] — 2026-05-25

### Game client

#### Character creation flow (#123, #145)

- Agency selection moved from account-level slots to character-level identity. The lobby server now stores characters with ID, name, agency, stats, and selected character; existing populated agency slots migrate into characters on load.
- Lobby protocol, C++ SDK, and TS SDK now support character list, create, and select messages. Auth sends the character list immediately, and protocol-version guards cover character create/select traffic.
- New three-screen character flow: Select Agent, Silencer Alias, and Select Agency. The lobby character panel now reflects the selected character name, agency icon, and stats.
- Replays preserve the selected character, and local lobby/client launch helpers plus E2E coverage were added for the new flow.

#### Sound Cue system (#223)

- Visual node-based cue editor in Sound Studio (admin web): Random, Sequence, Modulator, Delay, and Concatenate nodes with wiring, volume/pitch controls, and live preview.
- Download All Cues button — exports all cue JSON files as a zip via fflate.
- All game sounds migrated from raw soundbank arrays to cue JSON files: weapon/projectile fire and hit sounds, NPC alert/hurt/death/melee sounds, player action sounds (roll, reload, pickup, repair, powerup, disguise, jack in/out, undeploy, jetpack, weapon charged, security pass, breath), impact sounds, footsteps, ambient object sounds (door, station, grenade), and UI sounds.
- Random nodes replace all manual `rand()` array picks in C++; GAS fields consolidated (e.g. `soundAlert1-5` → `soundAlert`).
- `world_messaging` and `world_network` support `cue:` prefixed sound names sent from the server.
- Audio stacking fix: `Audio::PlayUI()` uses a dedicated interrupt channel (127) with a 30 ms per-chunk cooldown to prevent UI hover sounds from stacking and clipping. `Audio::Play()` gains an optional `maxInstances` cap that interrupts the oldest playing instance of a chunk instead of spawning unbounded copies.
- Map select list now plays the UI sound once per newly hovered row.

#### World and game subsystem extraction (#210, #211, #214, #216)

- `Game` and `World` implementation files were split into per-concern source files as groundwork for continued client modularization.
- `World` decomposed into 5 owned subsystem classes, each with its own `.h` + `.cpp` in `src/world/`: `WorldObjectRegistry`, `WorldMessaging`, `WorldNetwork`, `WorldPeerRegistry`, `WorldReplication`.
- All callers updated to go through the subsystem public API; reference shims removed.
- `World` is now a thin coordinator. Public API and runtime behaviour unchanged.

#### macOS updater (#184)

- macOS update zip extraction now uses `/usr/bin/ditto -x -k`, preserving symlinks, resource forks, xattrs, and the notarized app bundle's code-signature seal. This prevents auto-updated apps from relaunching as "damaged" after the bundle is reconstructed.
- Quarantine stripping remains as defense in depth on the freshly installed bundle before relaunch, with improved diagnostics.

#### Bug fixes

- Smoothed lobby panel chrome lines and glow so the Clay UI chrome matches the baked legacy green border treatment (#177, #191).
- Restored the Mission Summary layout and simplified the layout code (#232).
- Restored the legacy in-game chat overlay rendering path, including compositor payload support for the overlay (#234).
- Fixed chat input text centering with shared stable text measurement and coverage for the lobby chat field (#233).
- Fixed on-screen keyboard not appearing on ROG Ally / handheld Windows (#220) — `SDL_HINT_ENABLE_SCREEN_KEYBOARD` was not set; SDL3 requires it (before `SDL_Init`) to show the OS keyboard when `SDL_StartTextInput` is active.
- Fixed unity build collision on `kLegacyRenderWidth/Height` (#220) — constants are now defined once in `game_renderer.h` as `inline constexpr` instead of per-file.

### Admin web and API

#### Live game monitor (#23, #209)

- Lobby and dedicated server heartbeat health now flows through the admin API websocket pipeline.
- Admin dashboard game cards show live heartbeat status and expand for more detail.

#### Editor fixes

- Behavior Tree editor no longer crashes when a non-BT folder is opened; folder loading skips files without a `nodes` field and normalizes missing BT metadata.

### Docs

- Added reference docs for the Sound Cue system, Sound Studio, Map Designer, Behavior Tree system/editor, Actor Editor, GAS/GAS editor, Sprite Manager, and Weapon Editor.
- Added `CLAUDE.md` / `AGENTS.md` guidance for client subdirectories and shared GAS assets.

## [v00053] — 2026-05-18

### Game client

#### On-screen keyboard for handheld Windows (#197)

- `SDL_StartTextInput` / `SDL_StopTextInput` now called on text field focus/blur in `Game::RenderClientUiFrame`, triggering the OS on-screen keyboard on handheld Windows devices (ROG Ally, Steam Deck, etc.).
- `UiInteractionRegistry::HasTextInputFocus()` added to track which field holds focus.

#### Bug fixes

- Fixed null-pointer dereference on `GameStateObject` after `CreateObject` — was causing a SIGSEGV crash on ALLY10c.
- Fixed `build.sh` crash when `ninja` is not installed (empty `gen` array with `set -u`).

## [v00052] — 2026-05-17

### Game client

#### Magistrate boss NPC (#164)

- **New NPC — Magistrate** — fully BT-driven boss enemy. Patrols on a platform using the Civilian wall-turn pattern (immediate `mirrored` flip at edge, always returns Running — no pause or stuck frames). Activates after a configurable delay, broadcasts a spawn sound on appearance, and broadcasts a death sound + spawns death guards when killed.
- **Spawn / death sounds — all clients** — spawn sound plays on every client via draw-transition detection in `Tick()` (NEW state lasts 1 tick, too brief for snapshot delivery). Death sound plays via health-drop detection (`health > 0 → 0`). Both bypass distance attenuation with `EmitGlobalSound`.
- **DeathFade duration** — extended from 10 → 120 ticks so the DYING state is reliably snapshotted by clients before the Magistrate is removed.
- **Death guard spawning** — `SpawnDeathGuards` BT leaf is idempotent; safe to run multiple times during the extended DYING window.
- **Sound keys fixed** — BT JSON `sound` props now use `.wav` extension (`mgbegin.wav`, `mgend.wav`) matching the soundbank key format.

#### Behavior tree — Phase 2 & 3 (#164)

- **`EmitSound` leaf — global broadcast** — new `global` prop; when true, sound plays at full volume on every client regardless of distance.
- **`Walk` BT leaf** — Magistrate Walk leaf checks `DistanceToEnd` and flips `mirrored` inline each tick (Civilian pattern), always returning Running.

### Admin web

#### BT editor

- **Magistrate action dropdown** — all 8 Magistrate-specific leaf actions (`Patrol`, `Walk`, `TurnAround`, `Stand`, `CheckActivation`, `EmitSpawnSound`, `EmitDeathSound`, `SpawnDeathGuards`, `DeathFade`) now appear in the ACTION selector with descriptions.
- **`EmitSpawnSound` / `EmitDeathSound` inspector** — dedicated sound file dropdown prevents free-text key mistakes.
- **`EmitSound` inspector** — global / spatial toggle added alongside the sound dropdown.

#### Map designer

- **Magistrate spawn conditions** — right-click context menu now exposes **Activation delay (seconds)** and **Secrets to activate** fields. Values are packed into unused high bits of `actor.type` (bits 8–23 = activationTicks, bits 24–31 = secretTriggerN); `0` in either field falls back to the GAS default.

### Game client — bug fixes

- **`map.cpp` case 72** — reads and applies per-actor `activationTicks` and `secretTriggerN` from the new packed encoding in `actor.type`.

## [v00051] — 2026-05-13

### Game client

#### AI — Complete behavior tree migration (#136, issue #15)

- **Guard — 100% BT-only** — removed the ~560-line `if(!bt_)` legacy state machine fallback; the guard BT was already running (44 leaf actions covering all states) and the fallback was dead code. Guard now crashes fast if the BT file is missing rather than silently falling back.
- **Guard — patrol ledge turnaround fix** — `minWallDistance=35 > guard speed (~5 px/tick)` caused the turnaround check to fire every tick while inside the edge zone, making the guard oscillate and appear stuck. Fixed with an edge-triggered flip: `mirrored` is flipped only on the first tick entering the zone (tracked via `ctx.state`), not on every tick.
- **Civilian — `Uint8 state_i` overflow bug** — `state_i = -1` (a `Uint8`) produced 255; the RUNNING duration check (`255 >= 150`) passed immediately every tick, so RUNNING exited before any movement. Civilian now returns `Running` immediately on WALKING→RUNNING transition; `Tick`'s `state_i++` wraps 255→0 cleanly.
- **Civilian — threat detection** — switched from projectile `TestAABB` (unreliable: projectiles have `requiresauthority=true` and don't exist on the client) to `Player::IsShooting()` (`weaponfirecool > 0`, set for ~7 ticks per shot).
- **Civilian — flee direction locking** — flee direction is locked at trigger time for the full RUNNING duration; `DistanceToEnd` wall-bounce is the sole redirect, preventing direction oscillation against walls.
- **Robot** — confirmed 100% BT-only; no changes needed.

## [v00050] — 2026-05-12

### Game client

#### Spectator (#156)

- **Phase 1 — Spectatable flag** — per-game `spectatable` bit added to the lobby wire protocol, surfaced as a toggle in the Create Game UI and persisted via `Config::lastspectatable`. Server browser plumbs the new `can_rejoin` bit alongside it.
- **Phase 3 — Native observer joins** — `Peer::observer` flag on the wire; AUTHORITY admits observers in `MSG_CONNECT` without consuming a player slot. Observers free their slot on disconnect, and observer chat fans out to everyone.
- **Phase 4 — Spectator camera + controls** — `viewedpeerid` drives camera and HUD focus; free-cam, cycle-target, and Activate-names bindings rebound for spectators. ESC exits the match cleanly. Joiner-camera, visibility, and create-game guard fixes folded in.
- **Scrollable Game Options form** — variable-height scrollbar with drag support, plus font and padding fixes so longer option lists fit the panel.

#### Rejoin mid-game (#152)

- **Parked-peer rejoin** — `HandleDisconnect` parks the peer instead of evicting them while AUTHORITY + INGAME + real accountid (not bot, not permanent kick). The `Player` object stays alive in the world, retaining team, tech choices, credits, inventory, weapons, ammo, and snapshot history. `UnDeploy()` still runs so the body disappears while the player is gone.
- **MSG_CONNECT in INGAME** — AUTHORITY now accepts reconnects whose accountid matches a parked peer, rebinding ip/port and resuming. Brand-new joiners mid-game stay rejected — this is rejoin, not join-in-progress. `MSG_KICK` carries `permanent=true` so kicks remain terminal.
- **Sweep hygiene** — peer-timeout sweep and `SendSnapshots` skip parked peers; on rejoin the player redeploys at a deploy station like a normal life with all state intact.

#### Game.cpp refactor foundation (#140)

- **Screen / Panel / Modal infrastructure** — base classes plus a `ScreenContext` (curated subsystem refs + state-machine actions, all stubbed) lay groundwork for breaking up the 6,544-line `Game` god-class. No behavior change in this PR; the screen stack starts empty and every menu still flows through the existing `Game::Create*Interface` / `Process*Interface` helpers. Screens migrate over the new tier in follow-up work.
- **Widget primitives moved to `src/ui/components/`** — `button`, `interface`, `overlay`, `scrollbar`, `selectbox`, `stats`, `teambillboard`, `textbox`, `textinput`, `toggle`. Bare-filename includes still resolve via the updated CMake include path.

### Game client — bug fixes

- **`world.gameinfo` on create-game (#147)** — regression from #140. The post-create handler in `LobbyScreen::Tick` dropped the `lobbygame → world.gameinfo` Serialize roundtrip, leaving `world.gameinfo.loaded = false` on the host. The `world.cpp:709` gate (`ishost && !gameinfoloaded && gameinfo.loaded`) never fired, so the host never sent `MSG_GAMEINFO` to the dedicated server, `AllPeersLoadedGameInfo()` stayed false forever, and Ready never advanced to INGAME. Fix re-adds the Serialize roundtrip before `JoinGame` in the create-game path.

## [v00049] — 2026-05-10

### Game client — bug fixes

- **macOS auto-updater Info.plist seal (#146)** — production-signed binaries have their embedded code signature bound to the bundle's `Info.plist` (the `Info.plist entries=N` line in `codesign -dvvv`). Stage-1 mirrored the binary and `Frameworks/` into `/tmp/silencer-stage2.app/` but not `Info.plist`; at `execve()`, AMFI rejected the signature ("The code contains a Team ID, but validating its signature failed") and SIGKILLed stage-2 before `main()`. Silent under hardened runtime since the parent's TTY is gone, so users saw the app close and never restart. Fix mirrors `Info.plist` into the stage-2 bundle so AMFI's execve hook accepts the binary. Same root-cause shape as the earlier `Frameworks/` mirror fix.

### Release / CI

- **macOS DMG installer (#146)** — release workflow now ships a notarized + stapled DMG alongside the existing zip. The DMG is the user-facing install: Finder presents the drag-to-`/Applications` affordance, and an explicit copy out of the DMG clears quarantine so the app runs from its real path instead of being App-Translocated to a read-only mount under `/private/var/folders/.../AppTranslocation/` (which is what breaks the auto-updater's in-place rename when users run `Silencer.app` straight out of `~/Downloads/`). The zip artifact stays for the in-app updater path (consumed by `clients/silencer/src/updater/updaterstage2.cpp` and referenced by `services/lobby/update.go`'s `MacOSURL` field). Release notes list DMG (recommended) + zip, mirroring the Windows pattern.

## [v00048] — 2026-05-09

### Game client

#### Controller support — Bluetooth & USB gamepads (#141)

- **Auto-detect** — `SDL_EVENT_GAMEPAD_ADDED` / `_REMOVED` open and close `SDL_Gamepad` handles; D-pad and left stick drive UI focus nav with software repeat (300 ms initial delay, 120 ms repeat). A confirms, B cancels; first A press auto-focuses the first item.
- **Profile auto-switch** — connecting a gamepad switches the live keybind profile to `gamepad`; disconnect restores the previous keyboard profile. `gamepad-custom` and other gamepad-derived profiles persist across restarts.
- **Defaults & rebinding** — `gamepad.json` ships Xbox-style defaults for every game action. Configure Controls renders short button names (LB/RB/LT/RT/A/B/X/Y …), PlayStation-aware via `SDL_GetGamepadType()`. Rebind captures buttons and axes; bindings persist as JSON. Fixed an axis-rebind threshold bug where the trigger compared against `32768` (above `int16` max) and never fired — corrected to `AXIS_DEADZONE`.
- **Rumble** — fire 80 ms click, hit 200 ms punch, land 120 ms thud via `SDL_RumbleGamepad`.
- **Tutorial** — key hints render gamepad button names instead of `(unbound)` when on a gamepad profile; tutorial steps 22–24 no longer freeze if the player outpaces the internal state machine.
- **Sign-off** — Xbox controller (Bluetooth) tested end-to-end. Remaining polish tracked in #143.

#### 2D sound occlusion, low-pass filter, stereo pan (#137, #138)

- **Occlusion rays** — `ComputeOcclusion()` walks `world.map.TestLine()` between listener and emitter, accumulating dampening per platform crossing (`RECTANGLE × 0.15`, `STAIRSUP/DOWN × 0.60`, all GAS-tunable). Per-channel `occlusionCache[]` lerps each update (`occlusionLerpSpeed`, default `0.25`) to avoid zipper noise as players or emitters move.
- **IIR low-pass filter** — `MIX_SetTrackCookedCallback` registers a single-pole IIR per track in the SDL3_mixer pipeline. Cutoff lerps from `occlusionMuffleMaxHz` (8 kHz) down to `occlusionMuffleMinHz` (400 Hz) below `occlusionMuffleThreshold`; passthrough above. Volume vs. filter factors are computed separately so stairs reduce volume but no longer trigger muffle. **Disabled by default** (`soundFilterEnabled=false`) until ray accuracy is confirmed across all maps; opt in per-map via `world.json`.
- **Stereo panning** — `MIX_SetTrackStereo()` updated each `UpdateVolume()` with `MIX_StereoGains`; `pan = -(dx/radius) × 0.8` clamped to ±1.
- **Local-player bypass** — your own footsteps, fire sounds, and impacts skip all spatial processing (no distance falloff, no occlusion, no filter, no pan) and always play at full volume.
- **Footstep audibility** — civ/guard footsteps raised from 16→64; player crouch/stair from 24→64 / 16→48 so they're audible at normal in-room distances.
- **State hygiene** — `occlusionCache`, `filterAlpha`, `filterState`, and stereo pan are all cleared on `TrackStoppedCallback` and re-cleared in `Play()` when a channel is claimed, so stale occluded-sound state can't bleed into the next play.
- **GAS** — `EmitSound` now uses `audioRange` from `world.json` instead of a hardcoded `500`. New world fields: `soundOcclusionEnabled`, `occlusionDampenRect`, `occlusionDampenStairs`, `occlusionLerpSpeed`, `occlusionMuffleThreshold`, `occlusionMuffleMinHz`, `occlusionMuffleMaxHz`, `soundPanningEnabled`, `soundFilterEnabled`.
- **Camera pan fixes (rolled in)** — `pancamerareturncount` is now set when `pancamerareturn=true` arrives via `MSG_CAMERA(0,0)` (previously the `World::Tick()` `>0` decrement loop never ran, so the return state stuck forever); the renderer's lerp path now clamps `camera.x/y` to `w/2,h/2` floors after lerping, so walking far left no longer drives `camera.x` negative and makes `IsVisible()` reject the world.

#### Mission / Trigger Scripting Tool (#124, issue #30)

- **Generic event bus** — `TRIGGER_ENTER_ZONE`, `TERMINAL_ACTIVATED`, `ACTOR_KILLED`, `ACTOR_DAMAGED`, `OBJECTIVE_COMPLETE`, `ITEM_COLLECTED`, `PLAYER_DIED`, `ALL_PLAYERS_DIED`, `TIMER_EXPIRED`, `GAME_START` events wired into the trigger graph.
- **Action system** — `OPEN_DOOR`, `LOCK_DOOR`, `UNLOCK_DOOR`, `PLAY_SOUND`, `SHOW_OBJECTIVE`, `PAN_CAMERA`, `SPAWN_ACTOR`, `END_MISSION`, `DESTROY_ACTOR`, `MOVE_ACTOR`, `APPLY_DAMAGE_IN_ZONE`, `ENABLE_TRIGGER`, `DISABLE_TRIGGER`. Each action supports an optional delay in seconds.
- **Condition system** — `ALL_OF`, `ANY_OF`, team check, objective state, player count, health threshold.
- **One-shot vs repeatable** flag per trigger; trigger enable/disable state (triggers can arm/disarm other triggers).
- **Hit counter + `COUNT_REACHED` condition** — trigger fires only after it has been hit N times.
- **Flag system** — 256 boolean runtime flags; `SET_FLAG` action and `FLAG_SET` condition; synced via `MSG_TRIGGER_STATE`.
- **Destructible actors** — actors flagged as destructible take damage and fire `ACTOR_KILLED` on death.
- **Collectible actors** — `ITEM_COLLECTED` fires when a player walks over a flagged actor.
- **`MOVE_ACTOR` action** — smoothly moves an actor to a target X/Y over N seconds (elevators, moving platforms).
- **Zone definitions** — invisible trigger regions placed in the designer, stored in `.sil`, checked each tick. `TRIGGER_ENTER_ZONE` fires when a player enters a zone.
- **`COMPLETE_OBJECTIVE` action** — marks an objective complete from a trigger.
- **`END_MISSION` outcome** — paramU8: 0 = neutral, 1 = win, 2 = lose.
- **`LOCK_INPUT` / `UNLOCK_INPUT`** — freeze and restore local player controls (used during camera pan cutscenes).
- **Script loader** — reads the trigger graph from the `.sil` map file at load time; new trigger and zone sections appended to the binary format.
- **Authority-owned trigger state** — only the AUTHORITY peer fires triggers; `MSG_TRIGGER_STATE` replicates objectives, flags, and hit counts to all peers.
- **Camera pan intro** — `GAME_START` fires at tick=0 (reliable, before any player interaction). `PAN_CAMERA` lerps the camera to a target zone; `UNLOCK_INPUT` lerps it back to the player. Camera pan state is reset in `UnloadGame()` so the menu renders correctly if the player disconnects mid-pan.
- **Designer trigger panel** — full TRIGGERS tab in the map designer sidebar: all trigger/condition/action node types, action delay fields, one-shot/repeatable toggle, objective list editor (add/remove, required/optional), zone tool (draw zones on canvas with cyan dashed outline + id label), zone list editor, actor canvas linking (🎯 button enters crosshair mode to fill actor id by clicking the canvas), Move Actor path preview on canvas. Serializes trigger graph into the map file.

#### Loading screen

- **Green gradient progress bar** — loading bar uses the game's green palette (indices 101–113), dark-to-bright left-to-right gradient, 32 px height, dark green background track.

### Game client — bug fixes

- **`ListFiles` stack overflow on missing directory (#139)** — `FindFirstFile` returns `INVALID_HANDLE_VALUE` (= `(HANDLE)-1`), not `NULL`, when the target dir doesn't exist. The truthy `if(dir)` check let the loop body run with an unpopulated `WIN32_FIND_DATA` whose `cFileName` had no terminator, so `sprintf("%s\\%s", directory, info.cFileName)` read past the struct, scanning 0xCC debug-fill bytes until it hit the null inside `directory2` (308 bytes later) and copied that whole run into `fullname[MAX_PATH]` — a ~228-byte stack overflow that clobbered the saved NRVO return-slot pointer at the function's home space. Crash repro: open *Create New Game* without an existing `<datadir>/level/download` directory (it's only created lazily on first map download). Latent since 2014; surfaced now because the spectatable-dialog work shifted the caller's stack just enough that the corrupted slot ended up non-recoverable. Same `if(dir)` pattern fixed in `selectbox.cpp` `ListFiles` helper.

### Docs

- **Progression spec — XP formula** — `CalculateExperience()` weights documented for all 16 stat actions; threshold formula `threshold(N) = 100 × (N+1)`; cumulative `total_xp(N) = 50 × N × (N+1)`; level-0-to-99 milestone table; `xp_remaining` display formula.

## [v00047] — 2026-05-07

Test release — no behavior change. Cut to validate the v00046 Windows auto-updater end-to-end (per-file `MoveFileEx` replace path + `DisplayVersion`/`DisplayName` sync into Inno's HKCU `{AppId}_is1` uninstall key). An existing v00046 install should detect, download, swap, and relaunch as v00047 without manual intervention.

## [v00046] — 2026-05-07

### Game client

#### Main menu cleanup (#130)

- **Dev-only buttons removed** — `Host Game` / `Join Game` / `Test` / `Test Replay` (uids 4/5/6/7) gone from the main menu; they were dev entry points behind an `if(1)` gate, never intended user-facing. Rolls back the "Test button on the main menu" bullet from v00045. State enums and handlers stay (HOSTGAME is referenced by the dedicated-server gate; REPLAYGAME is reachable via the `-r` CLI flag).

#### Windows installer & auto-updater (#131)

- **Inno Setup installer** — new `silencer-windows-x64-setup-<version>.exe` artifact ships alongside the portable zip. Installs per-user to `%LOCALAPPDATA%\Programs\Silencer\`, outside Defender's hot-path Downloads scan. Recommended channel; the zip stays for advanced users.
- **Per-file replace updater on Windows** — stage-2's directory rename (`MoveFileA install → install.old`) was failing with `STATUS_ACCESS_DENIED` whenever any descendant had an open handle without `FILE_SHARE_DELETE` — exactly what Defender's hot-path scan of Downloads holds, which is why every prod update from a Downloads-extracted zip silently failed (`rename install→old failed`) and the user was never relaunched. Replaced with a recursive `MoveFileEx (REPLACE_EXISTING | COPY_ALLOWED | WRITE_THROUGH)` walk. For the rare genuinely-locked file, sideline as `<file>.old-<ticks>` and move the new one in; startup-time `CleanupPreviousUpdate` sweeps those leftovers recursively (so nested files under `assets/` aren't missed). POSIX rename keeps the original directory-swap path — `rename(2)` on Linux/macOS handles open files atomically.
- **Add/Remove Programs version sync** — stage-2 only swapped files on disk, leaving Inno's HKCU `{AppId}_is1` uninstall key's `DisplayVersion` stuck at install-time forever. Startup hook now overwrites `DisplayVersion` and `DisplayName` from the running EXE's compiled-in `SILENCER_VERSION`. No-op for zip users (the key doesn't exist; `RegOpenKeyEx` fails fast). `silencer.iss` also pins `AppVerName` so `DisplayName` is plain `Silencer` instead of Inno's default `Silencer version <ver>`.
- **Uninstaller wipes install dir** — `[UninstallDelete] Type: filesandordirs; Name: "{app}"` removes any file the auto-updater dropped in `{app}` (or earlier installs left there) — Inno's uninstaller only deletes files in its install-time manifest. Safe because user data lives at `%APPDATA%\Silencer\`.
- **Update log visibility** — `RenameDir`'s `MoveFileA` error path now routes through `Logf` (visible in `%TEMP%\silencer-update.log`) instead of stderr-only `fprintf`, so future failures are diagnosable.

## [v00045] — 2026-05-07

### Game client

#### Configure Controls — preset cycle (#127)

- **Preset row** — new "Preset:" button at the top of the Configure Controls menu cycles through available keybind profiles (`default` / `wasd` / `gamepad` plus any user-saved profiles) and updates the live keymap immediately.
- **Auto-fork on edit** — editing any binding while a built-in profile is active forks to `<name>-custom`, so on-disk built-ins are never shadowed by writable copies. Save skips writing built-in profiles since any edit forks first.
- **Default WASD bindings tweaked** — `fire` is now `J` (was Mouse 1) and `prev_cam` ships unbound (was Left Ctrl). Existing custom profiles are unaffected.

#### Bots & Test mode (#122, issue #32)

- **Test button on the main menu** — `Test` (and the previously hidden `Host Game` / `Join Game` / `Test Replay` buttons) is now exposed; `Test` launches a TESTGAME with 10 bots.
- **Combat AI** — `PlayerAI::ScanForTarget` does an AABB scan for the nearest valid enemy in `aiCombatRange` (300 px), filtering teammates / invisible / disguised / in-base players. `ApplyCombat` faces and fires at the locked target; combat input has higher priority than navigation so path inputs don't fight aim direction.
- **Difficulty system** — `EASY` (3× fire interval, no evasion), `MEDIUM` (2×, jump-dodge on damage), `HARD` (1×, jump-dodge on damage). TESTGAME spawns 4 easy / 4 medium / 2 hard.
- **`[BOT]` tag** in the player list.
- **New tunable GAS params** in `player.json`: `aiCombatRange`, `aiFireInterval`, `aiEvadeInterval`, `aiTargetLockTicks`.

#### Lighting & shadow editor (#120, issue #38)

- **Default ambience −20** — new maps start at `ambiencelevel ≈ 38` so placed lights visibly pop against the dark background. Closer to the original look the game was designed around.
- **Map Properties: ambience presets** — quick buttons: Bright (0) / Medium (−10) / Dark (−20) / Very Dark (−28).
- **Light actor placement** (Phase 2) — admin level designer now places `Light` actor (`id=71`); `map.cpp` case 71 spawns a bank-222 OVERLAY at `(x, y)` with `res_index = actortype`. Renderer pushes bank 222 overlays to `objectlights` and draws via `palette.Light()`.
- **`ALLY10cNight.sil`** added to the bundled map set.

### Game client — bug fixes

- **Guards stop wedging at walls** (#126) — `WALKING` is now pure motion (`xv`, `FollowGround`, footstep sounds). Turnaround/duration decisions move into the BT: `Patrol` flips `mirrored` at chain ends and transitions WALKING→LOOKING at `walkingDurationTicks`; `SearchAndReturn` clears `chasing` when blocked at a wall in the search direction (guard searches blindly the rest of the timeout) and snaps to `STANDING` facing `originalmirrored` when blocked on the way home.
- **Jetpack / hack / flamer / terminal / base-exit loops cut off correctly** (#125) — `Audio::Stop` was passing `MIX_MSToFrames(mixerspec.freq, ms)` to `MIX_StopTrack`, but `fade_out_frames` is in the *track input's* sample rate. With ~22 kHz ADPCM and 96 kHz mixer output, a 200 ms requested fade ran ~870 ms. Switched to `MIX_TrackMSToFrames(track, ms)`.
- **Civilian-disguise bypass** (#125, issue #3) — in `Guard::Look`'s line-shaped lookbox path (forward standing/crouched, up, down — directions 0/1/2/3), an AABB scan would set `target=true` from an undisguised player, then `TestIncr` returned a closer disguised one and the guard fired on the disguise. Now mirrors the rectangular-lookbox path and re-validates `ShouldTarget` on the `TestIncr` return.
- **Map API URL default** — corrected to `admin.arsiamons.com` (admin-api proxies `/api/maps` to the lobby; `maps.arsiamons.com` doesn't exist).

#### Windows / MSVC build

- `NOMINMAX` defined to stop `windows.h`'s unscoped `min`/`max` macros from breaking `std::min` / `std::max`.
- `_USE_MATH_DEFINES` force-included via `msvc_snprintf_compat.h` so every TU sees `M_PI` before any math header is pulled in (renderer.cpp errors after the lighting merge).

### Infrastructure

- **Temporary AWS smoke-test environment** (#91, removed in #235) — disposable `t4g.small` running the full prod stack (lobby + dedicated game servers + admin-api + admin-web + Mongo + LavinMQ) as native systemd units, redeployed on every push to `main` via GitHub Actions. `concurrency: cancel-in-progress: false` coalesces queued runs to the newest commit.
- **URL-safe seeded credentials** — `seed-ssm` now uses base64url for the seeded Mongo and LavinMQ passwords. Plain base64's `/` and `+` broke `net/url.Parse` when the password landed inside `mongodb://` / `amqp://` URLs (bit the staging lobby on first deploy).

## [v00044] — 2026-05-01

### Admin dashboard

#### VFX Editor (`/vfx`) — new page

- **Particle preset editor** — create, edit, and preview VFX effect presets in a two-pane layout.
- **Live canvas preview** — real-time particle simulation in the browser; updates instantly as you change parameters.
- **Effect types** — Particles, Sprite Flash, Screen Shake.
- **Particle controls** — emission rate, burst count, lifetime, start/end size, color gradient (color picker + hex), alpha, speed, speed variance, spread angle, gravity.
- **6 sample presets** included: Explosion (Small/Large), Sparks, Smoke (Rising), Plasma Trail, Screen Shake (Impact).
- **Load/save** `vfx-presets.json` locally; download updated file to your GAS folder.
- **Arrow key navigation** and search/filter in preset list.
- Add, duplicate, delete presets.
- VFX Trigger system integration with level designer (Phase 2 — in progress).

## [v00043] — 2026-05-01

### Admin dashboard

#### Items Tool (`/items`) — new page

- **Item list** — all items from `items.json` in a sidebar.
- **Property panel** — inline-editable identity (id, enumId, name, description), sprite (bank + index), purchase prices, tech-tree (techChoice bitmask, techSlots), agency restriction dropdown (All / per-agency), stats & effects (ammo, heal, poison).
- **GAS store integration** — folder opened in `/items` (or any other tool) stays loaded when navigating across tools without re-picking.
- Saves back to `items.json` via browser download.

## [v00042] — 2026-04-30

### Admin dashboard

#### Weapon Tool (`/weapons`) — new page

- **Weapon list** — all weapons from `weapons.json` with search/filter.
- **Property panel** — sprite bank pickers (8-directional), sound pickers per event (`soundFire`, `soundHit1/2`, `soundLoop`, `soundExplosion`, `soundLand`, `soundThrow`) with live preview links to Sound Studio.
- **Ballistics preview** — tick-accurate canvas simulation: gravity, velocity, rocket hover, plasma gravity, explosion radius, splash damage labels. Grenade arc, rocket loft, EMP/neutron effects all simulated at 24 ticks/sec.
- **Agency loadout editor** — checkbox grid showing which agencies carry each weapon; saves back to `agencies.json`.
- **Shared GAS store** — folder opened in `/weapons` stays loaded when navigating to `/gas` or `/sound-studio` without re-picking the folder.

#### GAS editor (`/gas`)

- **Restore sync fix** — `↩ RE-ADD` and `↩ RE-ADD ALL` now update the shared GAS store so restored fields are not lost on navigation.

#### Sound Studio (`/sound-studio`)

- **Styling parity** — page now inherits the global dark theme instead of rendering with hardcoded inline background/color/font overrides.

#### Admin UI

- **Animated space background** — programmatic starfield (350 stars with twinkling), comets with gradient fading tails, slowly drifting Mars surface image, mouse parallax (±25 px, smoothed).

### Game client

- **Bundled map upload skip** — maps shipped inside the app bundle (`Resources/`) are pre-loaded on the server and no longer trigger a redundant upload attempt on game start.
- **FindMap absolute path fix** — `FindMap()` now captures an absolute path immediately after `CDResDir()` so the fallback path is always absolute regardless of the current working directory.
- **GAS: all projectile classes data-driven** — blaster, laser, rocket, flamer, plasma, flare, wall, and grenade projectiles read sprite banks, sounds, and physics from `weapons.json` with hardcoded fallbacks (zero behavior change).
- **Bug fixes from code review:**
  - Neutron bomb and EMP bomb: `0` damage values were incorrectly treated as "not set" and fell back to `0xFFFF` (max damage). Fixed.
  - Rocket launch sound: was reading from `soundLand` (landing/bounce field) instead of `soundFire`.
  - `std::min` macro clash on Windows in flamer/flare projectile constructors fixed with `(std::min)()`.

### Infrastructure

- **Docker build context fixed** — `admin-web` and `admin-api` Dockerfiles use the repo root as build context (workspace lockfile + all `package.json` manifests live there). `docker-compose.yml` context pointers updated to `..` with explicit `dockerfile:` paths.
- **Compiled-in production defaults** — CMakeLists.txt now defaults to `lobby.arsiamons.com`, `maps.arsiamons.com`, and `admin.arsiamons.com` so release builds connect to production without manual config.

---

## [v00041] — 2026-04-28

### Admin dashboard

#### GAS editor (`/gas`)

- **URL-linked tabs** — each GAS file tab updates the URL (`?tab=filename`) so tabs are bookmarkable and survive page refresh.
- **Baseline validation** — on folder open the editor captures a baseline snapshot of all loaded JSON. Saving is blocked if any field that existed at load time has been removed, including fields inside array entries matched by `id` (weapons, enemies, items, etc.).
- **Problems panel** — a VS Code-style inline tray below the tab bar lists every validation violation with full field path. Clicking a file header in the tray jumps to that tab.
- **RE-ADD button** — each violation row shows a `↩ RE-ADD` button that restores the missing field from the baseline in one click.
- **RE-ADD ALL** — each file section header in the Problems panel shows `↩ RE-ADD ALL (N)` to restore all missing fields for that file in a single atomic update.
- **Direct tab URLs** — each tab is an `<a>` tag so tabs can be opened directly or middle-clicked.

### Game client

- **Community map upload** — client uploads the current map to the server before creating a game, so other players can download it. Upload URL read from `mapapiurl` config key.

### Infrastructure

- **nlohmann/json vendored** — `json.hpp` (v3.12.0) checked in to `clients/silencer/third_party/nlohmann/` to eliminate a flaky CMake FetchContent download step in CI.
- **Map symlinks** — lobby server maintains a `maps/` symlink directory so the dedicated server binary can read uploaded maps without a restart.

---

## [v1.9.0 / v00029] — 2026-04-26

### Game client / dedicated server

#### Behavior tree AI

- **C++ behavior tree interpreter** — loads `.json` BT files from the assets
  directory and evaluates them each tick. Supports Sequence, Selector,
  Inverter, and a full set of game-specific Condition/Action leaf nodes.
- **Guard AI wired to behavior tree** — full combat pipeline (Look, Aim, Shoot,
  Crouch, Patrol) now driven by the BT. `guard.cpp` state machine replaced by
  BT tick calls.
  - `SearchAndReturn` — non-patrol guards search the last known target position
    then walk back to spawn.
  - Ladder climbing in SearchAndReturn — guards climb/descend ladders during
    search phase with 2 s cooldown and 48 px vertical gap requirement to
    prevent stuck loops.
  - Fixed: crouch-shoot, stay-at-post patrol, stop gliding during crouch
    states, back-away-when-too-close, clear chasing on player death / base
    entry / untargetable.
- **Robot AI wired to behavior tree** — `robot.cpp` uses BT for patrol,
  `ReturnToSpawn` (replaces old Sleep), damage wakeup, `LookSides` returning
  Failure so Patrol runs every tick.
- **Civilian flee BT wired** — `civilian.cpp` flee logic driven by
  `civilian.json` behavior tree.

#### Actor definition system

- **Client syncs actordefs from server on startup** — fetched async on each
  map load so the admin tool changes are picked up without a client rebuild.
  Uses a separate `adminapiurl` config key to avoid affecting lobby traffic.
- **Per-frame sounds — data-driven** — `FrameDef` gains `sound` + `soundVolume`
  fields. `AnimSequence::GetFrameSoundByIndex(frameIdx)` looks up sound by
  sprite frame index (correct for `state_i % N` state machines).
- **Guard WALKING footsteps** — `guard.cpp` calls `GetFrameSoundByIndex` with
  `state_i % 19` to play `stostep1.wav` / `stostepr.wav` at frames 4/13,
  driven by `guard-*.json` actordefs — fully configurable in the actor editor.
- **Civilian footsteps** — `civilian.cpp` WALKING and RUNNING use
  `GetFrameSoundByIndex` to play footstep sounds from `civilian.json`.
- **Per-weapon guard actordefs** — `guard.json` split into
  `guard-blaster.json`, `guard-laser.json`, `guard-rocket.json`. Each can now
  have independently tuned animations, hurtboxes, and sounds. `ActorDefName(weapon)`
  helper maps weapon integer to the correct file name.
- **Body parts replicated to clients** via snapshot packets — previously only
  simulated on the server.
- **Player hurtboxes** — all player animation sequences (WALK, RUN, JUMP,
  CROUCH, etc.) now have default hurtboxes in the actordef; fixes bullet
  collision regression.
- **Guard kneel loop fixed** — CROUCHED uncrouch guard no longer loops endlessly.
- **`Look()` origin restored** to y=−55 for reliable target detection.
- **Version bumped to `00028`**.

### Admin dashboard

#### Behavior tree editor

- Visual drag-and-drop BT editor with node palette, JSON preview, and full
  undo/redo.
- **Local-file mode** — editor reads/writes `.json` files directly from a
  user-selected folder (no database required). `webkitdirectory` input on HTTP,
  `showSaveFilePicker` on HTTPS.
- Download button exports the current tree as a `.json` file.
- Blackboard key editor improved: sortable key list, type badges, one-click
  delete.
- State machine tab removed from actor editor — superseded by behavior trees.

#### Actor editor

- **Local-file mode** — load/save actordefs from a local folder via browser
  file picker; no MongoDB write path in production.
- **Auto-size preview canvas** — canvas resizes to the largest sprite in the
  selected sequence.
- **Tab URL persistence** — `?tab=` query param keeps selected tab across
  navigation.
- **Hitbox editor**:
  - Auto-fit hurtbox button snaps the box to the non-transparent pixel bounds
    of the current sprite frame.
  - Clear all hurtboxes button.
  - All player animation sequences (WALK, RUN, CROUCH, etc.) available for
    editing.
- **Animation tab**:
  - Sound picker — searchable dropdown of all 98 in-game sounds, plus a ▶
    preview button per row that plays the WAV at the configured volume.
  - `soundVolume` field (0–128) per frame; preview respects it.
  - Scale toggle: 1×/2×/3×/4× preview size (defaults to 1×).
  - Grid background — matches the hitbox tab dark grid so transparent sprites
    are visible.

#### Lobby server (Go)

- Behavior trees stored in MongoDB and synced to the game client on startup.
- Actordefs and BTs migrated to filesystem-first (MongoDB write-through
  removed from the read path).
- `–version 00028` passed to lobby process in `docker-compose.yml`.
- MongoDB password redacted from `mongosync` log lines.
- Empty `–version` flag now falls back to manifest version (not crash).

### Infrastructure

- **Assets volume mounted read-write** in `docker-compose.yml` so `admin-api`
  can write actordefs back to disk from the actor editor.
- **Dedicated server bundle** — `SDL3` and `SDL3_mixer` `.so` files copied into
  the server package by `install-linux-server.sh`; fixes missing-library crash
  on fresh ARM64 hosts.
- **Lobby flags pinned**: `–maps-dir /var/lib/silencer/maps`,
  `–update-manifest /opt/silencer/update.json`.
- **CI (macOS)**: `SDL3_ROOT` env vars passed to `dylibbundler`; search dirs
  narrowed to avoid scanning the full filesystem during macOS release builds.

---

## [v00025] — 2026-04-25

### Game client / dedicated server

- **Community map downloads** — server-published maps now appear in the
  Create Game map list with an inline `[DL]` badge. Clicking the badge
  starts an async background download (no UI freeze). A progress bar fills
  the row (0–100%) while the file transfers via curl XFERINFO callback. On
  completion the interface rebuilds automatically so the map preview renders
  immediately. The Create button is blocked while a not-yet-downloaded map
  is selected.

- **Agency switch in pregame lobby** — switching agencies now broadcasts
  `MSG_SETAGENCY` to the dedicated server, which reassigns the peer to the
  correct team immediately. Previously the server only recorded agency at
  `MSG_CONNECT`; rejoining the lobby was required to see the change.
  Agency logo overlays now update in real-time when a peer switches.

- **Kick banned players from active games** — banning a player via the
  admin UI now sends `MSG_KICK` (UDP) to the dedicated server hosting their
  current game. The server authenticates the message by checking the sender
  IP matches the lobby address, then calls `KillByGovt`/`HandleDisconnect`
  to eject the player back to the main menu.

- **Tile-flip crash fix** — `patchTile` used a stale closure over
  `width`/`layers`, causing a client-side exception when flipping a tile on
  the X axis in the designer.

### Map designer (admin web)

#### Community map publishing

- **Publish maps from the designer** — maps can be uploaded to the lobby's
  community map API (`POST /api/maps`, multipart/form-data, SHA-1 indexed).
  `GET /api/maps` lists all published maps; `GET /api/maps/by-sha1/:sha1`
  serves the raw `.sil` for client download with hash verification.

#### Platform collision rendering

- **Diagonal cross-stitch restricted to stairs/ground** — the collision
  overlay cross-stitch pattern now only renders on `RECTANGLE`,
  `STAIRSUP`, and `STAIRSDOWN` platforms. `RAIN`, `ROOM`, ladders, and
  other volume types are no longer cross-stitched, matching their
  semantic meaning in the engine.

#### Platform tools

- **RAIN / OUTSIDEROOM platform** (`🌧 RAIN`, type1=3 type2=0) — the volume
  type the engine uses to calculate rain-puddle spawn locations; now
  available in the Toolbar.
- **ROOM / SPECIFICROOM platform** (`▣ ROOM`, type1=3 type2=1) — added
  alongside RAIN.
- **Platform drag fix** — all three event handlers (mouseDown / mouseMove /
  mouseUp) now include the new platform types so dragging to place volumes
  works correctly.

#### Platform selection, resize, and move (SELECT tool)

- Click any platform volume in SELECT mode to select it; an animated
  marching-ants outline appears.
- Eight white square handles (TL / T / TR / L / R / BL / B / BR) allow
  resizing by dragging. Corner handles move two edges; mid-point handles
  move one edge. Minimum size enforced at 16 world units.
- Drag the body of a selected platform to move it without resizing.
- Click empty space to deselect.
- Resize/move operations are added to undo history via `updatePlatform`.

#### Actor marching-ants selection

- Selecting an actor in SELECT mode or clicking an actor in the Actors
  panel now shows an animated dashed-outline highlight around the sprite
  bounds, drawn on the pointer-events-none overlay canvas.

#### Earlier designer additions (since v00024)

- **Erase tool** — removes tiles, actors, or platforms under the cursor.
- **New map / Map properties** — create a blank map or edit name,
  width, height of the current map; map resize preserves existing content.
- **Actor drag** — actors can be repositioned by dragging in SELECT mode.
- **Tile filter** — search tiles by name in the tile picker.
- **Keyboard shortcuts** — S(elect), T(ile), P(latform), A(ctor),
  E(rase), Ctrl+Z/Y undo/redo, Ctrl+drag to pan.
- **Actor list panel** — lists all placed actors; click to select and
  center the viewport.
- **Minimap** — thumbnail overview of the full map; click to jump.
- **Save dialog** — uses File System Access API with a download fallback
  so `.sil` files can be saved directly from the browser.
- **Actor 60 (Camera Focus)** — designer-only placement marker for
  scripting camera positions; no `map.cpp` handler required.
- **Actor types 47 / 50 / 61 / 67 / 68 / 69** — added with correct
  labels; Laser Defense label corrected.
- **Powerup actor** — correct 7 subtypes with dynamic sprite bank lookup.
- **Pickup type list** — corrected to match `pickup.h` enum (21 types).
- **Tile right-click context menu** — Flip X, LUM toggle, Clear tile.
- **Visibility toggles** — independently show/hide tile layers, platforms,
  actors, grid, and lighting overlay.
- **STAIRSUP / STAIRSDOWN rendering** — drawn as triangles matching
  in-game appearance.
- **Terminal sprite** and actor right-click properties panel.
- **Actor sprites** — rendered from the game's sprite banks (128×128
  sheets) rather than placeholder icons.
- **Per-cell luminance** — ambient/dark lighting applied per tile via
  `ctx.filter` brightness; LUM flag and toggle button added.
- **Undo / redo history** — full undo stack for tile paint, actor place,
  platform draw, and all property edits.
- **Map resize** — width/height resize via UI.
- **Performance** — eliminated per-tile `ctx.filter` changes for a large
  rendering speedup on dense maps.

---

## [v00024]

- **Auto-updater** — client consent modal + in-place binary swap driven by
  a lobby manifest; players are prompted when a new version is available.
- **Lobby presence sidebar** — shows which players are currently online in
  the main lobby.
- **libcurl + libminizip** — added to both cloud-init (Terraform VM
  provisioning) and the GitHub Actions Deploy workflow so the dedicated
  server binary builds correctly on ARM64.

---

## [v00023] and earlier

- Self-hosted Go lobby server replacing the defunct `lobby.zsilencer.com`.
- Headless dedicated-server mode (`zsilencer -s …`).
- Admin dashboard (Express API + Next.js frontend): player management,
  audit log, game stats, community leaderboard, ban/unban, password reset.
- Community game-stats page (`/gamestats`) with drill-in rows, sorting,
  agency filter, and search.
- How-to-Play guide (`/howto`) with original game images from the Wayback
  Machine.
- 200 concurrent game slots (ports 20000–20199).
- One-script Linux server install (`scripts/install-linux-server.sh`).
- AWS Terraform module (EC2 + EBS + Tailscale) for cloud hosting.
- GitHub Actions workflows: tag-triggered ARM64 lobby deploy, macOS +
  Windows client release zips.
