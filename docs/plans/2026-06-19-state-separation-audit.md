# State-management separation audit — Silencer

**Date:** 2026-06-19
**Scope:** Diagnostic, read-only. How cleanly are simulation/world state, screen/UI/mode state,
and network/connection state separated across the three tiers (local client, Go lobby server,
in-world dedicated server). No code changes.

**Headline:** The separation is **strong in exactly one place and weak everywhere else**. The cppx
UI layer (the part mid-migration) is the cleanest seam in the codebase — a genuine one-way
projection from game mode to screen, with no simulation bleed. Below that line, the C++ runtime is a
**partitioned god object** (`Game` owns `World` owns transport + lobby + audio + resources), three
desynchronized "where am I" state machines, two role bits (`World::mode`, `DedicatedServer::active`)
implemented as scattered `if` branches, and a single process-lifetime `World` whose per-match
correctness rests entirely on a hand-maintained reset list. None of this is broken at runtime — it
works — but almost none of it is *designed* separation; it's grown coupling held together by
convention.

The good news for the migration: the cppx boundary is the right shape and should be treated as the
template, not the exception. The risk is that the C++ tangle gets cemented behind that clean
facade — the UI looks decoupled while `Game`/`World` underneath stay fused.

---

## 1. Current map

### 1a. Simulation / world state

Lives in **`World`**, a value member of `Game` (`clients/silencer/src/game/game.h:99` — `World world;`).
`World` was refactored from a flat god object into a coordinator owning five subsystem structs
(`clients/silencer/src/world/world.h:46-50`):

```cpp
WorldMessaging      messaging;
WorldObjectRegistry objects;       // actors, physics, collision (TestAABB/TestIncr)
WorldNetwork        network;       // UDP socket + transport state
WorldPeerRegistry   peers;         // peerlist[], authoritypeer, peercount, localpeerid
WorldReplication    replication;   // snapshot ring oldsnapshots[25][...], input history
```

Plus simulation data as direct members (`world.h:172-198`): `Map map`, `Resources resources`,
`Audio &audio`, `Lobby lobby`, `gravity`, `tickcount`, `winningteamid`, `gameMode`, camera/spectator
structs, buyables, replay, and the in-flight map-download buffer (`world.h:300-301`).

- **Tick loop:** `World::Tick()` (`world/world.cpp:78`). The sim only advances when
  `world.gameplaystate == World::INGAME` (`game/loop/game_loop.cpp:282,289`).
- **Authority vs replica:** a single `bool World::mode ∈ {AUTHORITY, REPLICA}` (`world.h:270`,
  `world.h:37`), tested via `IsAuthority()` (`world/network/world_network.cpp:885`). The authority
  peer runs the sim and sends snapshots; replicas send inputs and apply deltas. Dedicated mode (`-s`)
  becomes permanent authority by calling `world.Listen(bindport)` at startup
  (`game/init/game_init.cpp:98`) and never switching back.

### 1b. Screen / UI / mode state

There are **two distinct mode axes plus a stack**, which is the single most important structural fact
in this audit:

1. **MODE axis — `Game::state`** (`game.h:117`), an 18-value enum `GameState::{NONE, FADEOUT,
   MAINMENU, LOBBYCONNECT, LOBBY, UPDATING, INGAME, MISSIONSUMMARY, SINGLEPLAYERGAME, OPTIONS*,
   HOSTGAME, JOINGAME, REPLAYGAME, TESTGAME, CREATECHARACTER}` (`game/state/game_state.h:6-27`).
   Driven by `GoToState` / FADEOUT / `stateisnew` (see 1d).
2. **SIM axis — `World::gameplaystate`** ∈ `{NONE, INLOBBY, INGAME}` (`world.h:281`). The simulation's
   own notion of liveness, set independently inside the tick handlers.
3. **SCREEN stack — `ScreenStack`** (`client/ui/app_shell/navigation/screen_stack.{h,cpp}`), a true
   fixed-array layered stack (cap 32). `visible_screens()` walks down from the top and returns the
   first `Normal` screen plus every `Overlay` above it (`screen_stack.cpp:82-95`).

