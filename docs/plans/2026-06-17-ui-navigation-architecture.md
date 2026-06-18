# Silencer client navigation architecture: dissolving the `GameState` godbag

**Status:** Draft / proposal · **Date:** 2026-06-17 · **Scope:** `clients/silencer`

## TL;DR

The client today runs **three independent navigation mechanisms at once** and
scatters the same top-level route definition across **three files plus a third,
hidden copy of "where am I" state**. The `GameState` switch in `Game::Tick` is
the original zSILENCER godbag; it conflates three unrelated responsibilities.
"Phase" (`SessionPhase`) is **not** the relic — it is the thin read-projection
the cppx migration (SIL-14) bolted on to wrap the godbag so the new declarative
UI wouldn't reach into it.

The fix is the inverse of the obvious one: don't delete phase and keep
`GameState`. **Dissolve the godbag into a small, honest session-lifecycle state
machine, keep the declarative phase→screen reconciler the migration already
built, and make the screen stack the single owner of UI navigation.** The
codebase is ~70% of the way there already; the remaining work is deleting dead
states and consolidating session lifecycle, **not a rewrite**.

---

## 1. Problem

From the owner:

> Our Silencer client has a dirty architecture: it has both the concept of
> "phases" and "screen stacks". When popping an item off the stack it'll go back
> to a previous phase. I *think* phase is a relic from an old outdated
> godbag/massive switch statement for state, when really we should be doing a
> modern game UI paradigm.

The instinct is right; one detail of the mechanism is worth correcting (it
sharpens the fix — see §3). The dual-model smell is real and pinpointable.

---

## 2. How navigation works today

### 2.1 Three parallel mechanisms

The client navigates the UI in **three** unrelated ways simultaneously:

1. **The `GameState` switch (the godbag).** `Game::state` is a `Uint8` from an
   18-value enum (`src/game/state/game_state.h:6-27`). The per-frame dispatch is
   a single `switch(state)` in `Game::Tick`
   (`src/game/loop/game_loop.cpp:515-685`). Transitions go through `GoToState` →
   a `FADEOUT` pseudo-state → `nextstate` (`game_loop.cpp:700-713`). This drives
   the *top-level* "screens": main menu, lobby connect, lobby, character create,
   in-match, mission summary, options.

2. **The `ScreenStack` overlay stack.** `src/client/ui/app_shell/navigation/`.
   Entry 0 is the always-mounted `AppRoot`; tier-1 navigation (options, pause,
   modals) pushes `OverlayScreen`s *above* it. Mutations are queued and drained
   after render (`deferred_ui_mutation.h`). The **modern** mechanism.

3. **Screen-local `use_state`.** Sub-panels like GameSelect↔GameCreate and the
   StagingPanel/Tech swaps are **not** states or stack entries — they are
   React-style local component state inside one screen
   (`docs/plans/screens/reachability.md:52-60`, `lobby_screen.cppx`). Also
   modern.

So the client already *demonstrates* the modern paradigm (2 and 3) for
everything authored during the cppx migration — but the top-level spine (1) is
still the legacy godbag.

### 2.2 The same route is defined in 3 files + a hidden 3rd nav-state copy

Adding/changing one top-level route is a double-dispatch
(`GameState` → `SessionPhase` → screen factory) edited across three files kept
manually in sync:

| Table | File |
|---|---|
| `GameState` enum (+ `StateName` debug switch) | `game_state.h:6-27`, `game_loop.cpp:727` |
| `project_session_phase()` (GameState→SessionPhase) | `game/ui/session_phase.h:24-58` |
| `SessionPhase` enum + `kPhaseScreens[]` factory | `hooks/use_session.h:14`, `app_root.cpp:66-100` |

And "where am I / where did I come from" lives in a **third location** that
neither the state machine nor the stack can see: the `sessionPhaseCurrent_` /
`sessionPhasePrevious_` latches inside `GameUiPipeline`
(`game_ui_pipeline.h:184-185`, cached at `game_ui_pipeline.cpp:1316-1318`).
`game.state` has no memory of where it came from; the stack doesn't model
phases; so back-navigation memory is stashed in the composition root.

### 2.3 The `GameState` enum, categorized

The 18 values split into **three different kinds of thing**:

**(a) Genuine session / match lifecycle** — async, event-driven, world/network
side effects. These belong in *some* lifecycle model:

- `LOBBYCONNECT` — async connect/auth FSM (`lobbyConnectFlow.Advance`,
  `game_loop.cpp:532-549`).
- `LOBBY` — authenticated; pumps chat, map downloads, game-join
  (`game_loop.cpp:550-610`).
- `UPDATING` — self-updater (`game_loop.cpp:633-643`). *(borderline — see §7.)*
- `HOSTGAME` / `JOINGAME` — connect-and-load (`tick_hostjoin.cpp`); project to
  `Loading`.
