# Game Staging (pre-match) — cppx parity plan

Golden: `/tmp/goldens/game_staging.png` (960×720 capture of the 640×480
logical screen). Authoritative design spec: `docs/plans/2026-06-06-cppx-visual-design-language.md`
§3.8 (and §3.6 lobby chrome, §3.9 tech-select, which are the SAME persistent
frame — staging is one center-stack variant of that shared chrome).

Source under edit: `clients/silencer/src/client/ui/screens/lobby_screen.cppx`
(`StagingPanel` + the `LobbyScreenView` right-column swap). Staging model:
`hooks/use_staging.h` (already wired by `providers/lobby_provider.{h,cpp}`).

---

## 0. The core realization

The lobby, create-game, **game-staging**, and tech-select goldens are the SAME
screen: one persistent "console chrome" frame whose **center button column** and
**right region** swap by sub-state. The current `lobby_screen.cppx` instead
renders a generic 3-column `ScreenLayout::Game` (Agent | Chat | right-swap)
with NO persistent chrome, plain text rosters, and slate `Panel`s. So game
staging is not a standalone screen — it is the `staging.active` branch of the
lobby screen, drawn inside the shared chrome with the center stack
`Choose Tech / Change Team / Ready` and a SECOND agent card top-right.

This plan therefore (a) introduces the shared persistent chrome
(TitleBar + AgentCard + StatColumn + EmblemStrip + center stack region + body
panels), and (b) maps the staging hook intents onto the golden controls, and
(c) adds a "Choose Tech" entry that swaps the right region to the tech list
(routing into the existing tech model). Lobby/create/tech parity reuse the same
chrome; this doc owns staging + the Choose-Tech seam.

---

## 1. Golden layout breakdown (regions filling 640×480)

Root: full-bleed `Starfield` (bank 6 idx 0) behind everything (already the
`ScreenLayout::Game` job — but Game currently paints `kSurfaceGame`, not the
starfield; switch it to the starfield image_patch like Menu/Overlay do, OR
add a `Lobby` variant — see §2.0).

The frame is an asymmetric tiled HUD with ~4–8px green-bordered panels and a
top title bar; NO large dead margins. Regions (logical 640×480 coords approx,
read from the golden):

```
┌──────────────────────────────────────────────────────────────┐
│ TitleBar  "Silencer v.0033" ............... [ Go Back ]   y0..18│
├─────────────┬───────────────────────┬────────────────────────┤
│ AgentCard#1 │  center button stack   │  AgentCard#2 (top-right)│
│  avatar tile│   [ Choose Tech ]      │   avatar + "AgentZero-1"│
│  LV 0       │   [ Change Team ]      │   (team member emblem)  │
│  WINS 0     │   [ Ready ]            │                         │
│  LOSSES 0   │                        │                         │
│  XP 0/100   ├────────────────────────┴────────────────────────┤
│  [ Agents ] │                                                  │
│ ┌─────────┐ │   "New Game-1" Panel (mid-left, bordered)        │
│ │ENDURANCE│ │   ┌───────────────┐                              │
│ │SHIELD   │ │   │  Pregame float │  (center overlay subpanel)  │
│ │JETPACK  │ │   │  AgentZero-...  │                             │
│ │TECH SLOTS│ │   │  -New Game-...  │                            │
│ │HACKING  │ │   └───────────────┘                              │
│ │CONTACTS │ │   (game/chat content region behind floats)       │
│ └─────────┘ │                                                  │
├─────────────┴──────────────────────────────────────────────── │
│ gg ready when you are                          (dim footer)    │
├───────────────────────────────────────────────────────────────┤
│  ◯  ◯  ◯  ◯  ◯  ◯  ◯   EmblemStrip (bottom, ~7 emblems)  y458..480│
└───────────────────────────────────────────────────────────────┘
```

Flex grammar (Yoga, fills viewport edge-to-edge):

- Root `Column`, `width/height 100%`, no padding (panels self-inset ~6–8px).
  Children: `titlebar` (fixed h≈18), `body` (flex-grow:1 Row), `footer`
  (fixed, status text), `emblem-strip` (fixed h≈22).
- `titlebar` Row, `align Center`, `justify SpaceBetween`: brand+version (left,
  Hero+Detail), `Go Back` Chrome/Oval button (hard right).
