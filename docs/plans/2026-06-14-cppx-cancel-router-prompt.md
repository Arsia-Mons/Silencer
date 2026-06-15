# Implementer prompt — UI-layer cancel/back router + PauseScreen (cppx)

You are implementing one cohesive change in the Silencer C++/SDL3 client
(`clients/silencer/`), on the already–checked-out branch `hv/cppx-migration-cc`.
**Do not branch, do not commit** — leave the working tree dirty for review. Work
only in `/Users/hv/repos/Silencer/.worktrees/cppx-migration-cc`.

## Why this exists (read this first)

Silencer's "back"/cancel affordance is inconsistent: ESC pops overlays but does
nothing on phase screens (the Lobby — the user's reported bug), controllers can't
cancel at all, and the in-match "pause" affordance (the origin "Hit Enter To Quit"
prompt) is implemented as an *old-implementation remnant* in the game/sim layer
(`game_input.cpp` raw-scancode `world.quitstate` machine + a HUD-baked prompt + a
pre-frame mutation block in the composition root).

The product owner's decisions, already made — **do not re-litigate, do not invent
alternatives**:

1. **All cancel/back POLICY lives in the UI layer** (`src/client/ui`), driven by a
   `use_cancel` hook. `game.cpp`/`game_loop`/`events.cpp` only plumb the cancel
   *edge*; they hold no navigation policy.
2. The in-match "pause" affordance is the **origin "Hit Enter To Quit"** behavior.
   It becomes a real `PauseScreen` `OverlayScreen` in the cppx/ScreenStack
   paradigm — the foundation for a future full pause menu (Resume/Options/Leave),
   which is a SEPARATE later milestone you are NOT building now.
3. **Fully unify and delete `world.quitstate`** — zero remnants. Both ESC/back AND
   the scripted mission-end (`END_MISSION` GAS action) route through `PauseScreen`.

This is a hooks-first, declarative refactor. Build it the way a 2026
React/shadcn-style retained UI is built: screens compose capability hooks and
return element trees; intents are closures installed by the composition root;
state changes are queued and drained after render; no screen reaches into SDL,
`Renderer`, `World`, or `Game`. Read `clients/silencer/src/client/ui/CLAUDE.md`
and `clients/silencer/src/game/CLAUDE.md` and follow them exactly.

## The seam (build this first)

A `use_cancel(handler)` hook, mirroring `use_navigation` — a UI-layer seam wired
directly to `ClientUi`, NOT installed by the game composition root. Each frame the
**top** screen registers a cancel handler during render; the central cancel pass
invokes it on the cancel edge, else falls back to today's default (pop the top
overlay).

- `NavigationProvider` already publishes per-screen `{client_ui, current_entry_id,
  is_top}` (`providers/navigation_provider.cpp:12-23`, installed per screen in
  `client_ui.cpp:51-55`). Reuse that context — `use_cancel` does NOT need a new
  provider.
- `ClientUi` (`app_shell/client_ui.{h,cpp}`): add a frame-scoped slot
  `{ bool present; UiScreenEntryId entry_id; std::function<void()> handler; }`,
  a `register_frame_cancel_handler(entry_id, handler)` method (last-writer-wins),
  clear it in `begin_frame` (`client_ui.cpp:33-37`), and rewrite `end_layout`
  (`client_ui.cpp:115-120`):

  ```
  if (!input.cancel_pressed) return;
  UiScreen* top = screens_.top();
  if (!top) return;
  if (slot.present && slot.entry_id == top->entry_id()) slot.handler();   // screen's cancel
  else if (top->kind() == ScreenKind::Overlay) queue_pop_current(top->entry_id());  // default
  ```

  Tagging by `entry_id` and honoring only the top screen's handler is what makes
  it correct when an overlay sits over `InGameScreen` (the overlay is top → its
  handler or the default pop wins; the base screen's stale registration is
  ignored). The handler is invoked at `end_layout` (after build, before
  `drain_deferred_mutations`); handlers must therefore only *queue* work (call
  `nav.push`, `chat.cancel()`, etc., which queue deferred mutations) — never touch
  the retained tree. Screens capture intent closures BY VALUE into the handler so
  they survive the arena reset at `tree end_frame` (`ui_pipeline.cpp:91`).
- `hooks/use_cancel.h`: declare `void use_cancel(std::function<void()> on_cancel);`.
  Implement alongside `use_navigation` (it reads the same `NavigationContext`).

## Input — make controller "back" and the `back` op feed the same edge

