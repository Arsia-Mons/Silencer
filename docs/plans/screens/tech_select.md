# Screen plan — Tech Select (`tech_select.png`)

> SPRITE-first parity recreation of the **Game-Staging "Choose Tech"** screen
> in the cppx engine. Golden: `/tmp/goldens/tech_select.png`. Hooks:
> `use_tech.h` + `use_buy_tech.h`. Design-language §3.9 + §3.8.
> origin/main authority: `clients/silencer/src/client/ui/screens/lobby/
> game_tech_panel.cpp` + `tech_tree_grid.cpp` + `tech_selected_panel.cpp` +
> `docs/design/screen-lobby-game-tech.md`.

## 0. State of play — is there a current screen? NET-NEW (mostly)

There is **no Tech Select screen** today. What exists:

- An **in-match** buy/tech overlay (`in_game_screen.cppx` `BuyTechOverlay`,
  driven by `use_tech()`/`use_buy_tech()`). This is the *techstation in a live
  match*, NOT the golden. It renders a slate Hero `Panel` with 5 rows + a
  "Close" button — wrong screen, wrong chrome.
- The lobby **Game-Staging** room (`lobby_screen.cppx` `StagingPanel`, mounted
  when `use_staging().active`). The golden is reached by clicking **Choose Tech**
  from this staging panel — but `StagingPanel` has NO "Choose Tech" button and
  there is no tech sub-view.

origin/main models this exactly: `GameTechPanel` **replaces** the staging
panel's right-column upper button (`Choose Tech` → `Back To Teams`) and swaps
the tall pane for a tech grid; `Change Team`/`Ready` remain. So Tech Select is a
**screen-local panel swap inside the staging room**, identical in spirit to the
existing `GameSelect ↔ GameCreate` swap already in `lobby_screen.cppx`
(`use_state<bool> show_create`). That is the seam to reuse.

**Critical data gap:** `use_tech()` reads `WorldSessionProvider`, whose snapshot
(`world_session_model.cpp:55`) **early-returns unless phase == InMatch/
SinglePlayer**. In the Lobby phase `use_tech()` returns an all-empty `Tech{}`.
The staging tech list (`world.buyableitems` filtered by agency/techslots, the
per-item slot cost, the toggle/selected state) is **not surfaced anywhere** in
the cppx port. This must be added to the **lobby** snapshot/provider, not the
world-session one. See §3.

---

## 1. Golden layout breakdown (regions + flex grammar)

The golden is the full **Game-Staging chrome** (§3.8) with the right-region
upper button swapped to **Back To Teams** and a **tech list panel** added top-
right. Reading the 640×480 golden region by region:

```
┌──────────────────────────────────────────────────────────────┐
│ TITLE BAR  "Silencer v.0033" ......................  Go Back  │  titlebar (h~18)
├──────────────────────────────────────────────────────────────┤
│ ┌AgentCard┐  ┌Back To Teams┐         ┌ Tech slots left: 1 ──┐ │
│ │ avatar  │  └─────────────┘         │ Laser        [$3]    │ │  ← golden's
│ │ LV 0    │  (Change Team / Ready    │ Rocket       [$3]    │ │    visible
│ │ WINS 0  │   live under, off-frame  │ Flamer Ammo  [$3]    │ │    panels
│ │ LOSSES0 │   top in golden)         │ Health Pack  [$3]    │ │
│ │ XP 0/100│                          │ Shaped Bomb  [$3]    │ │
│ │[Agents] │  ┌AgentCard #2 (top-rt)┐ │ ...           (dim)  │ │
│ ├StatCol──┤  │ AgentZero-1         │ │ Plast Cannon [$3]    │ │
│ │ENDURANCE│  └─────────────────────┘ │ Camera       [$3]    │ │
│ │SHIELD   │  ┌ New Game-1 ─────────┐ │ Radar Jam    [$3]    │ │
│ │JETPACK  │  │ AgentZero - New Game│ │ Base Defense [$3]    │ │
│ │TECHSLOTS│  └─────────────────────┘ │ Insider Info [$3]    ┃ │ ← ScrollBar
│ │HACKING  │  ┌ Pregame float ──────┐ └──────────────────────┘ │
│ │CONTACTS │  │ AgentZero - New ... │                          │
│ └─────────┘  └─────────────────────┘                          │
│                  (Mars backdrop bleeds behind center)          │
├──────────────────────────────────────────────────────────────┤
│ gg ready when you are  (dim footer)        [emblem strip]      │
└──────────────────────────────────────────────────────────────┘
```