- `body` Row, `flex 1`, `gap ~6`: LEFT col (AgentCard#1 + StatColumn, fixed
  w≈150), CENTER (flex 1, holds the top button stack + the New Game/Pregame
  floats), RIGHT col (AgentCard#2 top-right, fixed w≈150).
- CENTER column itself is a `Column`: a top button-stack region
  (`Choose Tech` / `Change Team` / `Ready`, centered, gap≈6) over the
  game/float content region (flex 1).
- `footer` status text bottom-left dim; `emblem-strip` Row `justify
  SpaceAround` pinned bottom.

Text roles: brand = `hero`; version = `detail`; stat labels = `stat-small`
uppercase dim, values brighter; roster/`AgentZero-1` = `body`; footer = `detail`
dim. Button labels = oval-label (Choose Tech/Change Team/Ready are GREEN OVALS
in the golden — `AppButtonVariant::Oval`).

Sprite-backed elements:
- Starfield backdrop — whole-sprite stretch (`chrome.starfield`).
- AgentCard avatar tile — whole-sprite (steel-blue portrait). **No baked avatar
  texture exists today** (see §3); the agency emblem (bank 181) is the closest
  baked art, but it is the wrong art for the portrait. v1 ships a bordered
  Panel placeholder (vector) for the avatar cell and tracks the real portrait
  bake as a follow-up — DO NOT block staging parity on it.
- Choose Tech / Change Team / Ready / Go Back / Agents — oval sprite buttons
  (`AppButtonVariant::Oval`, nine-sliced bank-6, transparent interior). These
  ALREADY render correctly (foundation done).
- Panels (New Game-1, Pregame float, StatColumn frame, AgentCard frames) —
  thin 1px green border, transparent fill: `PanelVariant::Bordered` (already
  remapped to the green console look).
- EmblemStrip emblems — whole-sprite glow ovals; **no baked emblem texture**
  today → v1 placeholder (small bordered ovals) or omit; track follow-up.

---

## 2. Concrete ordered edits

### 2.0 — Starfield behind the lobby frame (prereq, shared)

`components/layout/screen_layout.cppx` `ScreenLayoutVariant::Game` currently
paints `tokens::fill_patch(tokens::kSurfaceGame)`. The golden shows the
starfield behind the whole frame. Change Game to use the starfield image_patch
(mirror the Menu branch), keeping the flat surface as the not-yet-baked
fallback, and drop the 24px padding to ~6px so panels reach the edges:

```cpp
if (props.variant == ScreenLayoutVariant::Game) {
  client::ui::ChromeTextures chrome = client::ui::use_chrome();
  ::ui::StylePatch bg = chrome.starfield
      ? tokens::image_patch(chrome.starfield)
      : tokens::fill_patch(tokens::kSurfaceGame);
  return <Box key={props.key} layout={::ui::LayoutStyle{
      .direction = ::ui::FlexDirection::Column,
      .width = ::ui::Length::percent(100.0f),
      .height = ::ui::Length::percent(100.0f),
      .padding = {6.0f, 6.0f, 6.0f, 6.0f},
      .gap = 4.0f}} style={bg}>
    {props.children}
  </Box>
}
```

(Lobby/create/tech share this; it is the single biggest structural fix.)

### 2.1 — New shared chrome sub-components (file-local helpers in lobby_screen.cppx)

These are net-new but SMALL and lobby-cluster-local; keep them as plain
`::ui::UiElement` helpers inside `lobby_screen.cppx` (NOT a new component dir —
combat overengineering; they are single-use to this screen family). Build the
StatColumn / AgentCard subtrees imperatively with `::ui::component(...)` only if
JSX-in-var bites; simple JSX-returning helpers transpile fine (see existing
`roster_line`).

**a) StatColumn** — label:value rows from the agent summary. The summary is a
single pre-formatted string today (`chars.selected_summary`), NOT structured
stats. v1: render it as one `BodyText` block inside a `Bordered` Panel titled by
the agent name. (Parity-faithful structured ENDURANCE/SHIELD/... rows need a
structured stat model on `use_characters` — track as follow-up; do not invent a
parse.) Keeps the left column visually a bordered stat well.

