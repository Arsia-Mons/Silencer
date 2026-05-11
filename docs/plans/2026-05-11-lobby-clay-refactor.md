# Lobby → Clay refactor

**Status:** in progress (autonomous Ralph loop)
**Scope:** lobby is the *first consumer*. Primitives are designed to
serve the whole client UI (main menu, options, modals, mission summary,
future screens). Migrating the rest of the game's screens is out of
scope for this Ralph run — they keep their `Object`/`Interface` widget
system until separately migrated using the same primitives.

## Goal

Build a small set of **reusable, screen-agnostic Clay primitives** for
the Silencer client UI, then reimplement the lobby as the first
consumer. The lobby's 2440 LOC of imperative widget construction
(`Object`-subclass widgets, screen-pixel coords, polled `clicked`
flags, `AddObject` / `AddTabObject` lists, manual `Tick` syncs) is
replaced by Clay-based layouts: every frame the screen walks app state
and emits a `CLAY(...) { ... }` tree; Clay produces render commands;
an SDL3/Surface bridge dispatches them through the existing bank-text
and sprite-chrome rendering routines.

Constraint: **<5% byte difference** between before/after screenshots
of each of the four right-pane states (gameselect, gamecreate,
gamejoin, gametech) and the always-on left column.

## Primitive design rules (load-bearing)

Primitives live in `clients/silencer/src/ui/clay/primitives/` —
*outside* `screens/lobby/`. Each is a screen-agnostic header+impl pair
that emits a Clay subtree.

1. **No lobby vocabulary in any primitive.** A primitive that mentions
   "agency", "map list", "tech slot", or any lobby-specific concept is
   wrong. Primitives talk in terms of layout + content + behavior
   (`label`, `variant`, `onClick`, `items`, `selectedIndex`).
2. **State passed in, mutations returned via callback.** Primitives do
   not own state. The caller passes immutable inputs + a callback;
   primitive emits Clay + invokes the callback on interaction.
3. **No reference to `world`, `lobby`, `MapDownloader`, `Config`** from
   primitives. Domain glue lives in the screen, not the primitive.
4. **Variants are explicit enums, not strings.** `BankText::Heading`
   not `"heading"`. Compiler enforces the variant set.
5. **Composition over options.** Three near-identical primitives is
   better than one primitive with 17 bool flags.

## Primitives

| Primitive | Variants | Wraps |
|---|---|---|
| `BankText` | `Title` (bank 135, w11), `Heading` (134, w8), `Body` (133, w6), `BodySm` (133, w7); optional `colorRamp/effectColor/brightness` | `Overlay` text path |
| `BankButton` | `B156x21` (chrome), `BNONE` (inline text-only, e.g. Security cycler), `BCHECKBOX` (tech matrix) | `Button` |
| `Toggle` | sprite-faced radio in a `set` group | `Toggle` |
| `ScrollList` | base, items-with-DL-badge (for the map list) | `SelectBox` + `ScrollBar` |
| `ScrollTextBox` | base, `bottomtotop` | `TextBox` + `ScrollBar` |
| `TextInput` | base, `password`, `numbersonly` | `TextInput` |
| `LabelValueRow` | one form row | composition |
| `FormBorder` | 1-px stroke | Clay `.border` |
| `Panel` | left chrome (background-baked), right chrome (bank 7 idx 8) | composition |
| `RightPane` | swap-of-4 container | composition |

## Layout

```
Root (640×480, fullscreen background image)
├── TitleBar (Silencer / v.X / map-name / Go Back)
└── Body (LEFT_TO_RIGHT)
    ├── LeftCol (TOP_TO_BOTTOM)
    │   ├── CharacterPanel
    │   └── ChatPanel
    └── RightPane (one of)
        ├── GameSelectPanel
        ├── GameCreatePanel
        ├── GameJoinPanel
        └── GameTechPanel
```

Every panel and every primitive declares its own `Clay_ElementDeclaration`
inline. No retained `Object` tree, no `Build/Tick/Destroy` lifecycle,
no `interfaceId`s.

## Ownership of concerns

| Concern | Owner |
|---|---|
| Layout (positions, sizing, padding, alignment) | Clay (per-frame tree) |
| Hover / click dispatch | Clay (`Clay_OnHover`, `Clay_Hovered`) |
| Focus / tab order | Plain C++ state in `LobbyState` (no `AddTabObject`) |
| App state (selected agency, map list, chat scroll, …) | `LobbyState` struct, allocated on the screen, read by the layout function |
| Domain operations (`world.lobby.CreateGame`, `JoinGame`, etc.) | Unchanged |
| Rendering of render commands | New SDL3 Clay bridge (`render/clay_bridge.cpp`) dispatches into existing `Surface` / sprite-bank / button-chrome routines |
| Pixel-bank text metrics | New `MeasureBankText()` reads existing `resources.fontwidth[bank][char]` |

Per-frame loop:

1. `Clay_SetPointerState(pos, down)` from SDL mouse.
2. `Clay_BeginLayout()`.
3. `BuildLobbyTree(lobbyState, world)` — pure function of state.
4. `Clay_EndLayout()` → render commands.
5. `RenderClayCommands(surface, commands)` — bridge dispatches.

## Renderer bridge

Clay's render commands are dispatched as:

- `RECTANGLE`, `BORDER` → flat color fills on `Surface`.
- `IMAGE` → existing sprite blit (`imageData` = packed `bank<<16 | index`).
- `TEXT` → existing bank-text path; `fontId` = bank, `fontSize` = textwidth, text user-data carries `effectColor`, `brightness`, `colorRamp`.
- `CUSTOM` → for things that don't fit (e.g., button chrome variants), the `customData` is a tagged struct allocated from a per-frame arena.
- `SCISSOR_START`/`END` → push/pop `Surface` clip rect.