Region inventory and flex grammar (fill 640×480 edge-to-edge; root is the
`Starfield` from `ScreenLayoutVariant::Game`):

| Region | Source | Flex |
|---|---|---|
| **Starfield root** | `ScreenLayout` Game variant (already starfield-backed in `screen_layout.cppx`; menu/Overlay variants are; **Game is NOT — see Risk R1**) | column, 100%×100% |
| **Title bar** | `Box` row: `ScreenTitle` Hero "Silencer" + Detail "v.0033" + `Go Back` button, `SpaceBetween` | row, width 100%, h~18 |
| **Body** | `ActionRow`-style row (3 columns) | row, grow, gap ~6 |
| · LEFT col | `AgentCard` + `StatColumn` (ENDURANCE…CONTACTS) | column, fixed ~120w |
| · CENTER col | top→bottom: **Back To Teams** oval (swapped), AgentCard#2, `New Game-1` panel, `Pregame` float | column |
| · RIGHT col | **Tech list panel**: header "Tech slots left: N" + `ListRow`s + `ScrollBar` | column, fixed ~200w |
| **Footer** | `Box` row: dim status "gg ready when you are" + `EmblemStrip` | row, width 100% |

**The tech list panel (the focus of this screen).** Right-aligned panel,
`Panel` variant `Bordered` (thin dim-green outline, transparent fill, title band
top). Header line in green `Tech slots left: 1`. Then a vertical list of
`ListRow`s: `Name` left, `[$cost]` right suffix. In the golden:

- The **top rows are bright** (`Laser`, `Rocket`, `Flamer Ammo`, `Health Pack`,
  `Shaped Bomb`) = affordable / enough tech slots; the **lower rows are dim**
  (`Plast Cannon`, `Camera`, `Radar Jam`, `Base Defense`, `Insider Info`) =
  exhausted slots (slots-left=1, these cost more slots than remain).
- A **scrollbar** rides the right seam of the panel.
- Cost suffix renders as `[$3]` (legacy shows price, even on the tech tab; the
  golden's `[$3]` matches origin's per-row label) — but origin's **canonical
  detail is slot-count** (`<name> (<slots>)`). The golden literally shows `[$3]`
  so the row detail string must be `[$N]` price form (see §3 row builder).

Per design-language §3.9: `Name [$cost]` `ListRow`s, affordable=bright,
exhausted=dim, + ScrollBar; top-center `Back To Teams` oval; top-right
`Tech slots left: 1` header.

---

## 2. Concrete ordered edits

The screen is a **panel swap inside the existing staging room**, so most work is
in `lobby_screen.cppx` + the **lobby** model/provider (not world-session). Order:

### Edit A — surface the staging tech list on the LOBBY snapshot (data)

`use_tech()`/`use_buy_tech()` are bound to the **world-session** provider, which
is empty in the Lobby phase. Rather than re-home those hooks, **extend the
Staging model** (the screen already consumes `use_staging()` for the room).
Add to `client/ui/hooks/use_staging.h`:

```cpp
// One selectable tech entry in the staging Choose-Tech list (origin/main
// game_tech_panel). `label` = item name, `detail` = "[$cost]" (golden form),
// `affordable` = enough tech slots remain OR already chosen (interactable),
// `chosen` = the local peer currently has this tech toggled on.
struct StagingTechItem {
  std::string label = {};
  std::string detail = {};
  bool affordable = false;
  bool chosen = false;
};

struct Staging {
  // ... existing fields ...
  std::vector<StagingTechItem> tech_items = {};
  int tech_slots_left = 0;
  std::function<void(int)> toggle_tech = {};   // flip choice for item index
};
```