- `INGAME` / `TESTGAME` / `REPLAYGAME` — live match variants; project to
  `InMatch`.
- `SINGLEPLAYERGAME` — tutorial match (`tick_singleplayer.cpp`).
- `MISSIONSUMMARY` — post-match teardown, gated on lobby auth
  (`tick_ingame.cpp:303`).
- `CREATECHARACTER` — *borderline*: the form is pure UI, but it blocks on lobby
  roster growth (a network event), routing to `LOBBY`
  (`game_loop.cpp:611-632`). See §7.

**(b) Pure-UI navigation — the relic.** Vestigial: each case does nothing but
`world.DestroyAllObjects()` (`game_loop.cpp:657-680`), the projection collapses
all four to `MainMenu` (`session_phase.h:50-56`), and the real Options UI is
already a **stack overlay** with no `GoToState` (`options_screen.hx:9`,
`reachability.md:50`). **Verified dead: zero `GoToState(OPTIONS*)` and zero
`state = OPTIONS*` anywhere in `src/`.**

- `OPTIONS`, `OPTIONSCONTROLS`, `OPTIONSDISPLAY`, `OPTIONSAUDIO`.

**(c) Meta / transient.** `NONE` (boot sentinel, projects to `MainMenu`) and
`FADEOUT` (a transition-visual flag, not a screen — see §2.4).

`MAINMENU` itself is mostly pure-UI: its only non-UI side effect is **teardown
on arrival from a match** (disconnect/unload/destroy, `game_loop.cpp:517-531`).
That teardown belongs to the *match-exit transition*, not to a "menu state."

### 2.4 The coupling seams (where the two models fight)

- **Route definition spread across 3 files + the latch** — §2.2. The projection
  layer is pure indirection that exists solely because the state machine and the
  screen model speak different vocabularies.
- **Two unrelated "back" axes.** Overlay back = `ScreenStack::pop_top` via the
  `use_cancel` router (`use_navigation.h:15-20`, `client_ui.cpp` cancel pass) —
  **never touches phase.** Phase back = `leave_to_previous`, a hardcoded
  `GoToState(prev==Lobby ? LOBBY : MAINMENU)` keyed off the `sessionPhasePrevious_`
  latch (`game_ui_pipeline.cpp:1330-1334`). A screen cannot express "go back a
  phase" through the UI layer — it must call an injected intent. *(This is the
  precise correction to the owner's "popping a screen goes back to a previous
  phase": popping an overlay does not; only `leave_to_previous` rewinds phase,
  and it's an intent shim, not a stack pop.)*
- **Every `Session` intent is a `GoToState` shim** — `play_online`,
  `start_tutorial`, `open_character_create`, `leave_to_menu`, `leave_to_previous`
  all just call `game.GoToState(...)` (`game_ui_pipeline.cpp:1325-1334`). The
  `Session` model is a façade over the godbag.
- **Match-exit routing duplicated in 3 places.** The policy "authenticated →
  LOBBY else MAINMENU" is independently re-derived in `LeaveMatchToMenu`
  (`game.cpp:108-122`), the `INGAME` connection-lost handler
  (`tick_ingame.cpp:304-318`), and `leave_to_previous`
  (`game_ui_pipeline.cpp:1330-1334`). No router owns it.
- **Fade has two authorities.** The FSM owns `FADEOUT` + the palette fade
  (`game_loop.cpp:686-691,700-713`); the stack has its *own* transition-fade
  system (`resolve_fade`, `wants_transition_fade`, `FadeOverride`,
  `ui_screen.h`). They coordinate manually via an `if(game.GetState()==FADEOUT)`
  check in the composition root (`game_ui_pipeline.cpp:~2052`).
- **Pause is modeled three ways** — `game.paused` bool, `Session.paused`, and a
  `PauseScreen` overlay.

---

## 3. Verdict on "phase is a relic"

**Partly true — right about origin, right about the cure, slightly off on the
mechanism.**

- **Provenance (TRUE).** `GameState` was a single monolithic `switch(state)`
  inside a multi-thousand-line `game.cpp`. It was split into per-state
  `tick_*.cpp` files (`08f03cbd` "extract subsystem classes") and the cppx
  migration (`b2e4edb0`). SIL-14 then deliberately **removed screen-mounting**
  from the switch while *preserving it for world side effects*: `85116394`
  ("AppRoot + minimal use_session phase reconciler") added the reconciler, and
  `c3b9c6bf` ("cppx is the live UI; game.cpp mounts no screens"). So
  `SessionPhase` literally exists only to translate the surviving relic for the
  new UI. The UI half of the enum (`OPTIONS*`) is dead relic.