The cppx UI models screen state as a **layered stack reconciled against a read-only projection of the
mode axis**, not as its own state machine:

- **`AppRoot`** (`client/ui/app_shell/app_root.{h,cpp}`) is always stack entry 0, pushed once at init
  (`game/ui/game_ui_pipeline.cpp:1767-1768`) and never popped. Its view reads `use_session().phase`
  and maps it to a base screen via a static table (`app_root.cpp:60-77`): MainMenu→MainMenu,
  Connecting→LobbyConnect, Lobby→LobbyScreen, InMatch/SinglePlayer→InGameScreen, etc.
- **Overlays** (PauseScreen, OptionsScreen + sub-tabs, modals) are `OverlayScreen`s pushed *above*
  AppRoot via `use_navigation()` and never touch `phase`.

So: **mode → base screen is one-way** (only the game state machine can swap the base layer, via
phase); **UI → overlays is free** (the queued nav stack). Stack mutations are queue-then-drain:
screens enqueue via `use_navigation` closures → `ClientUi::queue_*` → `mutations_[]`, drained after
the build pass in `drain_deferred_mutations()` (`client_ui.cpp:314-343, 391-410`). The only un-queued
push is the one-time AppRoot mount.

### 1c. "Am I in a menu vs. a live match?" — no single source of truth

There is **no single variable**. The canonical predicate ANDs two axes:
`Game::IsLiveMultiplayer()` = `(world.peers.peercount > 1) && (world.gameplaystate == World::INGAME)`
(`game.cpp:736-738`). The in-game guard in the loop ANDs mode *and* sim:
`world.gameplaystate == World::INGAME && (state == INGAME || SINGLEPLAYERGAME || TESTGAME)`
(`game/loop/game_loop.cpp:456`). A third cluster of connection booleans lives on `World`
(`world.network.state == CONNECTED`, `world.lobby.state == AUTHENTICATED`). Answering "where am I"
correctly requires reading `Game::state` **and** `World::gameplaystate` **and** `Lobby::state` —
three independent state machines in two objects.

### 1d. The mode transition mechanism

`Game::GoToState(newstate)` (`game.cpp:647-656`) never jumps directly: it stashes the target in
`nextstate`, sets `state = FADEOUT`, starts a palette fade, and latches `stateisnew = true`.
`TickFadeOut()` (`tick/tick_misc.cpp:6-14`) dims to black, then commits `state = nextstate`.
`stateisnew` is a one-shot "first frame of this state" latch — **all destructive lifecycle work
(Disconnect, UnloadGame, DestroyAllObjects, LoadMap) hangs off these `if(stateisnew){…}` entry
blocks.** `session_phase.h::project_session_phase(state, fadefromstate)` (`game/ui/session_phase.h:14-48`)
is a **pure read-only projection** of the mode axis onto `SessionPhase`; during FADEOUT it reports
the *source* state so the outgoing screen stays mounted while dimming.

### 1e. Cross-tier handoff — tracing one match

Client at main menu → connect to lobby → create/join → in-world sim → return:

1. **Main menu.** `Game::state = MAINMENU`. `world.gameplaystate = NONE`. No connections.
2. **Connect to lobby.** `LOBBYCONNECT` → `Lobby` (a value member of `World`, `world.h:176`) opens its
   **own TCP socket**, runs its 14-state handshake (`net/lobby.h:24`: RESOLVING→…→AUTHENTICATED).
   On the lobby server, `serveClient` spins one goroutine per conn (`services/lobby/client.go:56-86`);
   the `Hub` adds the client to `clients` and replays the game list via `sendNewGame(1, g)`
   (`hub.go:79-81`). Client mirrors the list into a `std::list<LobbyGame*>` (`lobby.cpp:225-285`).
3. **Create game.** Client `opNewGame` → `Hub.RequestCreateGame` allocates `gid`, inserts a
   `pendingGame` with a 30s abort timer, and `proc.Start` spawns
   `silencer -s 127.0.0.1 <lobbyPort> <gameID> <accountID>` (`proc.go:45-55`). That dedicated process
   is a **second instance of the same binary**, permanent AUTHORITY, SDL video/audio skipped
   (`game_init.cpp:176-202`).