Build it in `game/ui/lobby_ui_model.cpp` `BuildStaging(...)` (which already has
`world`, `lobby`, `localpeer`, `team`). Mirror origin
`game_tech_panel.cpp:GameTechPanelTick` exactly:

```cpp
User *user = lobby.GetUserInfo(localpeer->accountid);
if (user) {
  const int used = world.TechSlotsUsed(*localpeer);
  snap.staging_tech_slots_left = user->agency[team->agency].techslots - used;
  for (int i = 0; i < (int)world.buyableitems.size(); ++i) {
    BuyableItem *bi = world.buyableitems[i];
    if (bi->techslots == 0) continue;                 // buy-only items skipped
    if (bi->agencyspecific >= 0 && bi->agencyspecific != team->agency) continue;
    client::ui::StagingTechItem row;
    row.label = bi->name;
    row.detail = "[$" + std::to_string(bi->price) + "]";
    row.chosen = (localpeer->techchoices & bi->techchoice) != 0;
    row.affordable = (bi->techslots <= snap.staging_tech_slots_left) || row.chosen;
    snap.staging_tech_items.push_back(std::move(row));
  }
}
```

Add the matching `LobbySnapshot` fields in `client/ui/providers/lobby_provider.h`
(next to `staging_roster`), and a `toggle_tech` intent closure in
`LobbyProviderValue` wired in the composition root
(`game/ui/game_ui_pipeline.cpp`) to the public seam used by origin
(`owner.TechPanelSetTech` → `localpeer->techchoices ^= item->techchoice` then
`Config::Save`). Verify the public `World`/`Player` seam exposes a
"set tech choices" path; if only the in-match `Player::*Item` exists, add a thin
public `World::SetPeerTechChoices(peerid, mask)` mirroring origin's
`LobbyScreen::TechPanelSetTech` (no backwards-compat shim; this is the new seam).
Surface all of it in `lobby_provider.cpp use_staging()`.

> **Why not `use_tech()`?** That hook is the *in-match techstation* (world-
> session). The golden is *pre-match staging loadout* — a different lifecycle
> and a different data source (`world.buyableitems` + `localpeer->techchoices`
> vs `Player::CollectBuyMenuItems`). Keeping them separate matches origin's two
> distinct code paths and avoids overloading one hook with two meanings.

### Edit B — add the "Choose Tech" button + tech sub-view swap to `StagingPanel`

In `lobby_screen.cppx`, the staging room currently renders one `StagingPanel`.
Add a screen-local `use_state<bool> show_tech` (mirroring `show_create`) and an
action button. Origin keeps `Change Team`/`Ready` and swaps only the top button:

```cpp
// inside LobbyScreenView, alongside show_create:
bool *show_tech = use_state<bool>(false);
// ... guard with the other use_state checks ...

// in the staging.active branch:
if (staging.active) {
  if (*show_tech) {
    TechSelectPanelProps tp;
    tp.items = staging.tech_items;
    tp.slots_left = staging.tech_slots_left;
    tp.selected = /* cursor, see Edit C */;
    tp.on_back = [show_tech]() { *show_tech = false; };
    tp.on_team = staging.change_team;
    tp.ready_label = ready_label;
    tp.ready_blocked = staging.ready_blocked;
    tp.on_ready = staging.send_ready;
    tp.on_toggle = staging.toggle_tech;
    right = TechSelectPanel(tp);
  } else {
    StagingPanelProps sp; /* existing */
    sp.on_choose_tech = [show_tech]() { *show_tech = true; };
    right = StagingPanel(sp);
  }
}
```

Add `on_choose_tech` to `StagingPanelProps` and a `Choose Tech` `AppButton`
(controlId `ChooseTech`) as the FIRST action in `StagingPanel`'s `ActionRow`
(origin order: Choose Tech / Change Team / Ready).

### Edit C — author the `TechSelectPanel` sub-component (in `lobby_screen.cppx`)