The cancel edge (`UiInputFrame::cancel_pressed/cancel_down/cancel_released`,
`ui/input.h:74-76`) is set today by: keyboard ESC (`events.cpp:109/121`), control
socket `escape` (`controldispatch.cpp` `InjectKeyOp`, ~`:125-135`). Keep those.
Add the two missing sources:

1. **Controller back** — `events.cpp` `SDL_EVENT_GAMEPAD_BUTTON_DOWN`
   (`:158-163`) today only feeds keybind capture. When `!IsCapturingKeybind()`,
   resolve the pressed button through the **active keymap's `Action::UiCancel`
   binding** (`gameInput.GetKeyMap()`, `keybinds.h` `Action::UiCancel`,
   `IsPressed`) — DO NOT hardcode `PAD:east`; `gamepad.json` stays authoritative.
   If it maps to `UiCancel`, set `cancel_pressed = cancel_down = true` (mirror the
   ESC line). Add an `SDL_EVENT_GAMEPAD_BUTTON_UP` case that sets `cancel_released`
   for the same binding, for parity with the keyboard up-edge (`:121`).
2. **`back` op / `Game::GoBack()`** — `Game::GoBack()` (`game_loop.cpp:696-702`)
   today does `GoToState(MAINMENU); return false;`. Re-point it to inject the
   cancel edge into the UI input frame (the same `cancel_pressed = cancel_down =
   true` the `escape` key path uses) and `return true`. The `back` control op
   (`controldispatch.cpp` ~`:598`, reports `went_back`) then routes through the
   one UI-layer router instead of its own `GoToState`.

Keep the keyboard, control-socket, and (new) gamepad/`back` cancel paths in parity.

## Screens — register cancel handlers

- **`PauseScreen`** (NEW): `screens/pause_screen.hx` + `.cppx`, an `OverlayScreen`
  modeled on `OptionsScreen` (`screens/options_screen.hx`, `options_screen.cppx`).
  It presents the origin "Hit Enter To Quit" affordance as an **auto-focused**
  primary action (`AppButton` with `default_focused = true` →
  `interaction.initial_focus`; `confirm_pressed` already routes to the focus
  runtime, `ui_pipeline.cpp:38`, so Enter — or a click — activates it). Its
  `onPress` leaves the match. **Leaving must reproduce the origin
  `GameSession::CheckForQuit` outcome** (`game_session.cpp:164-172` →
  `tick_ingame.cpp:303-313`: `world.Disconnect()` then `GoToState(LOBBY)` +
  rejoin channel if authenticated, else `GoToState(MAINMENU)`). Use the existing
  `use_session().leave_match` intent if `Game::LeaveJoinedGame` already does this;
  otherwise wire the intent to match. Dismiss (ESC/back again) is the cancel
  router's default overlay pop — `PauseScreen` registers no special cancel
  handler. Keep it minimal; the Resume/Options menu is the later milestone.
- **`InGameScreen`** (`screens/in_game_screen.cppx`, `InGameScreenView` ~`:847`):
  register the full in-match cancel chain via `use_cancel`, using EXISTING intents
  (never mutate `Player`/`World` flags):
  - `use_ingame_chat().active` → `.cancel()`;
  - else buy/tech open (`use_tech().buy_active || use_tech().tech_active`) → the
    existing buy/tech close intent;
  - else `use_navigation().push(std::make_unique<PauseScreen>())`.
  Also: when the projected **mission-over** flag (see below) rises, push
  `PauseScreen` once via `use_effect`/`use_ref` (rising-edge, not a per-frame
  imperative check). Delete `build_quit_prompt` + its call (`:829-839`, `:876`)
  and the `hud.quit_state` read.
- **Phase screens with a back**: `Lobby` reuses its existing `on_go_back`
  (`lobby_screen.cppx` ~`:1979`: if `show_create`, toggle it off; else
  `session.leave_to_menu`) — pass that closure to `use_cancel`. `Connecting`,
  `CharacterCreate`, and `PostMatch`/`MissionSummary` register
  `use_cancel(session.leave_to_menu)`. `MainMenu` registers nothing (no-op; the
  quit machine is out of scope).

## Delete the old dogma (no backwards-compat shims)

Remove every `world.quitstate` remnant and the game-layer cancel policy; route the
mission-end trigger through the projection instead:

- `game_input.cpp` `OnScancodeDown`/`OnScancodeUp`: delete the `if(sc ==
  game.quitscancode){…quitstate…}` blocks (`:207-219`, `:248-255`). Keep
  F1/F2/F4/F5/F9. If `game.quitscancode` becomes unused, remove its declaration
  and `game_init.cpp` assignment (mind the `OUYA` `#ifdef`).
