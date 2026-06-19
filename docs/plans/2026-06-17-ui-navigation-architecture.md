# Silencer client navigation: the Quake three-axis model

**Status:** Draft / proposal · **Date:** 2026-06-17 · **Scope:** `clients/silencer`

## TL;DR

Silencer's top-level control flow is one god-enum (`GameState`) that a
`switch` in `Game::Tick` runs on. It fuses three things that classic engines
(Quake, Half-Life) deliberately kept as **separate axes**: the network
**connection status**, the **UI screen stack**, and whether the **world sim is
live**. "Phase" (`SessionPhase`) isn't a second design — it's a read-projection
the cppx migration added to recover the connection axis back out of the god-enum
for the new UI.

Target: fully decoupled axes.

1. **Two independent connection states**, not one ladder — Silencer connects to
   **two** servers: the **lobby** (TCP: matchmaking/chat/roster, `world.lobby.state`)
   and the **dedicated game server** (UDP: the actual match, `world.network.state`
   /`world.gameplaystate`). These are orthogonal — authed to the lobby while
   joining/playing a server, holding the lobby link through a match, or dropping
   it mid-game. Both already exist in the world layer.
2. **A screen stack you navigate — DECOUPLED from the connections.** The current
   screen is NOT a function of connection state (that coupling is the godbag's
   actual sin). The menu can be up over a live match; you stay connected to the
   lobby while sitting on a menu screen. Screens *read* the link states to fill
   themselves in and connection events *trigger* navigations (push/replace), but
   the links never *select* the current screen. Overlays (options, pause, buy,
   tech, chat) push on top.
3. **Sim-live** — one bit derived from the game-server link being in-match: draw
   the world behind the stack and run the tick.

> ⚠️ Correction (owner feedback): an earlier draft made "base screen = f(single
> connection status)". That re-created the godbag coupling and falsely serialized
> the two independent connections into one ladder. The axes above are the
> corrected, fully-decoupled model: two link states + a free stack + sim-live.

"Phase"/`GameState` is the redundant THIRD thing that flattens both link states
*and* the screen identity into one enum. The fix is to delete it in favor of the
two already-existing link states + a navigated stack. Slice 1 (delete the dead
`OPTIONS*` states) is already shipped; the rest drives toward this.

---

## 1. Problem (owner)

> Our Silencer client has a dirty architecture: it has both "phases" and "screen
> stacks". When popping an item off the stack it'll go back to a previous phase.
> I think phase is a relic from an old godbag/massive switch statement for state,
> when really we should be doing a modern game UI paradigm.

Correct, with one factual sharpening: popping an *overlay* never touches phase.
The only thing that rewinds a phase on "back" is `leave_to_previous`, a UI
closure that calls `GoToState(LOBBY|MAINMENU)` off a hidden `sessionPhasePrevious_`
latch in `GameUiPipeline` (`game_ui_pipeline.cpp:1330`). So the smell is "a UI
back intent reaches into the game FSM," not "the stack pops into a phase."

---

## 2. How it works today

### 2.1 Three mechanisms, one of them legacy

- **The `GameState` switch** (`game_loop.cpp:515-661`) — the godbag. `Game::state`
  is a `Uint8`; transitions go `GoToState` → a `FADEOUT` pseudo-state → `nextstate`
  (`game_loop.cpp:676-689`). Drives the top-level "screens."
- **The `ScreenStack`** (`client/ui/app_shell/navigation/`) — entry 0 is the
  always-mounted `AppRoot`; overlays (options, pause, modals) push above it.
  The modern mechanism.
- **Screen-local `use_state`** — GameSelect↔GameCreate, Staging/Tech panel swaps.
  Also modern (`reachability.md:52-60`).

### 2.2 The same route lives in 3 files + a hidden 4th copy

Adding/changing a route is a double-dispatch
(`GameState` → `project_session_phase` → `kPhaseScreens`) across three files,
plus the `sessionPhaseCurrent_/Previous_` latches in `GameUiPipeline`
(`game_ui_pipeline.h:184-185`) that neither the FSM nor the stack can see.

### 2.3 The god-enum is three axes wearing one hat

After Slice 1, `GameState` is: `NONE, FADEOUT, MAINMENU, LOBBYCONNECT, LOBBY,
UPDATING, INGAME, MISSIONSUMMARY, SINGLEPLAYERGAME, HOSTGAME, JOINGAME,
REPLAYGAME, TESTGAME, CREATECHARACTER`. These mix:

- **Connection/match status** (the real lifecycle): `LOBBYCONNECT` (connecting),
  `LOBBY` (authenticated), `HOSTGAME`/`JOINGAME` (loading), `INGAME`/`TESTGAME`/
  `REPLAYGAME`/`SINGLEPLAYERGAME` (in match), `MISSIONSUMMARY` (post-match),
  `UPDATING` (updater). `MAINMENU` is really "disconnected."