A file-local component beside `StagingPanel`. SPRITE-first: `Panel`
`Bordered` (already nine-slice green-outline, transparent fill — design §P14).
Header `BodyText` Strong; a dynamic `ListRow` list via `::ui::children`; the
`Back To Teams` oval + `Change Team`/`Ready` actions; a `ScrollBar` if the list
overflows (defer to `ScrollView` — design §P11/§P18; see Risk R3).

DRAFT (follows the existing `roster_line`/`game_line` dynamic-list idiom and the
`buytech_row` per-row pattern already proven in `in_game_screen.cppx`):

```cpp
struct TechSelectPanelProps {
  std::vector<StagingTechItem> items = {};
  int slots_left = 0;
  int selected = 0;
  const char *ready_label = "Ready";
  bool ready_blocked = false;
  std::function<void()> on_back = {};
  std::function<void()> on_team = {};
  std::function<void()> on_ready = {};
  std::function<void(int)> on_toggle = {};
};

// One tech list row: "Name        [$cost]", bright when affordable/chosen, dim
// when exhausted; caret/selected marker on the cursor row. Focus drives the
// cursor; activation toggles the choice (origin toggleClickedItemIndex path).
::ui::UiElement tech_row(int i, const std::vector<StagingTechItem> &items,
                         int sel, std::function<void(int)> toggle) {
  if (i < 0 || i >= (int)items.size())
    return ::ui::empty();
  const StagingTechItem &it = items[i];
  std::string text = std::string(it.chosen ? "* " : "  ") + it.label;
  if (!it.detail.empty()) { text += "   "; text += it.detail; }
  const char *label = ::ui::copy_string(text.c_str());
  const char *key = ::ui::copy_string(("tr" + std::to_string(i)).c_str());
  std::function<void()> on_press = [toggle, i]() { if (toggle) toggle(i); };
  return <BodyText
    key={key}
    variant={it.affordable ? BodyTextVariant::Strong : BodyTextVariant::Detail}
    value={label}
  />
  // NOTE: to make rows clickable/focusable for nav + e2e, render each as an
  // AppButton (Ghost variant) instead of BodyText — see Edit C2.
}

::ui::UiElement TechSelectPanel(const TechSelectPanelProps &props) {
  std::vector<::ui::UiElement> rows;
  if (props.items.empty())
    rows.push_back(tech_row_placeholder());  // "No tech available"
  else
    for (int i = 0; i < (int)props.items.size(); ++i)
      rows.push_back(tech_row(i, props.items, props.selected, props.on_toggle));
  ::ui::UiChildren tech_rows = ::ui::children(rows);
  const char *slots = ::ui::copy_string(
      ("Tech slots left: " + std::to_string(props.slots_left)).c_str());
  return <Panel key="tech" variant={PanelVariant::Bordered}>
    <BodyText key="slots" variant={BodyTextVariant::Strong} value={slots} />
    {tech_rows}
    <ActionRow key="actions">
      <AppButton key="back" controlId="GameTechBack" label="Back To Teams"
                 onPress={props.on_back} />
      <AppButton key="team" controlId="ChangeTeam" label="Change Team"
                 onPress={props.on_team} />
      <AppButton key="ready" controlId="StagingReady" disabled={props.ready_blocked}
                 label={props.ready_label} onPress={props.on_ready} />
    </ActionRow>
  </Panel>
}
```

**Edit C2 — make rows focusable for nav + cursor.** To match origin's
hover-to-describe + click-to-toggle and to give the e2e a clickable target,
render each row as an `AppButton` (`AppButtonVariant::Ghost`, controlId
`GameTechRow`, `controlOffset=i`, `selected={i==sel}`, `defaultFocused={i==sel}`,
`onFocus` → cursor select, `onPress` → toggle) exactly like `buytech_row` in
`in_game_screen.cppx:90`. Drive the cursor with a screen-local
`use_state<int> tech_sel` (a full `use_buy_tech`-style replicated cursor is
unnecessary here — staging has no replicated tech cursor; the local cursor is
fine and simpler). Clamp `tech_sel` against `items.size()` each frame (copy the
`*sel` clamp pattern at `lobby_screen.cppx:222`).

### Edit D — fixed-slot list vs ScrollView