**b) AgentCard** — `Bordered` Panel: avatar placeholder Box (fixed ~64×64,
1px border) + `LV n` caption + W/L/XP `BodyText` + an `Agents` Oval button
(`session`/`characters` "Agents" intent — wire to `chars.select` open or, if no
such intent, reuse the existing `Agents`/leave affordance the lobby already has;
v1 may render it non-interactive if no intent exists). AgentCard#2 (top-right)
shows a teammate: in staging this is the first non-local roster row (or the
local row); render `name` from `staging.roster`.

**c) TitleBar** — already present in `LobbyScreenView` as the `titlebar` Box
(brand Hero + version Detail + Go Back). Keep; just confirm Go Back is hard
right (justify SpaceBetween already does this) and that version reads
`v.0033`-style (current `use_text_storage("v%s", ...)`).

**d) EmblemStrip / FooterStatus** — add a bottom `Row` of placeholder emblem
Boxes (SpaceAround) and a dim footer `BodyText` ("gg ready when you are"
mirrors the golden; source the text from a staging/chat status field — v1 can
hardcode the idle status string used by the original, or reuse
`chat.presence`). Pin to bottom of the root Column.

### 2.2 — Rework `StagingPanel` into the center stack + floats (the staging branch)

Today `StagingPanel` is one `Bordered` Panel: title "Staging" + a `[R]/[ ]`
text roster + an `ActionRow` of three buttons. The golden has NO "Staging"
title and NO bracketed-text roster. Instead:

- The three controls become a **centered vertical Oval stack** in the CENTER
  column top region: `Choose Tech`, `Change Team`, `Ready` (all
  `AppButtonVariant::Oval`). Order top→bottom matches the golden.
- The roster is surfaced as the **`New Game-1` body Panel** (game name title +
  roster `BodyText` rows) plus the **`Pregame` float** subpanel (present-agents
  list). Reuse the existing `roster_line` helper for rows; ready peers in the
  Strong (bright) face.

Draft (center stack; lives in the CENTER column of `LobbyScreenView`, gated on
`staging.active`):

```cpp
::ui::UiElement staging_center_stack(const StagingPanelProps &props) {
  return <Box key="cstack" layout={::ui::LayoutStyle{
      .direction = ::ui::FlexDirection::Column,
      .align_items = ::ui::AlignItems::Center,
      .gap = ::ui::StyleValue::points(6.0f)}}>
    <AppButton key="tech"  variant={AppButtonVariant::Oval}
        controlId="ChooseTech" label="Choose Tech" onPress={props.on_tech} />
    <AppButton key="team"  variant={AppButtonVariant::Oval}
        controlId="ChangeTeam" label="Change Team" onPress={props.on_team} />
    <AppButton key="ready" variant={AppButtonVariant::Oval}
        controlId="StagingReady" disabled={props.ready_blocked}
        label={props.ready_label} onPress={props.on_ready} />
  </Box>
}
```

NOTE: `AppButton` props use `variant`, `controlId`, `label`, `onPress`,
`disabled` (see `app_button.cppx`/`.hx`). Add `variant={AppButtonVariant::Oval}`
to EVERY lobby-cluster button — the current code omits `variant`, so buttons
default to slate `Secondary`, which is the wrong look. This single addition (oval
variant on all lobby buttons) is high-impact for parity.

`StagingPanelProps` gains `std::function<void()> on_tech` for the Choose Tech
entry (see §2.3). `on_ready`/`on_team`/`on_leave` already map to
`staging.send_ready`/`change_team`/`leave`. `Leave` is not a top-stack button in
the golden — keep Leave reachable via the top-right `Go Back`/agent card or a
small footer affordance, OR keep it but place it with the agent card (do not add
it to the center oval stack — the golden has only the three).

### 2.3 — "Choose Tech" → tech_select right region (the new entry)

The golden's tech-select screen (`tech_select.png`) is the SAME chrome with the
center stack replaced by a single `Back To Teams` oval and the RIGHT region
holding "Tech slots left: N" + a tech `ListRow` list (`Name [$cost]`,
affordable bright / exhausted dim) + scrollbar.

Wire it as a screen-local `use_state<bool> show_tech` in `LobbyScreenView`
(parallel to the existing `show_create`):

- `Choose Tech` press → `*show_tech = true`.
- When `show_tech && staging.active`: center stack becomes a single
  `Back To Teams` oval (`*show_tech = false`); the right/body region renders a
  tech list Panel built from the EXISTING tech model.