- **But not purely a relic (IMPORTANT).** ~half the values carry genuine
  session/match/world lifecycle (connect FSM, map load, `world.Disconnect`,
  `UnloadGame`, ambience, peer-ready gating) that has **no home in a screen
  stack**. Those must survive.
- **Mechanism correction.** Popping an overlay does **not** rewind phase; the
  two "back" axes share no state (§2.4). The only phase-rewind is
  `leave_to_previous` reading a latch. So the smell is "a UI back intent reaches
  into the game FSM," not "the stack pops into a phase."
- **"Modern paradigm" is the right call — and it's an evolution, not a rewrite.**
  `AppRoot` is already a declarative reconciler; overlays already use a real
  stack. The remaining work: delete dead UI states, collapse the duplicated
  vocabulary, centralize transitions, and give the fade one owner.

A real game **does** need both a UI navigation stack *and* a session/match
lifecycle — they are different axes (what the player sees vs. what the network
session is doing). The mess is not "stack + phases"; it is that the lifecycle
FSM is bloated with pure-UI states, mirrored by a redundant vocabulary,
split-brained on transitions/fade, and reached into by UI "back."

---

## 4. Target architecture (the modern paradigm)

**Hybrid: a small session/match state machine that *drives* a declarative UI
router + overlay stack.** Two owners, two axes.

> Why not a pure scene/pushdown stack (everything is a screen)? Because it has
> no clean home for "waiting for all peers to load" or "connection lost
> mid-match" — those are not navigation events. The lifecycle is genuinely
> event-driven backend state, so it stays a state machine.

### Owner 1 — Session/match lifecycle state machine

Rename out of the UI vocabulary (`GameState` → e.g. `SessionState` /
`MatchSession`). It owns **only** the network/world-lifecycle subset:

- `MenuIdle` (offline) · `Connecting` (LOBBYCONNECT) · `InLobby` (LOBBY) ·
  `Loading{host|join}` · `InMatch{multiplayer|tutorial|test|replay}` ·
  `PostMatch` (MISSIONSUMMARY teardown) · *(optionally `Updating`)*.
- It owns the side effects that today live in the tick switch: connect FSM, map
  load, `world.Disconnect`, `UnloadGame`, ambience, peer-ready gating — moved
  behind explicit `enter_*`/`exit_*` transition hooks rather than
  `if(stateisnew)` blocks inside a render-frame switch.
- **One transition router.** It absorbs `LeaveMatchToMenu`, the scattered
  `tick_*` exits, and `leave_to_previous`, so the "authenticated → LOBBY else
  MAINMENU" policy lives **once**. The `sessionPhasePrevious_` latch is deleted —
  the router computes its own destination.
- `FADEOUT`, `fadefromstate`, `nextstate`, `nextstateprocessed`, and the
  re-entrant `Tick()` self-pump disappear; a transition becomes a descriptor on
  the router (§5).

### Owner 2 — UI router + overlay stack (mostly already built)

- **`AppRoot` reconciler stays** — it maps session-state → base screen. This is
  today's projection, kept, but now projecting from a clean ~6-value lifecycle
  enum instead of an 18-value godbag, so the `OPTIONS*`/`NONE`/`FADEOUT`
  special-cases vanish.
- **`ScreenStack` is the single UI-navigation owner.** Overlays (Options + tabs,
  Pause, modals) stay on the stack exactly as now.
- **All pure-UI navigation moves to the stack/reconciler and out of the
  lifecycle enum.** Menu screens, options, the character-create *form*.
- **Sub-panels stay screen-local `use_state`** — already correct.
- **Back/cancel unifies on the stack's `use_cancel` router** (already correct).
  A cancel handler that wants to change the session calls a *session intent*
  (`request_leave_match()`), which mutates Owner 1 — it does **not** call
  `GoToState`. The reconciler renders whatever phase results; the stack never
  needs to know which phase it's returning to.

### Net effect

Delete the 4 dead `OPTIONS*` states + `FADEOUT` + `NONE` (~6 of 18 values gone,
with their dead tick cases), collapse the match variants, and you are left with
a ~6-value session machine whose every value has real network/world side
effects, a UI that is a pure declarative reaction, and one stack for overlays —
the modern "state machine drives a declarative UI router" paradigm. The
duplicated route vocabulary collapses; the 3-place exit routing and the hidden
latch collapse into the one router.

---

## 5. The hard part: transitions and the fade

The non-trivial coupling to unwind is `FADEOUT` (§2.4). Today the render fade is
welded to the gameplay state machine, and the UI's own transition-fade system
runs in parallel for stack pushes, coordinated by an
`if(game.GetState()==FADEOUT)` check.