4. **Heartbeat → joinable.** The dedicated server UDP-heartbeats `[0x00][gameid][port][state]`
   (`dedicatedserver.cpp:66-105`); `Hub.OnHeartbeat` promotes `pending → games`, synthesizes
   `Hostname` from `publicAddr`+port, and broadcasts `sendNewGame(status=1)` (`hub.go:366-401`). 30s
   of silence → `failPending` → `status=2` "Could not create game" (`hub.go:343-361`).
5. **Join the sim.** `GameSession::JoinGame` (`game_session.cpp:70-80`) sets the authority peer's
   ip/port from the lobby game record and calls `world.Connect(...)` — a **separate UDP socket** to
   the dedicated server. The lobby TCP conn **stays open** throughout; the client only sends
   `opSetGame` presence (`game_loop.cpp:392`). `TickInGame` `stateisnew` (`tick/tick_ingame.cpp:12-86`)
   runs `LoadMap`, sets `world.gameplaystate = INGAME`, creates PLAYER objects, builds `gameMode`.
6. **Return to lobby/menu.** `LeaveMatchToMenu()` (`game.cpp:93-104`) calls `world.Disconnect()` then
   `GoToState(LOBBY/MAINMENU)`. The **actual world teardown fires later** when the destination
   state's `stateisnew` block calls `GameSession::UnloadGame()` (`game_session.cpp:42-68`) — a
   hand-rolled field-by-field reset of the single, never-destroyed `World`.

**Authority/ownership per tier:** in-world server = sole authority for live match state; lobby =
authoritative for the browser list, presence, pending-spawn lifecycle, identity/auth; client = holds
read-only mirrors of both. That part is genuinely clean (see §2.6).

---

## 2. Where responsibilities blur

### 2.1 Connection state is fused into the simulation object — **structural, grown**

`World` owns the UDP transport (`WorldNetwork`, with `state ∈ {IDLE,LISTENING,CONNECTING,CONNECTED}`
at `world/network/world_network.h:43`) **and** the TCP lobby client (`Lobby lobby;` at `world.h:176`,
which itself has a 14-state connection machine) **and** the simulation **and** audio **and** resources
— all in one instance. There is no `Connection` / `Session` / `NetChannel` type. "Am I connected"
(`world.network.state`), "am I in a game" (`world.gameplaystate`), and "am I authed to the lobby"
(`world.lobby.state`) are three separate fields living inside the same simulation object, and the UI
reads them directly: `lobby_ui_model.cpp:380-381` reads `world.IsConnected()`;
`game_ui_pipeline.cpp:1251` reads `world.lobby.state`. **This is the single biggest blur.** Transport
liveness, lobby session, and world simulation should be three objects; they are one.

### 2.2 SIM teardown is bolted onto destination-screen entry, not match exit — **convention, grown**

There is **no `OnExit(INGAME)` hook**. Leaving a match does not itself destroy world state;
teardown happens *coincidentally* because whatever state you `GoToState` into runs `UnloadGame()` in
its own `stateisnew` block — MAINMENU (`game_loop.cpp:485`), LOBBY (`:515`), MISSIONSUMMARY (`:595`),
REPLAYGAME (`tick_replay.cpp:11`). `LeaveMatchToMenu()` (`game.cpp:93-104`) `Disconnect`s then
`GoToState`s; the world unload only fires when the destination's entry block remembers to call it. A
new state that forgets `UnloadGame()` would silently leak the live `World` into the next screen. Mode
exit and sim teardown are coupled by discipline, not structure.

### 2.3 The single never-recreated `World` + hand-rolled reset — **grown, latent-bug surface**

