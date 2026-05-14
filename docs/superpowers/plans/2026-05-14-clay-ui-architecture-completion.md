# Clay UI Architecture Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the four real architectural gaps remaining in the Clay UI migration: consolidate per-frame arenas, extract World/Player read models (and drop the `friend` declaration), decompose the 1050-line `InGameHud.cpp` and the other large raw-Clay screen files, and normalize text/key input through `UiInputState`.

**Architecture:** Behavior-preserving refactor. Each "move" makes one specific change, then runs the full gate suite before the next move starts. Test feedback comes from the existing `silencer_tests` + E2E suite + pixdiff harness rather than new unit tests — the gates already cover the surfaces touched. New unit tests get added only where a new public seam is introduced (notably `UiFrameContext` and the `HudView` builder).

**Tech Stack:** C++14, SDL3, CMake, Clay (nicbarker/clay vendored), Bun-based CLI E2E harness, in-tree pixdiff tool.

**Spec:** `docs/superpowers/specs/2026-05-14-clay-ui-architecture-completion-design.md`

---

## Step 0 — Baseline Capture

Before any code change, capture a known-good baseline. Every subsequent move is verified against this.

**Files:**
- Create: `tools/refactor-baselines/2026-05-14/` (transient working dir, gitignored)

- [ ] **0.1 — Build a clean baseline**

```bash
cmake --build build --target silencer silencer_tests -j 8
```

Expected: build succeeds with the usual `sprintf` / inline-definition warnings, no errors.

- [ ] **0.2 — Run unit tests**

```bash
build/tests/silencer_tests
ctest --test-dir build --output-on-failure
```

Expected: both exit 0.

- [ ] **0.3 — Run the E2E suite**

```bash
for t in 10_navigate 11_keyboard_navigation 12_controls_scroll 13_password_modal 14_directional_navigation 50_resize_screenshot 51_ingame_ui_overlays 60_ui_architecture_boundaries; do
  echo "=== $t ==="; tests/cli-agent/e2e/${t}.sh
done
```

Expected: each script exits 0.

- [ ] **0.4 — Capture baseline screenshots for pixdiff**

```bash
mkdir -p tools/refactor-baselines/2026-05-14
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port)
PID=$(start_silencer "$PORT")
wait_alive "$PORT"
for surface in MAINMENU; do
  bun clients/cli/index.ts --port "$PORT" wait_for_state --state $surface --timeout-ms 15000
  bun clients/cli/index.ts --port "$PORT" screenshot --out tools/refactor-baselines/2026-05-14/${surface}_640x480.png
done
stop_silencer "$PID" "$PORT"
```

For lobby/options/in-game baselines, follow whichever flow `tests/lobby-ui/` and `tests/cli-agent/e2e/51_ingame_ui_overlays.sh` already use to reach those states. Reuse the harness — do not invent new state-reaching code.

Expected: PNGs exist and are visually correct screenshots of the targeted surfaces.

- [ ] **0.5 — Record the baseline commit SHA**

```bash
git rev-parse HEAD > tools/refactor-baselines/2026-05-14/baseline.sha
```

This makes the regeneration path trivial: `git checkout $(cat …baseline.sha)` and re-screenshot.

Step 0 produces no commits. It exists to anchor the gate.

---

## Move 1 — `UiFrameContext`

### File Structure (Move 1)

**Create:**
- `clients/silencer/src/ui/runtime/UiFrameContext.h` — public surface.
- `clients/silencer/src/ui/runtime/UiFrameContext.cpp` — implementation.

**Modify:**
- `clients/silencer/src/client/ui/ClientUi.h` — own a `UiFrameContext` member.
- `clients/silencer/src/client/ui/ClientUi.cpp` — call `frameCtx_.BeginFrame()`, drop the seven individual `*BeginFrame()` calls and their forward-decls.
- `clients/silencer/src/client/ui/hud/InGameHud.cpp` — remove `HudPayloadBeginFrame()` definition (move it into UiFrameContext), remove inline call in `BuildInGameHudUi`.
- `clients/silencer/CMakeLists.txt` — add new source file.
- `clients/silencer/src/client/ui/modals/message_modal.cpp`, `password_modal.cpp`, `screens/options/options_screen.cpp`, `options_display_screen.cpp`, `options_controls_screen.cpp`, `options_audio_screen.cpp`, `screens/main_menu/main_menu_screen.cpp`, `screens/update/update_screen.cpp`, `screens/lobby_connect/lobby_connect_screen.cpp` — delete stale `using silencer::ui::primitives::*BeginFrame;` declarations (none of them call the functions).

**Hud payload arenas (currently in `InGameHud.cpp`):**