- **A render pseudo-state**: `FADEOUT` — not a screen, exists only to run a
  palette fade between transitions (`game_loop.cpp:676`, `tick_misc.cpp:6`).
- **A boot sentinel**: `NONE`.
- **A screen masquerading as a state**: `CREATECHARACTER` — the form is pure UI;
  its only engine behavior is "block until the lobby roster grows"
  (`game_loop.cpp:587-608`), a connection sub-status.

### 2.4 Why MAINMENU and LOBBY are even different (the key realization)

At the UI level they're both "in the menus." They are separate *engine* states
purely because of **connection status**:

- `MAINMENU` entry: `world.Disconnect()`, `world.lobby.Disconnect()`,
  `UnloadGame()` — disconnected, clean slate (`game_loop.cpp:493-507`).
- `LOBBYCONNECT`: the async connect+authenticate handshake.
- `LOBBY` entry: `gameplaystate = INLOBBY`, drain chat, pump map downloads —
  connected + authenticated to the lobby server (`game_loop.cpp:526-586`).

So "main menu vs lobby" = "do I hold a live authenticated lobby connection." That
is connection-lifecycle truth, not screen identity — exactly Quake's `cls.state`.

---

## 3. The reference model: Quake / Half-Life

Both kept these as **separate axes**, never one enum:

1. **`cls.state`** (`cactive_t`) — connection lifecycle only: `disconnected` →
   `connecting` → `connected` → (signon) → active. The entire "am I in a game"
   truth. Not a UI concept.
2. **The UI is a layered composite drawn over the world, every frame.**
   `SCR_UpdateScreen` always runs and draws world (only if active) → HUD →
   console → menu. **The menu is a layer toggled on top of whatever's underneath
   — never a state the engine transitions *into*.** Open it mid-game and the
   world is still behind it.
3. **`key_dest`** — a separate axis for where input goes (`key_game`/`key_menu`/
   `key_console`).
4. The menu has its own page navigation (`m_state`) — its own little stack,
   scoped to the menu layer.

Half-Life/Source: same bones, the menu (GameUI/VGUI) is a **panel stack layered
over the engine**; HUD, loading, pause are all panels; "in a game" is a
connection bool (`engine->IsInGame()`).

Silencer's bug is fusing Quake's `cls.state` (connection) and `m_state` (which
menu) into one `GameState` god-enum, and making menus into top-level *engine
states* instead of *layers drawn over* the connection status.

---

## 4. Target for Silencer: the three axes

### Axis 1 — Connection / match status (the only real state machine)

A small enum (Quake's `cls.state`), owned by the net/session layer (lobby +
world), reporting genuine lifecycle:

```
Offline        (was MAINMENU)        — no lobby connection
Connecting     (was LOBBYCONNECT)    — async connect + auth handshake
InLobby        (was LOBBY)           — authenticated to the lobby server
Loading        (was HOST/JOINGAME)   — joining a game server, loading the map
InMatch        (was INGAME/TEST/REPLAY/SINGLEPLAYER) — match loaded + ticking
PostMatch      (was MISSIONSUMMARY)  — match ended, summary
Updating       (was UPDATING)        — self-updater (own sub-flow; see Q)
```

The UI **never writes** this; it reads it and *requests* transitions through
intents that the net/session layer applies. (`GoToState` is the transition
primitive today; under the model it becomes "request a status change" owned by
one router — see §5/Slice 4.)

### Axis 2 — The screen stack (always on, the whole UI)

- The `ScreenStack` is the entire UI, in menus and in-world alike. `AppRoot`
  (entry 0) renders the **base screen**; overlays push on top.
- **Base screen = a pure function of Axis 1** (today's `project_session_phase` +
  `kPhaseScreens`, kept — but now a clean total function with no `FADEOUT`/`NONE`
  special-cases). `InMatch` → the HUD; `Offline` → MainMenu; `Connecting` →
  ConnectScreen; etc.
- **`CharacterCreate` is a base screen, not a status** — shown when authenticated
  with no/managed characters (a sub-condition of `InLobby`), driven by a roster
  signal rather than a `GoToState`.
- **All overlays live on this one stack** — options, pause, modals, *and* the
  in-match buy/tech/chat/scoreboard overlays, which today run through a separate
  in-game-only mechanism (`ingame_ui_mode` + an in-game overlay seam) rather than
  the app stack. Unify them onto it.
- **"Back" is one thing: a stack/cancel operation** (`use_cancel`, default = pop
  the top overlay). A screen that wants to change the *connection* calls a status
  intent (e.g. "leave to menu" = disconnect); it does not reach a `GoToState`.
  The reconciler then renders whatever base screen the new status implies. No
  `sessionPhasePrevious_` latch.