- Tech data: `hooks/use_tech.h` (`Tech{ items[], purchase, close, tech_active }`)
  + `hooks/use_buy_tech.h` (`BuyTech{ selected_index, select }`) ALREADY EXIST
  and are the in-match buy/tech station model. Confirm they are populated during
  staging (they are surfaced for the in-game buy/tech station; in pre-match
  staging the original opens the tech LOADOUT, which may be a DIFFERENT seam).
  If `use_tech` is empty in staging, the tech loadout list is a NET-NEW hook on
  the staging seam (the doc notes "Tech loadout (slots/buyable/wanted +
  set/toggle) joins in SIL-21 (4/n)" in `use_staging.h`). v1: render the tech
  list from `use_tech` if non-empty; otherwise render a "Tech slots left: N"
  header + an empty/"No tech available" Panel and track the staging-loadout hook
  as the follow-up. The Choose-Tech SEAM (button + state swap + Back To Teams)
  is the deliverable; full tech rows depend on the loadout hook.

Tech list rows reuse the `game_line`/`roster_line` pattern (a `ListRow`-style
`BodyText`, Strong when affordable/selected, Detail/Muted when exhausted):

```cpp
// inside the tech branch
std::vector<::ui::UiElement> rows;
for (int i = 0; i < (int)tech.items.size(); ++i) {
  const TechItem &t = tech.items[i];
  std::string s = t.label + "  " + t.detail;   // "Laser  [$3]"
  rows.push_back(roster_line(
      ::ui::copy_string(("tech" + std::to_string(i)).c_str()),
      ::ui::copy_string(s.c_str()), t.affordable));
}
```

### 2.4 — Compose the staging branch in `LobbyScreenView`

Replace the current `right = StagingPanel(sp)` (which renders a single
right-column panel) with a frame composition: keep the LEFT agent column and add
the CENTER stack (or tech list) + AgentCard#2 RIGHT + body floats. Concretely,
in the `if (staging.active)` branch, build:

- center = `*show_tech ? back_to_teams_stack : staging_center_stack(sp)`
- right region = `*show_tech ? tech_list_panel : agent_card_2 + body floats`
- assemble into the body Row alongside the persistent LEFT column.

Keep the call order of all hooks/`use_text_storage` stable (the existing code is
careful about this — preserve it; compute every text scratch each frame
regardless of the active sub-branch, as it already does).

---

## 3. Data / hooks / sprites: exist vs net-new

EXIST (cite):
- `Staging` model + `send_ready`/`change_team`/`leave` intents + `roster` +
  `ready_blocked`/`ready_label`/`active` — `hooks/use_staging.h`, produced by
  `providers/lobby_provider.{h,cpp}`. Maps directly to Ready/Change Team/Leave.
- Oval sprite buttons (transparent, nine-sliced) + Chrome buttons + Bordered
  Panel + Starfield + BodyText variants + ScreenTitle + ActionRow + ScreenLayout
  — `components/actions/*`, `components/surfaces/panel.cppx`,
  `components/text/*`, `components/layout/screen_layout.cppx`. Foundation done.
- Tech model — `hooks/use_tech.h` (`Tech`) + `hooks/use_buy_tech.h` (`BuyTech`)
  for the rows/selection/purchase. Use for the Choose-Tech list IF populated in
  staging.
- Agent summary string — `hooks/use_characters.h` `selected_summary` (one
  pre-formatted line; NOT structured stats).
- Starfield/oval/chrome/panel baked sprites — `hooks/use_chrome.h`
  `ChromeTextures` (starfield, oval_md/sm/lg, chrome panel, agency_emblem[5],
  toggle cells, logo).

NET-NEW (and how to add minimally):
- **Choose Tech entry**: `on_tech` field on `StagingPanelProps` +
  `use_state<bool> show_tech` in `LobbyScreenView` + `Back To Teams` button.
  Pure screen-local; no hook change.
- **Persistent chrome sub-components** (TitleBar already exists; AgentCard,
  StatColumn, EmblemStrip, FooterStatus): file-local helpers in
  `lobby_screen.cppx`. Single-use → do NOT promote to `components/`.
- **Structured agent stats** (ENDURANCE/SHIELD/JETPACK/TECH SLOTS/HACKING/
  CONTACTS): would need a structured field on `use_characters` (e.g.
  `std::vector<std::pair<std::string,std::string>> stats`). FOLLOW-UP — v1
  renders `selected_summary` as a single block. Do not parse the string.