Render commands carry app-side `userData` allocated in a frame arena
(reset each frame, zero `malloc` on the hot path).

## Integration boundary

`LobbyClayScreen : public Screen` plugs into `Game::PushScreen` like any
other screen. Its `Tick` runs the Clay layout pass + dispatches render
commands. It owns a `LobbyState` struct directly; no `World::CreateObject`
calls for UI.

The existing `Screen` base class, `Game::PushScreen / GoBack`, and
`MessageModal` overlays stay. `LobbyScreen` (the legacy class) is
deleted; `Game::ReplaceScreen(new LobbyClayScreen())` replaces it at
the same call sites.

## Verification

The original plan (pixdiff each Clay capture against the immutable
`tests/lobby-clay/baselines/*.png` committed in P2) couldn't be reached:
the legacy lobby's animated chrome (agency-icon idle, chat indicators,
palette fade) makes a frozen baseline non-reproducible even by the
legacy capture script against itself — re-capturing at a different tick
phase shows ~25% legacy-vs-self in the chrome strip.

Actual gate: **Clay vs. fresh-legacy, same run.** Each panel test
(`tests/lobby-clay/<panel>/run.sh`) boots one lobby instance, then runs
two silencer clients sequentially against it — first legacy (no env
var), then Clay (`SILENCER_LOBBY_CLAY=1`) — drives both through the
same milestone-based wait, then pixdiffs the two captures over the
panel's crop rect.

Determinism trick: after the milestone wait, both clients run
`cli step --frames N` (typically 30) instead of `wait_frames`.
`wait_frames` is wall-clock-gated, and legacy vs. Clay render at
different costs, so the same wall-clock wait elapses different sim-tick
counts → different `fade_i` → different palette. `step --frames N`
advances the sim by EXACTLY N catch-up ticks regardless of render cost,
so both implementations land at the same tick phase.

- `tools/pixdiff/pixdiff a.png b.png` — prints byte-diff % (P1).
- Per-panel pass bar: **<5%** byte diff, Clay vs. fresh-legacy. P20
  final sweep: all 7 panels ≤0.63%; P15 GameCreatePanel highest at
  1.45% in its own iteration's sweep (ScrollList scrollbar render-path
  divergence accounts for the floor).
- `tests/lobby-clay/baselines/*.png` are kept as informational
  artifacts only — they show the expected ~20-37% per-panel diff
  against any live capture and are not the pass gate.
- Final pass: `tests/cli-agent/e2e/` regression suite still green.

## Ralph backlog

See `ralph/prd.json` for the full sequenced list. Sketch:

1. Vendor `clay.h` + CMake wiring.
2. `tools/pixdiff` (stb_image).
3. Capture baselines.
4. Clay → SDL3/Surface bridge.
5. `BankText`.
6. `BankButton` (3 variants).
7. `Toggle`.
8. `ScrollList`.
9. `ScrollTextBox`.
10. `TextInput`.
11. `LabelValueRow` + `FormBorder` + `Panel`.
12. `LobbyClayScreen` chrome (title bar, background, Go Back).
13. `CharacterPanel` Clay.
14. `ChatPanel` Clay.
15. `GameSelectPanel` Clay.
16. `GameCreatePanel` Clay.
17. `GameJoinPanel` Clay.
18. `GameTechPanel` Clay.
19. CLI `inspect` compatibility (Clay tree → widget list).
20. Delete legacy lobby files; final E2E pass.
21. Dogfood pass: build a tiny demo screen (`tools/clay-demo/`) that
    uses every primitive once *without touching lobby code*. Proves
    primitives are screen-agnostic.

Each iteration: one item, one commit, screenshot DM'd to the user.

## Future migrations (out of scope for this run)

Once the primitives land, migrating other screens (main menu, options,
mission summary, modals) is a separate Ralph run. The primitives must
support those screens without modification — that's why P21 dogfoods
them on a non-lobby surface.

## Next milestone — chrome via Clay primitives (separate effort)

Decision recorded 2026-05-11: after the byte-identical lobby is fully
done (P12–P21), a **separate** milestone replaces the baked 640×480
background sprite's structural rectangles with Clay-drawn chrome. The
goal is reusable, future-friendly panel chrome that any screen can
compose without baking new sprites.

Shape:

- **Drop the decorative texture** baked inside the BG (circuit
  boards, planet monitor, photo collage). Panels become flat-colored
  interiors with stroked borders.
- **New `Rectangle` primitive** with two main controls:
  - **Stroke** — color matched as closely as possible to the legacy
    BG's bright-green panel borders (sample the palette index from
    the BG sprite at the stroke pixels). Width configurable.
  - **Background color with opacity** — so panels can render
    semi-transparent fills over whatever sits behind.
- **Non-rectangular container shape** — the chat-panel region in the
  legacy BG is an upside-down L (chat box extends below the character
  panel to form the L). The Rectangle primitive alone can't express
  this — open question for the milestone whether to:
  - compose two Rectangles that visually join,
  - introduce a `Polygon` / `LShape` primitive,
  - or rework the lobby so the chat container is a plain rectangle.
- **Defer the film-strip sphere lights** (top + bottom rows of green
  dots) — tackle in a follow-up after the rectangle primitives land.
- **Verification model changes.** Pixdiff-vs-legacy is no longer the
  gate (we're intentionally changing the visual). Replacement: layout
  correctness checks (containers land at expected positions/sizes at
  the target viewport) + visual eyeball + interaction checks already
  in place.
- **Out of scope for this milestone**: runtime resizing /
  web-responsive layouts. The renderer's framebuffer stays at
  640×480. Treat full responsiveness as yet another later milestone.