### Axis 3 — Sim-live

One derived bit: `status == InMatch` (incl. single-player). When true, draw the
world behind the stack and run `world.Tick()`/`DoNetwork()`. The HUD is just the
base screen for that status; the world rendering is gated on this bit.

### What "phase" becomes

Nothing. It was Axis 1 projected out of the god-enum. With Axis 1 owned cleanly
and the base-screen a pure function of it, the parallel `SessionPhase` *enum*
survives only as the UI-side vocabulary at the `60_ui_architecture_boundaries`
boundary (the UI layer may not include game headers) — a trivial 1:1 map, not a
duplicated source of truth, and no latch.

---

## 5. What changes vs. today (the deltas)

1. **`FADEOUT` stops being a `GameState`.** The fade is a *transition*, not a
   gameplay state. Pull the transition out of the enum: `state` stays = the
   outgoing status during the fade-out, with a `transitioning`/`pendingStatus`
   pair, and the dispatch runs a transition step instead of a `case FADEOUT`.
   Same visible fade, no pseudo-state, no `fadefromstate`, no projection
   special-case. (Slice 3 — behavior-preserving.)
2. **The latch + `leave_to_previous` die.** Match-exit/back routing (the
   "authenticated → LOBBY else MAINMENU" policy re-derived in three places —
   `game.cpp:108-122`, `tick_ingame.cpp:304-318`, `game_ui_pipeline.cpp:1330`)
   centralizes into one status router. (Slice 4 — fixes the owner's symptom.)
3. **In-match overlays move onto the app `ScreenStack`** so there is genuinely
   one stack. (Slice 6 — intricate; verify on the live HUD.)
4. **Per-status world side-effects move to enter/exit hooks** out of the
   render-frame switch. (Slice 5.)
5. **`CREATECHARACTER` becomes a base screen** keyed on the roster signal, not a
   status. (Slice 5/optional.)

---

## 6. Migration slices

Each slice is independently shippable, leaves the tree green, and is verified on
the real runtime (control socket + screenshots), not compile alone. No
backwards-compat shims.

1. **Slice 1 — delete dead `OPTIONS*` states.** ✅ Shipped (`c9da7b10`).
2. **Slice 3 — remove `FADEOUT` from `GameState`** (behavior-preserving
   transition bookkeeping; §5.1). Verify every transition fade still plays
   (menu↔lobby, lobby↔match, match→summary) on the live render.
3. **Slice 4 — one status router; delete the latch + `leave_to_previous`**
   (§5.2). Verify back-out from character-create (fresh-account vs returning)
   and leave-match against a live lobby.
4. **Slice 5 — per-status side-effects → enter/exit hooks** (§5.4); optionally
   re-model `CREATECHARACTER` as a roster-driven screen (§5.5).
5. **Slice 6 — unify in-match overlays onto the app stack** (§5.3).

Slices are ordered by risk: 3 is behavior-preserving; 4 needs lobby verification;
6 is the most invasive. After 3+4 the god-enum is a clean connection/match-status
enum and the latch is gone — the core of the complaint. 5+6 are the polish that
makes "one stack, always on" literally true.

---

## 7. Open questions (owner)

1. **`UPDATING`** is reachable only via the control socket today
   (`controldispatch.cpp:351`); no production path calls `GoToState(UPDATING)`.
   The updater *feature* runs on its own `UpdaterPhase`. Is the `UPDATING` status
   a real lifecycle rung, or should the updater present purely as its own
   overlay/flow and the status value go away?
2. **In-match overlay unification (Slice 6)** is the most invasive change and the
   in-match HUD is hard to golden-verify headlessly. Worth it for "one stack," or
   leave the in-game overlay seam as a documented exception?
3. **`CharacterCreate` as a screen vs. status** — modeling it as
   "authenticated + roster signal" is cleaner but moves who owns the completion
   edge (currently the `LOBBY`/`CREATECHARACTER` tick). Do it in Slice 5, or keep
   it a status?
4. **Transition coordinator ownership** — when `FADEOUT` leaves the enum, the
   palette fade is driven by the transition bookkeeping in `game_loop`/the UI
   pipeline. Keep it in the game loop (Slice 3 does), or hoist to a dedicated
   transition controller the stack also defers to (needed if Slice 6 lands so
   overlay fades and status fades share one owner)?

---

## 8. Non-goals

- Not touching gameplay simulation, networking, or the world tick.
- Not redesigning the visual look (SIL-84 /
  `2026-06-01-cppx-design-parity-restore.md`) — structure only.
- No `GameState`→`SessionState` rename: `GameState` is a fine name for the
  game-side status enum, and `SessionPhase` stays as the UI-boundary vocabulary.
  The duplication being removed is the god-enum's *conflation* + the latch, not
  the names.
- No configurability/"flexibility" beyond removing the conflation.