- **Avatar portrait sprite + team emblem sprite**: no baked texture in
  `ChromeTextures` (only `agency_emblem[5]`, wrong art). FOLLOW-UP bake; v1 uses
  bordered placeholders. Tracked, not blocking.
- **Staging tech-loadout hook**: if `use_tech` is empty pre-match, the tech
  loadout list is net-new on the staging seam (flagged in `use_staging.h` as
  SIL-21 4/n). v1 ships the SEAM (button+swap+Back) and an empty/placeholder
  list; full rows = follow-up hook.

---

## 4. Capture / verify recipe (headless)

The staging room requires a live dedicated server with a provisioned map, so the
full live capture is the "capstone E2E" (`31_lobby_create_staging.sh` only
reaches GameCreate, NOT the live staging room — see its comments). Two paths:

1. **Visual-regression harness (preferred for goldens):** extend
   `tests/cli-agent/e2e/71_visual_regression_lobby.sh` (and the non-Discord
   sibling `70_visual_regression.sh`). It already boots lobby + silencer, drives
   MAINMENU → Connect → Login → CREATECHARACTER → LOBBY and `cap`s
   `lobby_connect`/`character_create`/`lobby_screen`. Add steps after LOBBY to
   create a game, wait for `staging.active` (poll `inspect` for `ChooseTech` /
   `StagingReady` widgets), then `cap game_staging`, then click `ChooseTech` and
   `cap tech_select`. Reaching live staging needs a map provisioned for the
   spawned dedicated server (the harness already wires `-maps-dir`); follow the
   capstone E2E's map-provision step. Capture via
   `cli --port $CTRL screenshot --out work/game_staging.png` after
   `wait_frames --n 3`; diff with `tools/pixdiff/build/pixdiff` vs
   `tests/cli-agent/e2e/golden/game_staging.png` (threshold ~1.0). `BLESS=1`
   re-blesses.
2. **Manual headless check:** run `30_lobby_login.sh`/`31_lobby_create_staging.sh`
   to confirm widget IDs (`ChooseTech`, `ChangeTeam`, `StagingReady`,
   `BackToTeams`) appear via `cli ... inspect`, even before the live room is
   reachable, to prove the control wiring + state swap.

Build via `clients/silencer/build.sh` (never raw cmake). Then run the suite:
`bash tests/cli-agent/run.sh` (or the single 70/71 scripts). Self-verify by
reading the captured `work/game_staging.png` with the Read tool and comparing to
the golden; DM the side-by-side composite per the lobby VR script's existing
Discord path.

---

## 5. Risks / unknowns

- **Live staging is hard to reach headlessly.** It needs a spawned dedicated
  server + a provisioned map; `31_lobby_create_staging.sh` stops at GameCreate on
  purpose. The map-provision step from the capstone E2E is the unlock; if it is
  flaky, golden capture stalls. Mitigation: stand up the map provision first and
  prove `staging.active` flips before tuning visuals.
- **No portrait/emblem art baked.** `ChromeTextures` has only `agency_emblem`
  (wrong art for the avatar) — AgentCard avatar + EmblemStrip ship as
  placeholders, so pixel parity on those tiles is deferred. Accept a higher
  pixdiff threshold or mask those regions until the bake lands.
- **Stat column is a flat string, not structured.** Parity wants 6 label:value
  rows; `selected_summary` is one line. Don't invent a parser — render the block
  and follow up with a structured `use_characters` field.
- **Tech list source ambiguity.** Pre-match Choose-Tech loadout may NOT be the
  same seam as the in-game `use_tech` buy station. If `use_tech` is empty in
  staging, the rows need a new staging-loadout hook (flagged in `use_staging.h`).
  The Choose-Tech button + state swap + Back To Teams is deliverable regardless;
  the rows may be empty in v1.
- **All lobby buttons currently default to slate Secondary.** Every button in
  the cluster must get `variant={AppButtonVariant::Oval}` (or Chrome for Go
  Back). Missing this is the most likely "looks wrong" regression.
- **Shared chrome blast radius.** Making `ScreenLayout::Game` paint the starfield
  + introducing the persistent frame changes lobby AND create-game AND
  tech-select goldens simultaneously — coordinate the re-bless across the whole
  cluster (70/71 suites), not just `game_staging`.
