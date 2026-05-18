# Clay UI Architecture Completion — Design

Date: 2026-05-14
Audit basis: `docs/audits/2026-05-13-clay-architecture-audit.md` plus the
independent review on `hv/clay-ui-migration` at HEAD `e08d425`.

## Goal

Close the four real architectural gaps that remain after the Clay UI
migration, in a faithful way that does not preserve legacy coupling under new
file names. Where the audit was understated, do the bigger version, not the
smaller version.

Out of scope: any toolkit-boundary rename. Per user direction, `src/ui/`
primitives may carry Silencer-specific knowledge (banks, sprite fonts,
palettes). They are "our primitives." This is a deliberate design choice, not
an oversight; document it in `clients/silencer/CLAUDE.md` and move on.

## Moves

### Move 1 — `UiFrameContext`

**Problem.** Seven independent per-frame arenas live in anonymous-namespace
globals across `src/ui/primitives/*.cpp`
(`BankText`, `BankButton`, `Box`, `ScrollList`, `ScrollTextBox`, `Toggle`,
`TextInput`) plus an eighth HUD payload arena in
`src/client/ui/hud/InGameHud.cpp` (`HudPayloadBeginFrame`). Each exposes its
own `BeginFrame()` reset. `ClientUi::BeginFrame` calls seven of them; the
HUD calls its own reset from inside the HUD build, violating the dogma in
`clients/silencer/CLAUDE.md` that arenas reset only once in
`ClientUi::BeginFrame`. Unity builds break because each TU has its own
`g_payloads`/`g_customData` globals.

**Design.**

New module: `clients/silencer/src/ui/runtime/UiFrameContext.h/.cpp`.

```cpp
namespace silencer::ui {

struct UiFrameContext {
    UiFrameContext();
    ~UiFrameContext();

    // Resets every per-frame arena owned by the UI runtime in one call.
    // Called exactly once per Clay frame, before any primitive declaration.
    void BeginFrame();
};

}  // namespace silencer::ui
```

`UiFrameContext::BeginFrame()` invokes every per-arena reset function as
its implementation detail. The individual `BankTextBeginFrame()`,
`BoxBeginFrame()`, etc. functions remain present and exported so that
existing isolated unit tests under `src/render/clay_ui_tests/` keep working
without behavioral change; the production path stops calling them
individually.

`ClientUi` owns a `UiFrameContext` member and calls
`frameCtx_.BeginFrame()` at the top of `ClientUi::BeginFrame`. The seven
explicit calls in `ClientUi.cpp` go away. `HudPayloadBeginFrame()` moves
out of `InGameHud.cpp` into `UiFrameContext` (HUD payload arenas join the
roster), and the inline call inside `BuildInGameHudUi` is removed.

Stale unused `using silencer::ui::primitives::BankTextBeginFrame;` lines in
screen/modal `.cpp` files are deleted in the same step.

**Verification.** `silencer_tests`, E2E 10/11/12/13/14/50/51/60, pixdiff of
main menu / lobby / options / in-game HUD vs baseline snapshots captured in
Step 0.

### Move 2 — World/Player read models

**Problem.** `world.h:171` declares `friend class
silencer::client_ui::InGameUiController`. HUD code (`InGameHud.cpp`,
`InGameOverlays.cpp`) `#include`s `world.h`, `player.h`, `team.h`,
`lobby.h`, `terminal.h`, `detonator.h`, `basedoor.h`, `buyableitem.h`,
`objecttypes.h`, `user.h`, `camera.h`. The UI layer is recompile-coupled to
the whole gameplay surface. The audit framed this as "while read models are
being separated" — they have not been separated.

**Design.**

New directory: `clients/silencer/src/client/ui/views/`.

```cpp
// HudView.h
namespace silencer::client_ui {

struct PlayerHudView {
    bool valid = false;
    int health = 0;
    int maxHealth = 0;
    int shield = 0;
    int maxShield = 0;
    int armor = 0;
    int credits = 0;
    int weaponSlot = 0;
    // ... only fields the HUD actually reads
};

struct TeamHudView {
    int id;
    std::string name;
    int score;
    int color;
};

struct MinimapView {
    // map size, viewport, marker list — only what minimap reads
};

struct HudView {
    PlayerHudView localPlayer;
    PlayerHudView viewedPlayer;
    std::vector<PlayerHudView> playerList;
    std::vector<TeamHudView>   teams;
    MinimapView                minimap;
    // ...
};

// Populated once per frame from World+Player+Lobby.
HudView BuildHudView(const World& world, const Lobby* lobby);

}  // namespace silencer::client_ui
```