The list is up to ~12 rows (origin `kMaxRows=32`). Two options:
- **v1 (simplest, matches golden crop):** a fixed window of N rows via the
  proven `buytech_row(0..N)` idiom — emit `tech_row(0)…tech_row(11)` so the
  child shape is stable across frames (the doc's "fixed-slot dynamic lists"
  pattern). The golden shows ~12 rows; 12 covers it.
- **v2 (parity):** wrap rows in `ScrollView` (`ui/components/scroll_view.cppx`,
  virtualized, `row_height`) for the scrollbar + overflow. Prefer v2 if the
  scrollbar in the golden must render; v1 if the scrollbar is cosmetic. Pick v2
  to match the visible scrollbar (design §P11).

---

## 3. Data / hooks / sprites — exist vs net-new

| Need | Status | File |
|---|---|---|
| `Tech`/`TechItem`/`BuyTech` hooks | EXIST but wrong lifecycle (in-match only) | `hooks/use_tech.h`, `hooks/use_buy_tech.h`, `providers/world_session_provider.cpp:93` |
| Staging room model + `staging.active` swap seam | EXIST | `hooks/use_staging.h`, `lobby_screen.cppx:268`, `providers/lobby_provider.cpp:121` |
| Staging **tech list** data (`buyableitems` × agency × slots × choices) | **NET-NEW** (Edit A) | add to `use_staging.h` + `lobby_ui_model.cpp:BuildStaging` + `lobby_provider.{h,cpp}` + `game_ui_pipeline.cpp` |
| `world.buyableitems`, `World::TechSlotsUsed`, `techchoices`, `techslots`, `agencyspecific` | EXIST (origin uses them) | `world.cpp` / `buyableitem.h` / `team.h` / `user.h` (`world_gameplay.cpp:169,279`) |
| Public seam to set tech choices pre-match | **VERIFY / maybe NET-NEW** | origin `LobbyScreen::TechPanelSetTech`; may need `World::SetPeerTechChoices` |
| `Panel` Bordered, `AppButton` Oval/Ghost, `ActionRow`, `BodyText`, `ScreenTitle` | EXIST | `surfaces/panel.cppx`, `actions/app_button.cppx`, `actions/action_row.cppx`, `text/*` |
| `ScrollView` (virtualized, row_height) | EXIST | `ui/components/scroll_view.cppx` |
| `::ui::children`, `::ui::copy_string`, `use_text_storage`, `use_state` | EXIST | `ui/runtime/element.h:284`, `react.h:174` |
| Sprites (oval button, panel chrome, starfield, scrollbar) | EXIST baked | `use_chrome.h`; oval bank6 idx7/28/23, starfield bank6 idx0 |
| **AgentCard, StatColumn, EmblemStrip, TitleBar persistent chrome** | **NET-NEW** (shared with staging/lobby parity) | not yet built — see Risk R2 |

**No new sprites required** for the tech panel itself — it is `Panel` Bordered
(nine-slice) + text rows + (optional) `ScrollBar` from `ScrollView`. The
surrounding staging chrome (AgentCard/StatColumn/EmblemStrip/TitleBar) is the
same net-new persistent-chrome work the whole lobby cluster needs (design §3.6/
Phase 4); this plan does NOT block on it — the tech swap can land on the current
`LobbyScreenView` title bar + 3-column `ActionRow` and be tightened when the
persistent chrome lands.

---

## 4. Capture / verify recipe (headless)

The lobby visual-regression harness (`71_visual_regression_lobby.sh`) **cannot**
reach live staging — its own comment notes the live staging room needs a
map-provisioned dedicated server (the "capstone E2E"). Two verification paths:

**(a) Structural inspect (fast, no map needed) — extend `31_lobby_create_staging.sh`:**
That test already drives MAINMENU → connect → auth → CREATECHARACTER → LOBBY and
exercises the GameSelect↔GameCreate swap via `cli inspect` / `wait_for_widget`.
Once `staging.active` is reachable (needs the create→spawn→auto-join pump, which
that test notes runs through the LOBBY tick), add:
```bash
wait_for_widget "ChooseTech"
cli --port "$CTRL_PORT" click --label "ChooseTech"
wait_for_widget "GameTechBack"      # "Back To Teams" now present
wait_for_widget "StagingReady"      # Change Team / Ready still present
cli --port "$CTRL_PORT" click --label "GameTechBack"
wait_for_widget "ChooseTech"        # swap reverses
```
This proves the swap + button substitution without a live map (mirrors how 31
gates the GameCreate swap structurally).

