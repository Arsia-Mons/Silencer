Design and implement the correct greenfield Clay UI architecture for Silencer, grounded in the game/world requirements rather than the legacy pre-Clay UI.

Important framing:
- Do not preserve or imitate the existing pre-Clay UI architecture. Treat it only as evidence of workflows, domain actions, and state transitions. It was probably implemented before proper knowledge about correct Clay usage was obtained so do NOT use it to steer your own implementation.
- Do not treat the current Clay implementation as authoritative for visual layout, button styling, positioning, or screen composition. It has major regressions: original main-menu button style was lost, button positions are wrong, lobby layout has serious issues, and nearly every screen may have visual or interaction regressions.
- When canonical UI evidence is needed, compare against the pre-regression/main-branch UI and use diffs/screenshots to identify intended behavior. Current Clay code is useful as implementation evidence only, not as the source of truth for visual correctness.
- Do not assume the game is fixed to 640x480. That is a legacy implementation artifact. The greenfield target is a modern-resolution game UI.
- Do not assume the whole game simulation runs at 24Hz. Verify timing carefully. Sprite animations may use that cadence, but gameplay/world/network timing may differ.
- Use Clay as the primary UI system, not as a sidecar renderer or compatibility layer around legacy widgets.

First, audit and clarify the true game/world requirements:
- Determine actual simulation timing, render timing, animation timing, and network tick/snapshot timing.
- Identify which timing behaviors are hard requirements, current artifacts, or product decisions.
- Preserve the semantic per-tick gameplay input contract, including edge behavior.
- Preserve multiplayer safety: authority/server-owned world simulation, clients sending inputs, replicas consuming snapshots/deltas, and no client-side direct mutation of replicated world state.
- Preserve lobby/session flows: auth, game list, create, join, spectate, rejoin, ready state, map download/upload gates, dedicated server spawn/heartbeat, progress/failure states.
- Preserve real modes: dedicated server, headless/control, TUI if still product-relevant, spectator, replay, mission summary, single player/test flows.
- Preserve runtime data requirements: maps/assets/GAS/actordefs/behavior trees/admin/lobby/shared-data contracts.
- Preserve automation requirements: inspect/click/set-text/screenshot/world-state/pause/step style control surfaces must remain first-class, but do not preserve the old inspector implementation unless it is the right design.

Target architecture:
- Introduce a central `UiSystem` that owns Clay initialization, per-frame Clay lifecycle, focus, UI input state, action queue, accessibility/test metadata, and rendering backend.
- Clay lifecycle should happen centrally once per rendered UI frame:
  1. collect normalized UI input
  2. set layout dimensions from the current modern drawable/logical UI size
  3. set pointer state continuously
  4. pass scroll delta and frame delta
  5. begin layout
  6. screens/controllers declare UI
  7. end layout
  8. drain UI intents through domain controllers
  9. render commands
- No per-screen SDL polling.
- No artificial pointer reset that breaks pressed/held/released semantics.
- No duplicate click dispatch paths for the same widget.

Input architecture:
- Split input into:
  - `GameplayInput`: ticked, semantic, network-serializable, used by world simulation.
  - `UiInput`: pointer, text, nav, confirm/cancel, scroll, focus commands.
- Normalize device input once from SDL/TUI/control/automation into these two streams.
- Define explicit capture rules:
  - focused text input suppresses conflicting gameplay actions
  - modal/dialog owns top-level UI input
  - in-game HUD/station/chat UI masks only conflicting gameplay actions
  - spectator/replay controls have explicit policy
- Gamepad navigation should use semantic actions, not raw device assumptions.
- Rebinding must preserve keyboard, mouse, gamepad buttons, and gamepad axes.

Mutation rules:
- Clay callbacks must not directly mutate game/world state during layout.
- Clay widgets emit typed UI intents/action objects.
- Screen/domain controllers drain those intents after layout and call the correct domain APIs.
- Multiplayer-affecting UI must route through lobby messages, network messages, authority-approved requests, or gameplay input as appropriate.
- Never directly mutate replicated world objects from client UI.

Rendering architecture:
- Do not constrain the new UI to the old 640x480 paletted surface.
- Build for modern drawable/logical resolution and responsive layout.
- Keep world-space/camera coordinates separate from UI-space coordinates.
- Preserve the game’s visual requirements where they are product requirements: sprites, palette/effect-color/brightness equivalents, bank-font or replacement typography policy, lighting/visibility expectations, and readable HUD overlays.
- Decide whether the renderer remains paletted, hybrid, or fully modern as an explicit architecture/product decision. Do not inherit the old CPU surface path by default just because it exists.
- Clay renderer backend should support textured sprites, text, rectangles, borders, scissor/clipping, images, and custom game primitives needed by UI/HUD.

Screen/workflow architecture:
- Replace legacy widget/object UI with Clay-native screens and components.
- Follow sane modern UI architecture: first inventory the canonical UI, identify repeated visual/interaction patterns, extract reusable responsive base primitives, then compose those primitives into views/screens/pages.
- Build reusable responsive primitives for buttons, panels, lists, text inputs, toggles, modals, tab/segmented navigation, scroll regions, status/progress states, HUD overlays, and game-specific visual treatments. Avoid one-off screen-local layout code unless the screen is genuinely unique.
- Preserve canonical visual intent where it is still desired, but modernize layout responsiveness, spacing, scaling, focus behavior, accessibility/test metadata, and input semantics rather than copying fixed coordinates.
- Keep a clean screen/modal/overlay flow because the game has real workflows that need it, but do not blindly preserve the old `Screen` API if a better greenfield shell is warranted.
- Domain controllers own workflows such as lobby connect, game select/create/join/tech/ready, progress modals, password prompts, options, mission summary, replay/spectator overlays, and in-game station/chat/HUD behavior.
- UI primitives/components must remain domain-agnostic. They receive state and emit callbacks/intents; they do not know about `World`, `Lobby`, `Config`, or network internals.

Automation/accessibility:
- Build a first-class UI metadata tree for automation/testing/accessibility:
  - stable id
  - role
  - label
  - bounds
  - enabled/disabled
  - selected/focused state
  - value/text
  - available actions
- Control socket and tests should drive this metadata tree, not private widget internals.
- Screenshots and deterministic stepping must remain available for verification.
- Headless automation should not require SDL window input.

Deliverables:
- Produce an architecture document describing the greenfield UI system and its interaction with gameplay, networking, rendering, automation, and platform modes.
- Identify hard requirements vs current artifacts vs open product decisions.
- Produce a canonical UI inventory based on main/pre-regression behavior and current regression diffs, explicitly calling out what should be preserved, modernized, or discarded.
- Produce a reusable Clay primitive/component catalog before rebuilding screens, and use it to implement screens consistently.
- Implement the architecture incrementally in the codebase where feasible.
- Remove or bypass legacy pre-Clay UI architecture instead of layering compatibility shims over it.
- Add focused tests or control-socket verification for input routing, pointer/scroll behavior, focus, action dispatch, modal capture, lobby workflow actions, and automation metadata.
- Verify end-to-end through real game/client flows before claiming done.