`Game::RenderClientUiFrame` calls `BuildHudView` once before
`BuildVisibleClientUi` and passes the resulting `const HudView&` to the HUD
builder. `InGameHud` and `InGameOverlays` consume the view only and drop
their `world.h`/`player.h`/`team.h`/etc. includes. They keep sprite-bank
reads (still through `World::resources` or, if practical in this step, a
`SpriteBankView` ref) — sprite assets are renderer/UI shared and don't
warrant a deeper extraction here.

`friend class silencer::client_ui::InGameUiController;` is removed from
`world.h`. The `InGameUiController` either gets the access it needs through
the public surface or moves the small piece of behavior it needs onto
`World` as a public method. We will discover the minimal needed API by
removing the `friend` declaration and following the compiler errors;
nothing speculative.

**Out of scope for this move.** Sprite-bank access through `World::resources`
is intentionally left as-is. Splitting `World::resources` ownership out of
`World` is a much bigger move and not necessary to drop the friend grant or
to compile HUD without `world.h` (the view holds everything the HUD reads
*from gameplay state*; sprite reads can live behind a thin
`HudSpriteAccess` ref or stay in the renderer-side custom payload path).

**Verification.** Same gates. Particular attention to pixdiff of in-game HUD
and overlays at multiple viewport sizes — a stale field copy in the view
will show up there.

### Move 3 — Decompose `InGameHud.cpp` (1050 lines)

**Problem.** The audit said 408 lines and called the file a "raw Clay
layout." The file is 1050 lines and contains: HUD payload arena,
status/health/shield/armor bars, minimap rendering, player list (F1),
team emblem rendering, weapon slot icons, credits display, system-camera
inset, plus the build entry point.

**Design.**

Split `client/ui/hud/InGameHud.cpp` into focused TUs under the same
directory:

```
client/ui/hud/
  InGameHud.h/.cpp          // BuildInGameHudUi composition only
  hud_status_bars.h/.cpp    // health/shield/armor/fuel
  hud_minimap.h/.cpp        // minimap panel + markers
  hud_player_list.h/.cpp    // F1 overlay
  hud_team_emblems.h/.cpp   // team emblems
  hud_weapon_slots.h/.cpp   // weapon/buyable slot icons
  hud_credits.h/.cpp        // credits panel
  hud_system_camera.h/.cpp  // system-camera inset
```

Each sub-builder takes `const HudView&` and the small set of toolkit refs
it needs (renderer for measure helpers, surface for blits, automation
registry for stable IDs). Each owns its own internal helpers.

`InGameHud.cpp` reduces to the entry-point function plus the orchestration:
which sub-builders run, in what order, under which Clay container.

Target: no single HUD file over ~250 lines after the split. The composition
file should be under 100.

**Verification.** Same gates plus pixdiff of in-game HUD at 640×480 and
1280×720. Architecture-boundary test (`60_ui_architecture_boundaries.sh`)
must still pass — sub-builders live under `client/ui/hud/`, not under
`src/ui/`.

### Move 4 — Decompose remaining large raw-Clay files

Targets (current line counts):

- `screens/lobby/lobby_screen.cpp` (708)
- `screens/lobby/game_tech_panel.cpp` (515)
- `screens/options/options_controls_screen.cpp` (619)
- `hud/InGameOverlays.cpp` (392)

**Design.** Same shape as Move 3 — split each at natural domain boundaries.
Concrete proposal, refined when the implementation step starts and reads
the actual code:

- **`lobby_screen.cpp`**: lobby chrome (title bar, footer, version),
  lobby main area (panel switching), the lobby-level controller wiring.
- **`game_tech_panel.cpp`**: tech tree view, tech purchase row, selected
  panel detail.
- **`options_controls_screen.cpp`**: keybind list, axis row, button row,
  rebind capture UI.
- **`InGameOverlays.cpp`**: chat overlay, buy menu overlay, tech menu
  overlay. (Audit-noted target.)

Target: no single screen file over ~300 lines after the split.

**Verification.** Same gates. Each surface gets a focused pixdiff: lobby at
`640×480` and `1280×720`, options/controls at both sizes, in-game chat /
buy / tech overlays at the in-game viewport.

### Move 5 — Normalize text/key input through `UiInputState`

**Problem.** `UiInputState` carries pointer, wheel, gamepad nav, and now
text input — but text and key dispatch still flow through
`UiInteractionRegistry::dispatch` as compatibility hooks invoked from
screen callbacks. The architecture goal calls for one durable input
contract: events into `UiInputState`, typed actions out.