**(b) Pixel capture for the golden — staging requires a real game.** To get a
populated `world.buyableitems` (origin notes the dump harness leaves the grid
empty without it), the capstone path must: provision a map for the spawned
dedicated server, create+auto-join a game, reach `staging.active`, click
`ChooseTech`, then:
```bash
cli --port "$CTRL_PORT" resize --w 960 --h 720
cli --port "$CTRL_PORT" wait_frames --n 3
cli --port "$CTRL_PORT" screenshot --out /tmp/tech_select.png
tools/pixdiff/build/pixdiff /tmp/tech_select.png /tmp/goldens/tech_select.png
```
Add a `cap tech_select` block to `71_visual_regression_lobby.sh` once staging is
reachable there, and bless with `BLESS=1`.

**Build:** `bash clients/silencer/build.sh` (never raw cmake). The `.cppx` are
transpiled at build; check generated output under `build/generated/cppx/`.

**Boundary guard:** run `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh`
after the provider/hook edits (it bans deleted-layer tokens and guards UI seams).

---

## 5. Risks / unknowns

- **R1 — Game-variant root is not starfield-backed.** `ScreenLayoutVariant::Game`
  paints `tokens::kSurfaceGame` (flat), NOT the bank-6 starfield+Mars the golden
  shows behind the staging chrome (`screen_layout.cppx:22`). The Menu/Overlay
  variants ARE starfield-backed. Fix: paint `chrome.starfield` in the Game
  variant too (one-line, mirrors lines 41/108). Verify Mars bleeds behind the
  center column as in the golden.
- **R2 — Persistent chrome is net-new.** AgentCard, StatColumn, EmblemStrip, the
  real TitleBar (§3.6, design Phase 4) don't exist; the current `LobbyScreenView`
  is a 3-panel `ActionRow`, not the asymmetric tiled HUD of the golden. The tech
  panel can land independent of this, but full golden parity (left AgentCard +
  stat column, Pregame float, emblem strip, "gg ready when you are" footer)
  depends on that shared lobby work. Scope this screen to the tech-panel swap +
  R1 starfield; flag the chrome as a dependency, not a blocker.
- **R3 — Scrollbar.** The golden shows a visible scrollbar on the tech list.
  `ScrollView` exists but wiring its `row_height`/virtualization + the baked
  scrollbar nub/track sprites (NOT yet in `use_chrome` — table shows no
  scrollbar bank/idx baked) is unproven for this list. If the baked scrollbar
  art is missing, the track/thumb fall back to vector or are absent. Decide:
  v1 fixed-window (no scrollbar) for first parity pass, v2 ScrollView once the
  scrollbar sprite is baked.
- **R4 — Tech-choice write seam.** origin toggles `localpeer->techchoices` and
  persists to `Config`. Confirm a PUBLIC pre-match seam exists; if not, add
  `World::SetPeerTechChoices` (new seam, no shim). Read-only render works
  without it, but toggling (and the golden's bright/dim affordability) needs the
  live `techchoices` + `TechSlotsUsed` round-trip.
- **R5 — Row detail string: `[$N]` vs `(N slots)`.** Golden shows `[$3]`
  (price), origin's canonical detail is `(slots)`. Plan uses `[$price]` to match
  the golden pixels; if a future capture shows slot-count, swap the format in the
  Edit-A row builder (one line).
- **R6 — Reaching live staging headlessly is the hard part.** No current e2e
  reaches `staging.active` (31 stops at the create-form swap; 71 can't provision
  a map). The pixel golden depends on standing up the capstone path
  (map-provisioned dedicated server + auto-join). Structural inspect (§4a) is the
  achievable near-term gate.