`World` is constructed once with `Game` (`game.h:99`, `main.cpp:244`) and destroyed only at process
exit (`world/world.cpp:57-76`). Per-match teardown is `UnloadGame()` (`game_session.cpp:42-68`): a
manual list — `SwitchToLocalAuthorityMode`, `map.Unload`, reset camera/messaging/winningteamid/
matchEndCalled, `DestroyAllObjects`, recreate `gameMode`, clear chat, … Match-scoped correctness
depends entirely on every field being remembered here; anything added to `World` and forgotten here
leaks into the next match. (`gameMode` is also separately `delete`d in `tick_ingame.cpp:83`, a
double-ownership smell.) The clean shape — a fresh `Match`/`World` object per match, RAII teardown —
is structurally unavailable because `World` is a by-value member.

### 2.4 The client/server role is a runtime bool tested by scattered `if`s — **grown**

The same binary is client or dedicated server, branched on two runtime flags, not a polymorphic split:
- `DedicatedServer::active` (`net/dedicatedserver.h:17`) — "I am headless server."
- `World::mode` (`world.h:270`) — authority vs replica.

`if(world.dedicatedserver.active)` / `if(!… active)` guards are smeared across the codebase:
render gating at `game_loop.cpp:286,319,382`, lobby-status at `:401`, ambience at
`game_session.cpp:28`, the sim tick at `world/world.cpp:90`, events at `session/events.cpp:71`,
headless glue at `session/headless.cpp:20`, the renderer at `game/render/game_renderer.cpp:191`,
resource load takes it as a parameter (`game_init.cpp:253`). Separately, `WorldNetwork::DoNetwork()`
forks into `DoNetwork_Authority()` / `DoNetwork_Replica()` — two ~250-line near-duplicate `recvfrom`
switch loops (`world_network.cpp:44-54,494`), and `mode == AUTHORITY` checks recur through
`World::Tick`, `Disconnect`, `GameStateObject::Tick` (`game/state/gamestateobject.cpp:33`), and the
game modes. This is a server concept smeared across client code via boolean guards — the symptom of a
missing client/server (and authority/replica) boundary.

### 2.5 `Game` is a god object with bidirectional back-references — **grown**