**Design.**

Extend `UiInputState`:

```cpp
struct UiKeyEvent {
    int sdlScancode;
    int sdlKeycode;
    int sdlModFlags;
    bool down;          // press or release
    bool repeat;
};

struct UiInputState {
    // ... existing fields ...
    std::vector<UiKeyEvent> keyEvents;  // already have textInput
};
```

`ClientUi::DispatchInput` (or a renamed peer in the runtime) walks
`keyEvents` + `textInput` and routes them through the focus dispatcher in
`UiInteractionRegistry`, emitting typed `UiAction`s exactly as the pointer
path does today. Screens stop implementing `OnTextInput`/`OnKey`
compatibility hooks; those interfaces come off `Screen`. Anything currently
calling them either reads from `UiInputState` directly (raw screens that
need scancode-level input — keybind capture in options/controls is the
main one) or consumes typed `UiAction`s.

**Verification.** Same gates with heightened focus on E2E 11 (keyboard nav)
and 13 (password modal text entry). Keybind capture in options/controls
gets manual smoke verification.

## Cross-cutting rules

1. **No backwards-compatibility shims.** When a function moves into
   `UiFrameContext`, callers update. When the friend declaration goes,
   nothing replaces it that re-grants the same access in a different
   shape. When a screen file splits, the old monolithic translation unit
   is deleted, not left as a thin re-export.
2. **Verification gates are mandatory between moves.** Each move's gate
   must pass on the working tree before the next move begins. No "I'll
   fix the failing test in the next step."
3. **Pixdiff baselines captured once, before Move 1.** Reused for every
   subsequent move. If a move legitimately changes visuals (it shouldn't —
   none of this is visual), the baseline is regenerated and the change
   noted in the move's commit.
4. **No new files under `clients/silencer/src/ui/` get Silencer-domain
   names.** New files outside that directory follow existing local
   conventions.
5. **Subagents own implementation, not synthesis.** Each move's subagent
   gets a self-contained prompt that names files, line numbers, and exit
   criteria. The orchestrator (me) verifies gate output and refuses to
   advance on partial completion.

## Verification gates (every move)

Mandatory:

1. `cmake --build build --target silencer silencer_tests -j 8` clean.
2. `build/tests/silencer_tests` exits 0.
3. `ctest --test-dir build --output-on-failure` exits 0.
4. `tests/cli-agent/e2e/10_navigate.sh`
5. `tests/cli-agent/e2e/11_keyboard_navigation.sh`
6. `tests/cli-agent/e2e/12_controls_scroll.sh`
7. `tests/cli-agent/e2e/13_password_modal.sh`
8. `tests/cli-agent/e2e/14_directional_navigation.sh`
9. `tests/cli-agent/e2e/50_resize_screenshot.sh`
10. `tests/cli-agent/e2e/51_ingame_ui_overlays.sh`
11. `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh`

Visual:

- `tools/pixdiff` of each affected surface against the baseline snapshot
  captured before Move 1. Max-pixel-delta and mean-color-delta must stay
  within the existing visual-parity tolerances encoded in the pixdiff and
  CLI screenshot harnesses.

## Acceptance

- `clients/silencer/src/ui/runtime/UiFrameContext.h/.cpp` exists and is the
  only frame-arena reset entry point used by production.
- `world.h` no longer declares `friend class
  silencer::client_ui::InGameUiController`.
- `client/ui/hud/InGameHud.cpp` is ≤ 250 lines.
- `lobby_screen.cpp`, `game_tech_panel.cpp`, `options_controls_screen.cpp`,
  `InGameOverlays.cpp` are each ≤ 300 lines.
- `UiInputState` carries `keyEvents` and is the routed source for keyboard
  text/key dispatch in production.
- All gates above pass on the final tree.
- An independent auditor (Explore agent posing as a principal game
  engineer) reads the code and confirms each acceptance item without
  relying on the implementer's narrative.

## Anti-acceptance (the user explicitly warned against this)

- Renaming the old monolithic HUD file to `InGameHud_old.cpp` and leaving
  it referenced.
- Making the `friend` declaration go away by moving the offending code
  inside the friend boundary, or by re-granting equivalent access through
  a new accessor that exists only for the HUD.
- Splitting a 1050-line file into a 100-line composition file plus a
  950-line "helpers" file. Real decomposition with real cohesion.
- Adding TODOs or `// FIXME` for things in scope.
- Pretending tests passed without showing the command output.