```cpp
constexpr int kSpritePayloadCapacity = 512;
silencer::clay_bridge::SpritePayload g_spritePayloads[kSpritePayloadCapacity];
silencer::clay_bridge::ClayCustomData g_spriteCustomData[kSpritePayloadCapacity];
int g_spritePayloadCount = 0;

constexpr int kTeamEmblemPayloadCapacity = 64;
silencer::clay_bridge::TeamEmblemPayload g_teamEmblemPayloads[kTeamEmblemPayloadCapacity];
silencer::clay_bridge::ClayCustomData g_teamEmblemCustomData[kTeamEmblemPayloadCapacity];
int g_teamEmblemPayloadCount = 0;
```

Plus the `AllocSpriteCustomData` / `AllocTeamEmblemCustomData` helpers. These move into a new HUD payload arena module living *next to the HUD* (the audit's "client/ui keeps Silencer-specific knowledge" principle): the alloc helpers stay in `client/ui/hud/` but their `BeginFrame()` resets are exposed and called by `UiFrameContext::BeginFrame()`.

**Final layout:**
- `clients/silencer/src/client/ui/hud/HudPayloadArena.h/.cpp` (new) — the four globals + `HudPayloadBeginFrame()` + `AllocSpriteCustomData` + `AllocTeamEmblemCustomData`.
- `clients/silencer/src/client/ui/hud/InGameHud.cpp` (modified) — uses HudPayloadArena, no longer owns the arenas.
- `UiFrameContext::BeginFrame()` invokes 8 underlying resets: seven primitives + `HudPayloadBeginFrame`.

### Tasks (Move 1)

- [ ] **1.1 — Create the `UiFrameContext` header**

```cpp
// clients/silencer/src/ui/runtime/UiFrameContext.h
#pragma once

namespace silencer {
namespace ui {

// Owns the per-frame reset of every UI-runtime arena (BankText, BankButton,
// Box, ScrollList, ScrollTextBox, Toggle, TextInput) plus client-side HUD
// payload arenas. Production code calls BeginFrame() exactly once per Clay
// layout, before any primitive declaration. The individual *BeginFrame()
// functions remain exported so isolated primitive unit tests can keep
// driving them directly.
class UiFrameContext {
public:
	UiFrameContext();
	~UiFrameContext();

	void BeginFrame();
};

}  // namespace ui
}  // namespace silencer
```

- [ ] **1.2 — Create the `UiFrameContext` implementation**

```cpp
// clients/silencer/src/ui/runtime/UiFrameContext.cpp
#include "ui/runtime/UiFrameContext.h"

#include "ui/primitives/bank_button.h"
#include "ui/primitives/bank_text.h"
#include "ui/primitives/box.h"
#include "ui/primitives/scroll_list.h"
#include "ui/primitives/scroll_text_box.h"
#include "ui/primitives/text_input.h"
#include "ui/primitives/toggle.h"
#include "client/ui/hud/HudPayloadArena.h"

namespace silencer {
namespace ui {

UiFrameContext::UiFrameContext() = default;
UiFrameContext::~UiFrameContext() = default;

void UiFrameContext::BeginFrame() {
	silencer::ui::primitives::BankButtonBeginFrame();
	silencer::ui::primitives::BankTextBeginFrame();
	silencer::ui::primitives::BoxBeginFrame();
	silencer::ui::primitives::ScrollListBeginFrame();
	silencer::ui::primitives::ScrollTextBoxBeginFrame();
	silencer::ui::primitives::TextInputBeginFrame();
	silencer::ui::primitives::ToggleBeginFrame();
	silencer::client_ui::HudPayloadBeginFrame();
}

}  // namespace ui
}  // namespace silencer
```

- [ ] **1.3 — Extract `HudPayloadArena` from `InGameHud.cpp`**

Move the four payload globals plus `HudPayloadBeginFrame`, `AllocSpriteCustomData`, and `AllocTeamEmblemCustomData` from `clients/silencer/src/client/ui/hud/InGameHud.cpp` into a new pair of files:

```cpp
// clients/silencer/src/client/ui/hud/HudPayloadArena.h
#pragma once

#include "render/clay_ui_payloads.h"

namespace silencer {
namespace client_ui {

void HudPayloadBeginFrame();

silencer::clay_bridge::ClayCustomData* AllocSpriteCustomData(
	silencer::clay_bridge::SpritePayload payload);
silencer::clay_bridge::ClayCustomData* AllocTeamEmblemCustomData(
	silencer::clay_bridge::TeamEmblemPayload payload);

}  // namespace client_ui
}  // namespace silencer
```

```cpp
// clients/silencer/src/client/ui/hud/HudPayloadArena.cpp
#include "client/ui/hud/HudPayloadArena.h"

namespace silencer {
namespace client_ui {

namespace {
constexpr int kSpritePayloadCapacity = 512;
silencer::clay_bridge::SpritePayload   g_spritePayloads[kSpritePayloadCapacity];
silencer::clay_bridge::ClayCustomData  g_spriteCustomData[kSpritePayloadCapacity];
int g_spritePayloadCount = 0;

constexpr int kTeamEmblemPayloadCapacity = 64;
silencer::clay_bridge::TeamEmblemPayload g_teamEmblemPayloads[kTeamEmblemPayloadCapacity];
silencer::clay_bridge::ClayCustomData   g_teamEmblemCustomData[kTeamEmblemPayloadCapacity];
int g_teamEmblemPayloadCount = 0;
}  // namespace

void HudPayloadBeginFrame() {
	g_spritePayloadCount = 0;
	g_teamEmblemPayloadCount = 0;
}

silencer::clay_bridge::ClayCustomData* AllocSpriteCustomData(
	silencer::clay_bridge::SpritePayload payload) {
	if(g_spritePayloadCount >= kSpritePayloadCapacity) return nullptr;
	g_spritePayloads[g_spritePayloadCount] = payload;
	g_spriteCustomData[g_spritePayloadCount] = {
		silencer::clay_bridge::CustomKind::Sprite,
		&g_spritePayloads[g_spritePayloadCount],
	};
	return &g_spriteCustomData[g_spritePayloadCount++];
}

silencer::clay_bridge::ClayCustomData* AllocTeamEmblemCustomData(
	silencer::clay_bridge::TeamEmblemPayload payload) {
	if(g_teamEmblemPayloadCount >= kTeamEmblemPayloadCapacity) return nullptr;
	g_teamEmblemPayloads[g_teamEmblemPayloadCount] = payload;
	g_teamEmblemCustomData[g_teamEmblemPayloadCount] = {
		silencer::clay_bridge::CustomKind::TeamEmblem,
		&g_teamEmblemPayloads[g_teamEmblemPayloadCount],
	};
	return &g_teamEmblemCustomData[g_teamEmblemPayloadCount++];
}

}  // namespace client_ui
}  // namespace silencer
```

(The implementer must verify the exact `ClayCustomData` shape against the current `InGameHud.cpp` lines 55–95 — show the actual current code preserves the existing initializer pattern. If the current code uses different field names, mirror those.)

In `InGameHud.cpp`, delete the moved code and replace the `HudPayloadBeginFrame()` definition + inline call inside `BuildInGameHudUi` with `#include "client/ui/hud/HudPayloadArena.h"`. The call site at `InGameHud.cpp:862` gets deleted entirely — the reset is now `UiFrameContext`'s responsibility.

- [ ] **1.4 — Wire `UiFrameContext` into `ClientUi`**

Modify `clients/silencer/src/client/ui/ClientUi.h`:

```cpp
#include "ui/runtime/UiFrameContext.h"
// ...
class ClientUi {
	// ...
private:
	silencer::ui::UiFrameContext frameCtx_;
	silencer::ui::ClayService& clay_;
	silencer::ui::UiAutomationRegistry& automation_;
	ScreenStack screens_;
};
```

Modify `clients/silencer/src/client/ui/ClientUi.cpp`:

- Delete the seven forward-declared `void BankButtonBeginFrame(); ...` lines at the top.
- In `BeginFrame`, replace the seven explicit `primitives::*BeginFrame()` calls with one `frameCtx_.BeginFrame();` call at the top, *before* any ClayService work.

- [ ] **1.5 — Delete dead `using` declarations**

In each file below, remove every `using silencer::ui::primitives::*BeginFrame;` line — they are unused after Move 1:

- `clients/silencer/src/client/ui/modals/message_modal.cpp`
- `clients/silencer/src/client/ui/modals/password_modal.cpp`
- `clients/silencer/src/client/ui/screens/options/options_screen.cpp`
- `clients/silencer/src/client/ui/screens/options/options_display_screen.cpp`
- `clients/silencer/src/client/ui/screens/options/options_controls_screen.cpp`
- `clients/silencer/src/client/ui/screens/options/options_audio_screen.cpp`
- `clients/silencer/src/client/ui/screens/main_menu/main_menu_screen.cpp`
- `clients/silencer/src/client/ui/screens/update/update_screen.cpp`
- `clients/silencer/src/client/ui/screens/lobby_connect/lobby_connect_screen.cpp`

Use:

```bash
for f in clients/silencer/src/client/ui/modals/*.cpp clients/silencer/src/client/ui/screens/options/*.cpp clients/silencer/src/client/ui/screens/main_menu/*.cpp clients/silencer/src/client/ui/screens/update/*.cpp clients/silencer/src/client/ui/screens/lobby_connect/*.cpp; do
  grep -l "using silencer::ui::primitives::[A-Za-z]*BeginFrame" "$f"
done
```

Then `Edit` each file to delete the matching lines. Re-grep to confirm zero remaining hits in those directories.

- [ ] **1.6 — Update CMakeLists for new source files**

Add `src/ui/runtime/UiFrameContext.cpp` and `src/client/ui/hud/HudPayloadArena.cpp` to the appropriate target in `clients/silencer/CMakeLists.txt`. The existing pattern groups source files by directory; follow it.

- [ ] **1.7 — Build**

```bash
cmake --build build --target silencer silencer_tests -j 8
```

Expected: clean build, no new warnings beyond the existing baseline.

- [ ] **1.8 — Run gates**

```bash
build/tests/silencer_tests
ctest --test-dir build --output-on-failure
for t in 10_navigate 11_keyboard_navigation 12_controls_scroll 13_password_modal 14_directional_navigation 50_resize_screenshot 51_ingame_ui_overlays 60_ui_architecture_boundaries; do
  echo "=== $t ==="; tests/cli-agent/e2e/${t}.sh || { echo "FAILED: $t"; exit 1; }
done
```

Expected: each command exits 0. Save the run log.

- [ ] **1.9 — Pixdiff vs baseline**

```bash
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port)
PID=$(start_silencer "$PORT")
wait_alive "$PORT"
mkdir -p /tmp/refactor-after
bun clients/cli/index.ts --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
bun clients/cli/index.ts --port "$PORT" screenshot --out /tmp/refactor-after/MAINMENU_640x480.png
stop_silencer "$PID" "$PORT"

# Use the in-tree pixdiff tool from tools/pixdiff
tools/pixdiff/pixdiff tools/refactor-baselines/2026-05-14/MAINMENU_640x480.png /tmp/refactor-after/MAINMENU_640x480.png
```

Expected: zero pixel delta (this is a behavior-preserving refactor; arena reset order is unchanged).

Repeat for any other baselines captured in Step 0.

- [ ] **1.10 — Commit**

```bash
git add clients/silencer/src/ui/runtime/UiFrameContext.h clients/silencer/src/ui/runtime/UiFrameContext.cpp clients/silencer/src/client/ui/hud/HudPayloadArena.h clients/silencer/src/client/ui/hud/HudPayloadArena.cpp clients/silencer/src/client/ui/ClientUi.h clients/silencer/src/client/ui/ClientUi.cpp clients/silencer/src/client/ui/hud/InGameHud.cpp clients/silencer/CMakeLists.txt clients/silencer/src/client/ui/modals/*.cpp clients/silencer/src/client/ui/screens
git commit -m "$(cat <<'EOF'
Consolidate UI frame arenas behind UiFrameContext

Single per-frame reset entry point for all primitive and HUD payload arenas.
Extracted HudPayloadArena out of InGameHud.cpp so the HUD build no longer
resets its own arena mid-frame, satisfying the "arenas reset once in
ClientUi::BeginFrame" dogma from clients/silencer/CLAUDE.md.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Move 2 — World/Player Read Models

### File Structure (Move 2)

**Create:**
- `clients/silencer/src/client/ui/views/HudView.h` — view structs.
- `clients/silencer/src/client/ui/views/HudView.cpp` — `BuildHudView` populator.

**Modify:**
- `clients/silencer/src/world/world.h` — remove `friend class silencer::client_ui::InGameUiController;` (line 171). Track downstream compiler errors and address by promoting the minimal needed pieces to a public API.
- `clients/silencer/src/game/game.cpp` — call `BuildHudView` in `RenderClientUiFrame` before HUD build, thread the view through `BuildVisibleClientUi`.
- `clients/silencer/src/client/ui/ClientUi.h/.cpp` and `ScreenStack.h/.cpp` — accept the `HudView` and forward it to `Screen::BuildUi` callees as part of `ScreenContext`, OR introduce a focused `HudBuildContext` consumed by HUD/overlay builders only. Pick the approach that minimizes screen-side churn.
- `clients/silencer/src/client/ui/hud/InGameHud.cpp` — take `const HudView&`, drop `world.h`, `player.h`, `team.h`, `lobby.h`, `terminal.h`, `detonator.h`, `basedoor.h`, `buyableitem.h`, `objecttypes.h`, `user.h`, `camera.h` includes. Keep `surface.h`, `render/renderer.h`, `render/clay_ui_payloads.h`, `clay/clay.h`, `ui/primitives/*`, `ui/runtime/UiAutomationRegistry.h`, `client/ui/hud/HudPayloadArena.h`.
- `clients/silencer/src/client/ui/hud/InGameOverlays.cpp` — same treatment.
- `clients/silencer/src/client/ui/ingame/InGameUiController.h/.cpp` — re-route any code that used friend access. Use the public surface; if a piece of state truly has no public reader, add a narrow `World::Get<Thing>()` public method rather than re-granting friendship.

### Tasks (Move 2)

- [ ] **2.1 — Inventory current HUD/overlay reads**

```bash
grep -nE "(world|player|peer|team|lobby|peerlist|GetPeerPlayer|viewedpeerid|localpeerid)\." clients/silencer/src/client/ui/hud/InGameHud.cpp clients/silencer/src/client/ui/hud/InGameOverlays.cpp | sort -u
```

Record every field touched. The view structs in 2.2 must cover all of them; no field gets added speculatively.

- [ ] **2.2 — Define `HudView` shape**

Write `clients/silencer/src/client/ui/views/HudView.h` containing only the fields surfaced by the 2.1 inventory. Plausible starting shape:

```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace silencer {
namespace client_ui {

struct PlayerHudView {
	bool valid = false;
	std::uint16_t peerId = 0;
	int health = 0;
	int maxHealth = 0;
	int shield = 0;
	int maxShield = 0;
	int armor = 0;
	int maxArmor = 0;
	int fuel = 0;
	int maxFuel = 0;
	int credits = 0;
	int currentWeaponSlot = 0;
	int teamId = -1;
	std::string name;
	// extend per 2.1 inventory
};

struct TeamHudView {
	int id = -1;
	std::string name;
	int score = 0;
	int color = 0;
	int emblemSpriteBank = 0;
	int emblemSpriteIndex = 0;
};

struct PlayerListEntry {
	std::uint16_t peerId = 0;
	std::string name;
	int teamId = -1;
	int score = 0;
	int kills = 0;
	int deaths = 0;
	int latencyMs = 0;
	bool isLocal = false;
	bool isViewed = false;
};

struct MinimapMarker {
	int x = 0;
	int y = 0;
	int kind = 0;
	int color = 0;
};

struct MinimapView {
	int worldWidth = 0;
	int worldHeight = 0;
	int viewportX = 0;
	int viewportY = 0;
	int viewportW = 0;
	int viewportH = 0;
	std::vector<MinimapMarker> markers;
};

struct HudView {
	bool mapLoaded = false;
	PlayerHudView localPlayer;
	PlayerHudView viewedPlayer;
	std::vector<PlayerListEntry> playerList;
	std::vector<TeamHudView> teams;
	MinimapView minimap;
	// extend per 2.1 inventory — chat lines, buy menu state, tech tree state,
	// detonator countdown, terminal state, etc. Add only what 2.1 found.
};

}  // namespace client_ui
}  // namespace silencer
```

The implementer MUST cross-check this skeleton against the 2.1 inventory and add/remove fields to match real usage. **Adding fields the HUD doesn't read, or omitting fields it does, fails this step.**

- [ ] **2.3 — Implement `BuildHudView`**

`clients/silencer/src/client/ui/views/HudView.cpp` populates a `HudView` from `const World&` and `const Lobby*`. It is allowed (and expected) to use `friend`-free public access. If a needed read requires a private field, *add a public const accessor on `World`/`Player`/`Team`* — do not reach into private state from the populator.

- [ ] **2.4 — Thread the view into HUD/overlay builders**

Two options; pick the one with smaller diff:

**Option A:** Add `HudView* hudView` to `ScreenContext`, populated in `Game::RenderClientUiFrame` before `BuildVisibleClientUi`. HUD/overlay screens read it.

**Option B:** Add `void BuildInGameHud(const HudView&, …)` signature; the HUD `Screen::BuildUi` resolves the view from a context pointer it was handed at construction.

The Option A wiring requires touching `ScreenContext`'s definition (likely in `clients/silencer/src/client/ui/screens/screen.h`) and `Game::RenderClientUiFrame` only. Prefer it.

- [ ] **2.5 — Migrate `InGameHud.cpp` to use `HudView`**

Replace every `world.X` / `player->X` / `peer->X` read inside `InGameHud.cpp` with the equivalent `HudView` field. Delete the gameplay includes listed in the file structure above. Keep sprite-bank reads through whatever shape lets the file compile *without* `world.h`; if necessary, pass a `const SpriteBankRefs&` ref through the same context.

- [ ] **2.6 — Migrate `InGameOverlays.cpp` to use `HudView`**

Same treatment. The chat / buy menu / tech menu overlays read player credits, team membership, buyable item lists, tech tree state — all in the `HudView`.

- [ ] **2.7 — Remove the `friend` declaration**

Edit `clients/silencer/src/world/world.h:171` to delete the line:

```cpp
friend class silencer::client_ui::InGameUiController;
```

Rebuild:

```bash
cmake --build build --target silencer -j 8
```

For every compile error, do *one* of:

1. The accessed field is no longer needed (HUD got migrated): nothing to do.
2. The accessed field has a public reader: switch to that.
3. The accessed field is genuinely needed and private: add a narrow public `const`-correct getter on `World`/`Player`/etc. Name it for what it returns (`GetLocalPeerId()` not `GetWorldFieldForUi()`).

**Never** re-add a `friend` declaration. **Never** introduce a public field that just exposes a former private one in shape. **Never** add a getter named after the UI subsystem.

- [ ] **2.8 — Build and run gates**

```bash
cmake --build build --target silencer silencer_tests -j 8
build/tests/silencer_tests
ctest --test-dir build --output-on-failure
for t in 10_navigate 11_keyboard_navigation 12_controls_scroll 13_password_modal 14_directional_navigation 50_resize_screenshot 51_ingame_ui_overlays 60_ui_architecture_boundaries; do
  echo "=== $t ==="; tests/cli-agent/e2e/${t}.sh || { echo "FAILED: $t"; exit 1; }
done
```

Expected: every command exits 0.

- [ ] **2.9 — Pixdiff every captured baseline**

In-game HUD pixdiff is the strictest test here — any stale field copy will show. Verify each baseline matches.

- [ ] **2.10 — Verify the `friend` and includes are actually gone**

```bash
grep -n "friend class silencer::client_ui" clients/silencer/src/world/world.h && echo "FAIL: friend still present" || echo "OK: friend removed"
grep -nE "#include \"(world|player|team|lobby|terminal|detonator|basedoor|buyableitem|objecttypes|user|camera)\.h\"" clients/silencer/src/client/ui/hud/InGameHud.cpp clients/silencer/src/client/ui/hud/InGameOverlays.cpp && echo "FAIL: gameplay includes still present" || echo "OK: HUD/overlays decoupled"
```

Expected: both lines print OK.

- [ ] **2.11 — Commit**

```bash
git add -A
git commit -m "$(cat <<'EOF'
Extract HudView read model and drop World friend grant to client UI

InGameHud and InGameOverlays now consume a HudView populated once per frame
in Game::RenderClientUiFrame. The HUD includes no longer pull world.h,
player.h, or the rest of the gameplay surface; the friend declaration for
InGameUiController on World is gone.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Move 3 — Decompose `InGameHud.cpp`

### File Structure (Move 3)

**Create (under `clients/silencer/src/client/ui/hud/`):**

- `hud_status_bars.h/.cpp` — health/shield/armor/fuel bars for the viewed player.
- `hud_minimap.h/.cpp` — minimap panel + markers (uses `HudView::MinimapView`).
- `hud_player_list.h/.cpp` — F1 player list overlay.
- `hud_team_emblems.h/.cpp` — team emblem rendering.
- `hud_weapon_slots.h/.cpp` — current weapon + buyable slot icons.
- `hud_credits.h/.cpp` — credits panel.
- `hud_system_camera.h/.cpp` — system-camera inset (only the UI framing — the actual world inset draw stays in renderer).
- Additional split files as the actual code demands; the audit-noted reality may differ.

**Modify:**
- `InGameHud.h` — public signature stays `BuildInGameHudUi`; internals shrink.
- `InGameHud.cpp` — entry point + ordering only. Target ≤ 250 lines.
- `clients/silencer/CMakeLists.txt` — add new sources.

### Tasks (Move 3)

- [ ] **3.1 — Map the current `InGameHud.cpp`**

Read the full 1050-line file. Identify natural unit boundaries by visual section / functional concern. List them in a working note before splitting; the actual split may differ from the file-structure proposal above and should follow the code's natural seams.

- [ ] **3.2 — For each sub-builder unit, in turn:**

For unit `hud_status_bars` (then minimap, then player list, then team emblems, then weapon slots, then credits, then system camera):

  - [ ] **3.2.a — Create header**

    ```cpp
    // hud_status_bars.h
    #pragma once
    #include "client/ui/views/HudView.h"
    class Surface;
    namespace silencer { namespace ui {
      class UiAutomationRegistry;
    } }

    namespace silencer {
    namespace client_ui {

    void BuildHudStatusBars(const HudView& view,
                            silencer::ui::UiAutomationRegistry& automation,
                            Surface& dst);

    }  // namespace client_ui
    }  // namespace silencer
    ```

  - [ ] **3.2.b — Move the corresponding code block** from `InGameHud.cpp` into the new `.cpp` file, preserving exact behavior. File-internal helpers move into an anonymous namespace inside that TU.

  - [ ] **3.2.c — Update `InGameHud.cpp`** to call the new entry point from the composition function and delete the in-place code.

  - [ ] **3.2.d — Build incrementally:** `cmake --build build --target silencer -j 8`. Fix any compile error before continuing.

  - [ ] **3.2.e — Quick pixdiff** of the in-game HUD baseline. If it diverges, the move broke behavior — investigate before moving on.

- [ ] **3.3 — Verify final line counts**

```bash
wc -l clients/silencer/src/client/ui/hud/InGameHud.cpp clients/silencer/src/client/ui/hud/hud_*.cpp
```

`InGameHud.cpp` ≤ 250. No sub-file > 300.

- [ ] **3.4 — Update CMakeLists.txt**

Add every new `hud_*.cpp` to the silencer target.

- [ ] **3.5 — Run gates**

Full suite per Move 1 / Move 2.

- [ ] **3.6 — Pixdiff baselines at 640×480 and 1280×720**

In-game HUD must be visually identical at both resolutions.

- [ ] **3.7 — Commit**

```bash
git add -A
git commit -m "$(cat <<'EOF'
Decompose InGameHud.cpp into focused sub-builders

InGameHud.cpp shrinks from 1050 to <=250 lines; status bars, minimap,
player list, team emblems, weapon slots, credits, and system-camera inset
each live in their own translation unit. All consume the HudView read
model; no behavior change.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Move 4 — Decompose Remaining Large Raw-Clay Files

Same shape as Move 3, applied in order to:

1. `clients/silencer/src/client/ui/hud/InGameOverlays.cpp` (392) →
   `overlay_chat.h/.cpp`, `overlay_buy_menu.h/.cpp`, `overlay_tech_menu.h/.cpp` under `clients/silencer/src/client/ui/hud/` (the directory's current name; do not rename to "overlays/" — minimize churn).

2. `clients/silencer/src/client/ui/screens/options/options_controls_screen.cpp` (619) →
   `controls_keybind_list.h/.cpp`, `controls_rebind_capture.h/.cpp`, `controls_axis_row.h/.cpp`, plus whatever else the code reveals as natural seams. Keep `options_controls_screen.cpp` as the controller + composition only.

3. `clients/silencer/src/client/ui/screens/lobby/game_tech_panel.cpp` (515) →
   `tech_tree_view.h/.cpp`, `tech_row.h/.cpp`, `tech_selected_panel.h/.cpp`.

4. `clients/silencer/src/client/ui/screens/lobby/lobby_screen.cpp` (708) →
   `lobby_chrome.h/.cpp` (title bar / footer / version), `lobby_main_area.h/.cpp` (panel switching), residual controller in `lobby_screen.cpp`.

### Tasks (Move 4)

For each of the four files above, in the order listed:

- [ ] **4.N.1 — Read the full file** and map its natural seams.
- [ ] **4.N.2 — For each seam, repeat the Move 3 sub-builder pattern:** header, code move, composition call, incremental build, pixdiff.
- [ ] **4.N.3 — Update CMakeLists.txt.**
- [ ] **4.N.4 — Verify line counts:** original file ≤ 300, sub-files ≤ 300.
- [ ] **4.N.5 — Run full gates** + pixdiff of that screen at 640×480 and 1280×720.
- [ ] **4.N.6 — Commit per surface** (four commits total in Move 4):

```bash
git commit -m "Decompose <surface> screen into focused builders

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Move 5 — Normalize Text/Key Input

### File Structure (Move 5)

**Modify:**
- `clients/silencer/src/ui/runtime/UiInputState.h` — add `keyEvents` vector.
- `clients/silencer/src/game/events.cpp` — populate `keyEvents` from SDL key events.
- `clients/silencer/src/game/game.cpp` — pass `keyEvents` through `preparedUiInput`, ensure they reset each frame in `ResetUiFrameDeltas`.
- `clients/silencer/src/ui/runtime/UiAutomationRegistry.h/.cpp` — accept the new event channel; dispatch key/text through it the same way pointer is.
- `clients/silencer/src/client/ui/ClientUi.cpp` — extend `DispatchInput` to drain key + text events through the registry, emit typed `UiAction`s.
- `clients/silencer/src/client/ui/screens/screen.h` — drop `OnTextInput`/`OnKey` compatibility hooks (if present).
- Every screen implementing those hooks — migrate logic onto the typed-action consumer path, OR if a screen really needs raw scancodes (keybind capture is the obvious case), have it pull from `UiInputState::keyEvents` directly.

### Tasks (Move 5)

- [ ] **5.1 — Inventory current callers**

```bash
grep -rn "OnTextInput\|OnKey\b" clients/silencer/src/client/ui clients/silencer/src/ui 2>/dev/null
```

Record the list. Each one needs a migration path in 5.5/5.6.

- [ ] **5.2 — Extend `UiInputState`**

```cpp
// in UiInputState.h
struct UiKeyEvent {
	int sdlScancode = 0;
	int sdlKeycode = 0;
	int sdlModFlags = 0;
	bool down = false;
	bool repeat = false;
};

struct UiInputState {
	// ... existing ...
	std::vector<UiKeyEvent> keyEvents;
};
```

- [ ] **5.3 — Populate `keyEvents` from SDL**

In `clients/silencer/src/game/events.cpp`, the existing `SDL_EVENT_KEY_DOWN`/`SDL_EVENT_KEY_UP` handlers push a `UiKeyEvent` onto `clientUiInput`'s pending list. Reset in `ResetUiFrameDeltas` (already exists in `game.cpp:422`).

- [ ] **5.4 — Add typed-key dispatch to `UiAutomationRegistry`**

The registry already has `dispatch` for focus/key/text. Refactor so its public surface accepts a `const UiInputState&` and emits typed `UiAction`s instead of running screen callbacks directly. The screen callback hooks become internal implementation: the registry routes a key event to the focused widget, the widget emits a `UiAction`, and the action is drained alongside pointer-derived actions.

- [ ] **5.5 — Drain in `ClientUi::DispatchInput`**

`ClientUi::DispatchInput` is the single integration point. It already drives pointer/wheel via the registry; add a parallel loop that walks `input.keyEvents` and `input.textInput`, dispatching each through the registry. The result joins the same `UiAction` vector returned from `DispatchInput`.

- [ ] **5.6 — Remove `Screen::OnTextInput` / `Screen::OnKey`**

For each screen the 5.1 inventory found:

- If the screen was using these hooks to drive typed widgets (text fields, list nav), it stops implementing them — the widget gets the input through the registry.
- If the screen needs raw scancode capture (keybind rebind UI), read from `UiInputState::keyEvents` directly inside `Screen::BuildUi`. Don't reintroduce a callback.

Delete the virtual methods from `Screen` if no overrider remains.

- [ ] **5.7 — Build and run gates**

Pay special attention to:

- `tests/cli-agent/e2e/11_keyboard_navigation.sh`
- `tests/cli-agent/e2e/13_password_modal.sh`
- `tests/cli-agent/e2e/14_directional_navigation.sh`

Manual smoke: run the client, open Options → Controls, rebind a key. The rebind UI must still capture the next-pressed key.

- [ ] **5.8 — Commit**

```bash
git add -A
git commit -m "$(cat <<'EOF'
Route keyboard/text input through UiInputState typed actions

Screens no longer implement OnTextInput/OnKey compatibility hooks. The
runtime collects key events into UiInputState alongside pointer/wheel,
UiAutomationRegistry dispatches them to focused widgets, and ClientUi
drains the resulting typed UiActions. Keybind capture in
options_controls reads raw scancodes from UiInputState directly.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Final — Update CLAUDE.md Files

- [ ] **F.1 — `clients/silencer/CLAUDE.md`**

Update the "Client UI dogma" section to reflect:
- `UiFrameContext` is the per-frame reset owner.
- `HudView` is the read-model contract between gameplay and UI.
- Input contract: events into `UiInputState`, typed actions out, no `OnKey`/`OnTextInput` callbacks.

- [ ] **F.2 — `docs/audits/2026-05-13-clay-architecture-audit.md`**

Append a closing handoff entry noting all gaps resolved and pointing at the spec + plan files.

- [ ] **F.3 — Commit doc updates**

```bash
git add clients/silencer/CLAUDE.md docs/audits/2026-05-13-clay-architecture-audit.md
git commit -m "Update Clay UI docs to reflect completed architecture moves

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Final — Independent Audit

After all moves commit:

- [ ] **A.1 — Spawn an independent auditor** via the `Explore` agent type. Prompt the agent as a fresh principal game engineer with no knowledge of this plan or session. Hand it the original audit (`docs/audits/2026-05-13-clay-architecture-audit.md`) and the spec (`docs/superpowers/specs/2026-05-14-clay-ui-architecture-completion-design.md`) and ask it to verify, against the actual code, every item in the Acceptance section of the spec.

Auditor must not be told the work was done by Claude. Its prompt: "Audit whether the code on this branch satisfies the acceptance criteria in the linked spec. Read the code; do not trust commit messages. Report each acceptance item as PASS, PARTIAL, or FAIL, with file:line evidence. Flag any shortcut, half-finished migration, or backwards-compat shim."

- [ ] **A.2 — Surface the auditor's report verbatim to the user.** Do not edit the report. If it flags PARTIAL/FAIL items, list them as immediate follow-ups; do not silently fix and re-claim done.

---

## Self-Review

Spec coverage:
- Move 1 (`UiFrameContext`): tasks 1.1–1.10 cover the spec section and acceptance "UiFrameContext exists and is the only frame-arena reset entry point used by production."
- Move 2 (read models): tasks 2.1–2.11 cover the spec section and acceptance "`world.h` no longer declares friend ..."
- Move 3 (HUD decomposition): tasks 3.1–3.7 cover acceptance "InGameHud.cpp ≤ 250 lines."
- Move 4 (screen decomposition): task block 4 covers acceptance "lobby_screen.cpp, game_tech_panel.cpp, options_controls_screen.cpp, InGameOverlays.cpp each ≤ 300 lines."
- Move 5 (input contract): tasks 5.1–5.8 cover acceptance "UiInputState carries keyEvents and is the routed source for keyboard text/key dispatch in production."
- Verification gates appear in every move's task block.
- Final independent audit covers acceptance "An independent auditor reads the code and confirms each item."

Placeholder scan: no "TBD"/"TODO"/"fill in"; the spec's read-model field list is intentionally followed by a hard requirement to cross-check against the 2.1 inventory, with explicit failure criteria.

Type consistency: `UiKeyEvent` field names match between 5.2 and the design spec; `HudView`/`PlayerHudView`/`TeamHudView` field names are consistent across 2.2 and 2.5/2.6; `HudPayloadBeginFrame` keeps its name from current code through 1.2/1.3.