- `game_session.cpp`: delete `CheckForQuit` (`:164-172`) + its declaration + the
  `if(gameSession.CheckForQuit())` block in `tick_ingame.cpp:303-314`, and the
  `quitstate = 0` reset (`:59`).
- `game_ui_pipeline.cpp`: delete the pre-frame cancel block (`:1663-1685` — the
  chat/buy/tech close moved into `InGameScreen`'s `use_cancel`), and the
  `confirm_quit` intent (`:1526-1531`) + the `WorldSessionValue::confirm_quit`
  field (`world_session_model.h:132`) if unused after `PauseScreen` uses
  `leave_match`.
- `world.h:181` `Uint8 quitstate;` + `world.cpp:29`: replace with a single
  projected mission-over signal (a `bool`). `ActionSystem.cpp:102` (`END_MISSION`)
  sets that flag instead of `quitstate`. **First verify** nothing consumes the
  win/lose 1-vs-2 distinction (grep shows only the prompt/`CheckForQuit`/summary
  read it; `winningteamid`→`MISSIONSUMMARY` is a separate path) — if something
  does, preserve it.
- Projection: `world_session_model.cpp:248` projects `quit_state`; replace with the
  mission-over bool into whichever hook `InGameScreen` reads for the auto-push.
  Remove `quit_state` from `world_session_provider.{h:105,cpp:150}` and
  `use_hud.h:54`.
- Summary/debug: `game_summary.h:41`, `game_loop.cpp:744`,
  `controldispatch.cpp:214` expose `quit_state` in the control-socket state JSON.
  Update these (and any `tests/cli-agent` reader of `quit_state`) — remove or
  re-expose the mission-over bool. Don't silently break an e2e reader.

`world.quitstate` is NOT serialized (`serializer.cpp` has no reference), so no wire
version bump is needed.

## Build & verify (compile success is NOT verification)

- Build only via `bash clients/silencer/build.sh` (never raw cmake/ninja). Iterate
  until it compiles AND the cppx transpile (`.cppx`/`.hx` → generated C++) is
  clean. C++ build time is minutes — expect it.
- Reproduce through the real runtime with the control socket. Source
  `tests/cli-agent/e2e/lib.sh`; use `start_silencer`, `bun clients/cli/index.ts
  --port <p> <op>` (ops: `state`, `inspect`, `ui_gallery`, `key --key escape`,
  `back`, `wait_frames`), `stop_silencer`. At minimum confirm and capture evidence
  for:
  - Overlay open → `key --key escape` pops it; and the `back` op pops it (proves
    the converged edge — the same edge a controller "back" sets).
  - **Lobby (the reported bug): `key --key escape` / `back` returns toward the
    main menu** (`leave_to_menu`).
  - In match: `escape`/`back` with chat or buy/tech open closes that first; with
    nothing open, pushes `PauseScreen`; `escape` again pops it; the `PauseScreen`
    primary action leaves the match.
- Add a numbered E2E scenario under `tests/cli-agent/e2e/` covering the back
  behavior across overlay + lobby + in-match, and run
  `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh` (it guards the UI
  ownership boundary and bans deleted-layer tokens).
- A physical gamepad can't be driven headless. The `back` control op injects the
  exact same `cancel_pressed` edge the gamepad `UiCancel` binding sets, so the
  router behavior is covered; state that explicitly and note the gamepad
  button→edge mapping (`events.cpp`) is covered by reasoning, not an e2e pad.

## Constraints

- Screens never touch SDL/`Renderer`/`World`/`Game` — only hook intent closures.
  Navigation/cancel policy lives in `src/client/ui`, never in the game layer.
- Don't add features, abstractions, config knobs, or error handling for impossible
  states beyond what's above. No backwards-compat shims — change the code and
  delete the old. If 200 lines could be 50, write 50.
- "Silencer"/SDL3 naming; when you touch a line referencing the legacy behavior,
  the codebase's term is **origin** (e.g. `origin InGameOverlays.cpp`), not
  "zSILENCER". Rename stale `zSILENCER`/SDL2 identifiers you touch.
- Do not reintroduce the deleted legacy UI layer or its tokens.

## Report (when done)

Lead with the outcome. Then: the seam shape you built, the files changed (grouped),
the deletions, and — for each verification bullet above — what you actually ran and
observed (quote control-socket output / state). Audit every "works" claim against a
tool result from your session; if something is unverified, say so. Do not commit.