Target: **one transition coordinator** that both the session router and the
stack defer to. A session phase change requests a transition the same way a
stack push does; the palette fade (`GameRenderer::ApplyPaletteFade`) follows
that coordinator's state, not a gameplay pseudo-state. This is the one place
that needs careful design + **golden-image verification** (fades are visually
load-bearing — see `game_loop.cpp:686-691`, `ui_screen.h:17-23`), so it is its
own migration slice with screenshot diffing.

> Open sub-question: must the world keep dimming under a phase change the way
> legacy retained world UI objects across `FADEOUT` (`game_loop.cpp:710-712`)? If
> yes, the coordinator must hold the outgoing screen mounted until black — the
> behavior the FADEOUT source-projection gives today — but owned by the UI
> transition system, not the FSM.

---

## 6. Migration plan (staged, no backwards-compat shims)

Per repo rules (no compat shims during refactors; verify end-to-end on the real
runtime; combat overengineering). Each slice is independently shippable and
leaves the tree green; riskiest coupling (fade) isolated; dead code first.

1. **Slice 1 — Delete the `OPTIONS*` relic states.** Remove the four enum
   values, their dead `switch` cases (`game_loop.cpp:657-680`), `StateName`
   entries, and `project_session_phase` cases. Provably safe (no transition into
   them). Smallest risk; proves the "already an overlay" claim end-to-end.

2. **Slice 2 — Collapse the duplicated vocabulary.** Decide the §7 "keep the
   projection?" question. Either way: strip `GameState` to lifecycle-only,
   delete `NONE`, rename to `SessionState`, and rewrite the ~30 `GoToState`
   callers to the new vocabulary. `kPhaseScreens` stays the screen table.

3. **Slice 3 — Centralize transitions; replace `GoToState`/`FADEOUT` with a
   session router + UI-owned fade.** The §5 work. Router absorbs
   `LeaveMatchToMenu` + tick exits + `leave_to_previous`; delete the latch,
   `FADEOUT`, `fadefromstate`, `nextstate`, `nextstateprocessed`, re-entrant
   `Tick`. **Gate on golden-image diffs of every transition** (menu↔lobby,
   lobby↔match, match→summary).

4. **Slice 4 — Cut UI "back" off the FSM.** Screen cancel handlers route through
   `use_cancel` → session intents; unify pause to a single owner.

5. **Slice 5 — Move per-phase world side effects to explicit enter/exit hooks**
   and (optionally) re-model `CREATECHARACTER` as a lobby screen observing a
   roster signal (§7). Polish; do last.

Slices 1–2 already remove the duplicated-vocabulary problem and most of the
smell. Slice 3 is where the real design risk (fades) lives. Each slice: issue →
branch → PR → squash (`docs/git-workflow.md`); update
`tests/cli-agent/e2e/60_ui_architecture_boundaries.sh` when ownership seams move.

---

## 7. Open questions for the owner

1. **Appetite / stopping point.** Full 5-slice refactor, or stop after Slices
   1–2 (kill dead states + collapse the duplicate vocabulary) and leave the
   fade/`GoToState` machinery for later? *(1–2 are low-risk and remove ~80% of
   the smell; 3 carries the fade risk.)*
2. **Is `UPDATING` live or also vestigial?** It's a placeholder with no exit
   logic, reachable only via the control socket. If the self-updater is real
   it's session-lifecycle; if abandoned, delete it alongside `OPTIONS*`.
3. **`CREATECHARACTER`: lifecycle state or lobby screen?** Its only non-UI
   behavior is "block until roster grows" off a lobby event. Modeling it as a
   screen that subscribes to a roster signal is cleaner but moves who owns the
   completion edge. *(Recommendation: keep it a phase through Slice 2; revisit in
   Slice 5.)*
4. **How aggressively to collapse the in-match variants** (INGAME/TEST/REPLAY/
   SINGLEPLAYER/HOST/JOIN)? They already project to only `InMatch`/`Loading`.
   Collapsing to `InMatch{kind}` + `Loading{kind}` simplifies the enum but
   pushes branching into match-setup code.
5. **Keep the `SessionPhase` projection layer, or fold it 1:1?** Keeping a thin
   projection preserves the UI↔game decoupling boundary that
   `60_ui_architecture_boundaries.sh` enforces (the UI layer must not include
   game headers). Folding removes indirection but couples the two. *(Recommendation:
   keep `SessionPhase` as the UI-facing enum and the projection as the single
   boundary crossing — but slimmed to a trivial near-1:1 map once the game-side
   enum is lifecycle-only.)*
6. **Who owns the transition fade** after `FADEOUT` leaves the enum — the UI
   pipeline, or a dedicated transition controller (§5)?

---

## 8. Non-goals

- Not touching gameplay simulation, networking, or the world tick.
- Not redesigning the *visual* look (that's SIL-84 /
  `2026-06-01-cppx-design-parity-restore.md`) — this is structure only.
- Not adding configurability/"flexibility" beyond removing the duplication.