`Game` (`game.h:23-141`) owns every subsystem by value: `World world`, `Renderer`/`GameRenderer`,
`GameInput`, `GameUiPipeline`, `GameSession`, `LobbyConnectFlow`, `Updater`, `ControlServer`,
`InputServer`, plus all the state-machine fields and loose flags (`paused`, `quitRequested`,
`creategameclicked`, `lobbyChatLog`, …). It declares `friend class` for Audio/GameRenderer/GameInput/
GameUiPipeline/GameSession (`game.h:32-36`); `GameSession` holds `Game &game` and mutates
`game.world.*` / `game.sharedstate` through it (`game_session.h:34`). A half-finished extraction is
visible: `Game` holds **reference members aliasing GameSession's fields** — `Uint32
&currentlobbygameid` and `bool &joininggame` (`game.h:110-111`) bound to `gameSession`'s refs in the
ctor — so the same state is read through two owners. `World` itself carries 30+ friend classes
(`world.h:201-217`) and UI toggles (`showplayerlist`/`showteamcolors`, `world.h:302`).

### 2.6 Cross-tier duplication — **mostly designed, one real de-sync**

This dimension is the cleanest of the C++ side. The lobby↔in-world↔client mirrors are deliberate and
one-directional:
- `State` (pregame/INGAME u8) and `ParkedAccountIDs` flow in-world → lobby via heartbeat, then lobby →
  client via `NewGame`/`DelGame` deltas; `ParkedAccountIDs` is explicitly *not* on the join wire
  (`protocol.go:218-239`). `Hostname`/`Port` are synthesized by the lobby because the in-world server
  can't know its public address.
- The client's `std::list<LobbyGame*>` is a pure mirror kept in sync by the add/upsert/delete stream
  (`lobby.cpp:249-285`).
- **The one genuine de-sync:** `LobbyGame.Players` is stamped to `1` at creation
  (`services/lobby/client.go:337`) and **never refreshed** — the heartbeat carries no player count,
  so the browser's advertised occupancy is stale. The authoritative `aliveMask` exists but is routed
  only to the admin event feed, not back into `Players`.

### 2.7 UI ↔ simulation — **clean; report the rare seams honestly**

The cppx UI does **not** bleed into simulation. No `.cppx`/`.hx` screen references `SDL_*`,
`::Renderer`, `Surface`, `world.`, or a raw `Game*`/`World*` outside comments. The in-game HUD — the
likeliest bleed point — composes from snapshot models (`use_player_status`/`use_hud`/`use_tech`/…);
world data is captured into those models by the composition root (`CaptureWorldSessionSnapshot`,
`game_ui_pipeline.cpp:1864`), never read live. Two minor seams, neither active bleed:
1. **`use_server()`** (`hooks/use_server.h`) is a sanctioned raw-`Game*` escape hatch, provided at
   `game_ui_pipeline.cpp:1759-1761`. It is namespaced/documented and **currently has zero callers** —
   but it is a live, mounted backdoor any `silencer::game_ui` consumer could use to bypass the
   intent-closure contract.
2. **`set_paused`** writes `game.paused` synchronously in the closure (`game_ui_pipeline.cpp:1265`),
   unlike the world-session intents that wrap effects in `queue_deferred_mutation` — a small
   consistency gap, harmless because it fires from event handlers.

---

## 3. Judgment against a clean target

A sensible separation for a game of this complexity (GoldSrc/Source-era shape):

- **Client-side screen/scene system** independent of the replicated simulation — a UI stack driven by
  local view state, talking to gameplay only through commands. ✅ **Silencer already has this** in the
  cppx layer: `AppRoot` + `ScreenStack` + phase projection + intent closures. This is the model.
- **Connection state as its own concern** — a `NetChannel`/`Connection` with its own lifecycle,
  distinct from "the world." ❌ **Missing.** Transport, lobby session, and world simulation are fused
  into one `World` instance (§2.1).
- **A `Match`/`World` created and destroyed per game** — RAII teardown, no stale state. ❌ **Missing.**
  One process-lifetime `World` reset by hand (§2.3).
- **A real client/server (and authority/replica) boundary** — ideally a shared sim core with thin
  client and server shells, not one binary branching on a bool. ❌ **Missing.** Two runtime bools and
  scattered `if`s (§2.4).
- **Authoritative lobby/in-world/client tiers with explicit, minimal mirrors.** ✅ **Largely clean**
  (§2.6), minus the stale `Players` count.

**Rating:**
- *Already sensible:* the cppx UI/screen separation (the best part of the codebase); the pure
  `session_phase` projection; the tier-authority split and one-way heartbeat mirrors.
- *Accidental coupling (grew this way):* SIM teardown bolted to screen-entry blocks; the `Game`
  god object with aliasing back-references; the half-extracted `currentlobbygameid`/`joininggame`
  refs; the stale `Players` count.
- *Actively structurally wrong (designed-in but the wrong design):* connection state fused into the
  simulation object; the never-recreated `World` + hand-rolled reset; the client/server and
  authority/replica roles as runtime bools with duplicated code paths.

The asymmetry is the story: the **new** layer (cppx, mid-migration) got the boundaries right; the
**old** C++ core never had them.

---

## 4. Recommendations (ranked)

Each: the blur it fixes · blast radius · incremental vs. big refactor.

### R1. Extract a `Connection`/`NetSession` object out of `World` — **biggest payoff**
Fixes §2.1 (the core blur). Move `WorldNetwork` transport state, `Lobby` TCP client, peer registry,
and the snapshot ring into a connection/session object that *owns* the channel; let `World` hold only
simulation. "Connected," "authed to lobby," and "in a match" become queryable on the connection, not
inferred from sim fields. **Blast radius: large** — touches `world.h`, all `world.network.*` /
`world.lobby.*` call sites, the UI models that read them (`lobby_ui_model.cpp`,
`game_ui_pipeline.cpp:1251`). **Needs a bigger refactor**; do it in stages (first lift `Lobby` out of
`World`, then transport). This is the one to plan deliberately.

### R2. Give a match an explicit lifecycle object with RAII teardown
Fixes §2.2 + §2.3. Replace the never-destroyed by-value `World` + `UnloadGame()` reset list with a
`Match` (or heap `World`) constructed on match enter and destroyed on exit, so leaving a match *is*
the teardown — no per-field reset to forget, no leak-into-next-match surface. **Blast radius: large**
(`Game` ownership of `World`, every `game.world.` access via accessor). **Bigger refactor**, but it
permanently kills an entire class of stale-state bugs. Pairs naturally with R1 (connection outlives a
match; match is recreated under it).

### R3. Introduce an authority/replica role abstraction; de-duplicate `DoNetwork`
Fixes §2.4 (the `mode == AUTHORITY` half). Replace the `bool World::mode` + scattered checks with a
role strategy (even just two classes behind an interface) and collapse the two ~250-line
`DoNetwork_Authority`/`_Replica` loops (`world_network.cpp:44-54,494`) onto one shared path with
role-specific hooks. **Blast radius: medium** (confined to `world/network/` plus the handful of
`IsAuthority()` callers). **Incremental** — can land independent of R1/R2.

### R4. Make match-exit explicit instead of destination-coincidental
Fixes §2.2 directly and cheaply, even before R2. Add an `OnExit`/teardown hook to the state machine
so leaving INGAME/SINGLEPLAYERGAME/TESTGAME/REPLAYGAME runs `UnloadGame()` once, instead of relying
on every menu-ish destination's `stateisnew` block to remember. **Blast radius: small**
(`game_loop.cpp` dispatcher + the four entry blocks that currently call `UnloadGame`). **Safe,
incremental** — a good first step that de-risks R2.

### R5. Fix the stale lobby `Players` count
Fixes the §2.6 de-sync. Carry a player count in the heartbeat and update `LobbyGame.Players` in
`OnHeartbeat`, or derive it from `aliveMask` which already arrives. **Blast radius: small** (heartbeat
encode in `dedicatedserver.cpp`, decode in `udp.go`, `hub.go` OnHeartbeat, a client version bump).
**Safe, incremental.** Low-effort correctness win.

### R6. Shrink the `Game` god object; remove the aliasing back-refs
Fixes §2.5. Finish the half-done extraction: drop the `Uint32 &currentlobbygameid` / `bool
&joininggame` reference members (`game.h:110-111`) and read through `gameSession` directly; reduce the
`friend` surface. **Blast radius: medium, mechanical.** **Incremental**, low risk, mostly renames.

### R7. Close the `use_server` backdoor (migration-hygiene)
Fixes §2.7.1 before it gets a caller. Either delete `use_server` entirely (zero callers today) or
gate it so it can't be reached from screen code. **Blast radius: tiny.** **Safe now.**

---

### What the cppx migration must bake in NOW

The migration is the leverage point — it's the one layer being rewritten, so it can set the boundary
the rest of the code should eventually meet:

1. **Keep the intent-closure contract absolute.** No screen ever gets a `Game*`/`World*`. Delete or
   fence `use_server` (R7) so the migration doesn't ship a sanctioned bypass that later code leans on.
2. **Make the UI consume a `Connection`/`Session` abstraction, not `world.network.state` /
   `world.lobby.state` directly.** Even a thin read-model interface now means R1 later is an
   implementation swap behind the hook, not a UI rewrite. Today `game_ui_pipeline.cpp:1251` and
   `lobby_ui_model.cpp:380` reach straight into `World` — that's the coupling to *not* cement.
3. **Route every UI→gameplay effect through the queued/deferred path.** Bring the synchronous
   `set_paused` and the `GoToState` session intents onto the same `queue_deferred_mutation` discipline
   the world-session intents already use (§2.7.2), so "UI mutates game state during build" never
   becomes a load-bearing pattern.
4. **Don't let the clean phase projection lull the refactor.** The phase reconciler makes the UI
   *look* decoupled from a tangled core. Resist treating "the UI is clean" as "state management is
   clean" — the work in R1–R3 is below the projection line and the migration won't touch it unless
   planned.
