# Lobby screen — origin/main parity recreate plan

> Target golden: `/tmp/goldens/lobby.png` (640x480 logical, dense green-phosphor
> HUD). Source: `clients/silencer/src/client/ui/screens/lobby_screen.cppx`
> (`LobbyScreenView` + `GameSelectPanel`/`GameCreatePanel`/`StagingPanel`).
> Spec section: §3.6 of `docs/plans/2026-06-06-cppx-visual-design-language.md`.
> SPRITE-first, nine-slice default. This is the most divergent menu screen: the
> current code has NO persistent chrome (title bar / AgentCard / StatColumn /
> emblem strip), wrong panel titles, plain-text lists, and a single flexed
> `ActionRow` of three columns instead of a tiled HUD.

---

## 1. Golden layout breakdown (regions + flex grammar)

The golden fills 640x480 edge-to-edge with **persistent chrome** framing a
**tiled body**. Measured from the crops:

```
┌───────────────────────────────────────────────────────────────┐  y0
│ Silencer v.00053                                    [ Go Back ] │  TITLE BAR (~0..24)
├───────────────────────────────────────────────────────────────┤
│ ┌AgentCard──┐  ┌StatCol──┐   · Create Game ·   ┌Active Games─┐ │
│ │ [avatar]  │  │ENDURANCE│                      │ (empty list)│ │  UPPER BODY
│ │ AgentZero │  │SHIELD   │                      │             │ │  (~28..~250)
│ │ WINS  0   │  │JETPACK  │                      │             │ │
│ │ LOSSES 0  │  │TECH SLTS│                      │             │ │
│ │ XP 0/100  │  │HACKING  │                      │             │ │
│ │ LV0 Agents│  │CONTACTS │                      │             │ │
│ └───────────┘  └─────────┘                      └─────────────┘ │
│ ┌Lobby (chat)──────────┐        ┌In Lobby─────┐                │  LOWER BODY
│ │ <scrollback>         │        │ AgentZero   │ (float over     │  (~250..~440)
│ │ ...                  │        │ ...         │  panels below)  │
│ └──────────────────────┘        └─────────────┘                │
├───────────────────────────────────────────────────────────────┤
│   ◉    ◉    ◉    ◉    ◉    ◉    ◉                                │  EMBLEM STRIP
└───────────────────────────────────────────────────────────────┘  (~458..480)
```

**Grammar (root = `ScreenLayout` Game variant, black fill, column flex):**

- **Title bar** (`titlebar-h ≈ 18-24`): Row, `justify=SpaceBetween`,
  `width=100%`. LEFT = hero `Silencer` + tiny dim `v.00053` suffix (one baseline
  group — version sits immediately right of the brand, NOT spread to center).
  RIGHT = `Go Back` chrome/rect button hard-right. The current code's
  `SpaceBetween` puts the version in the *center* (3-way spread); golden groups
  brand+version left.
- **Body** = a `flex: 1` Row filling the gap between title bar and emblem strip,
  split into a LEFT rail and a RIGHT field:
  - LEFT rail (Column, fixed ~190px): `AgentCard` (avatar + W/L/XP + `Agents`
    oval) on top of a `StatColumn` (six `LABEL : value` rows). In the golden the
    StatColumn sits just right of the card top; treat as: AgentCard column +
    StatColumn column in a small Row, pinned top-left.
  - RIGHT field (`flex:1` Column): top Row = floating `Create Game` oval
    (center) + `Active Games` Panel (right); bottom Row = `Lobby` chat Panel
    (left) + `In Lobby` SubPanel float (center-right). Panels TILE to fill —
    use `flex:1` on the field children and `gap ≈ 4-6` (`panel-pad` lobby tier)
    so they tessellate edge-to-edge with thin seams, matching the dense HUD.
- **Emblem strip** (`emblem-strip-h ≈ 22`): Row pinned to the bottom edge,
  `justify=SpaceAround`, ~7 whole-sprite glowing green ovals (NOT nine-sliced,
  NOT buttons — decorative).

Key fill rule: body is ONE `flex:1` region between two fixed-height bands; no
large dead margins. `ScreenLayout` Game currently pads `{24,24,24,24}` and
`gap=18` — too loose for this dense HUD. Override to a tight padding (≈8) +
small gap on the lobby root (or add a `Hud`-tight variant inline via the Box).

---

## 2. Concrete ordered edits to `lobby_screen.cppx`

The file already has the right *data wiring* (hooks, panel swap, intents). The
work is almost entirely **visual reframing** + two new presentational
sub-components + one new sprite + one data field. Ordered:

### Edit A — tighten the root + restructure into chrome + body + strip
Replace the `ScreenLayout`(Game) → `Box`(titlebar) + `ActionRow`(columns) tree
with: title bar Row, a `flex:1` body Row (left rail + right field), and a bottom
emblem strip Row. Drop the loose `ScreenLayout` padding by wrapping content in
an inner Box with tight padding, OR author the root as a bare full-bleed Box
with `tokens::fill_patch(tokens::kSurfaceGame)` and `padding≈8, gap≈6`. DRAFT:

```cpp
return <Box key="root" layout={::ui::LayoutStyle{
    .direction = ::ui::FlexDirection::Column,
    .width = ::ui::Length::percent(100.0f),
    .height = ::ui::Length::percent(100.0f),
    .padding = {8.0f, 8.0f, 8.0f, 8.0f},
    .gap = ::ui::StyleValue::points(6.0f)}}
  style={tokens::fill_patch(tokens::kSurfaceGame)}>
  {title_bar}                 // Row, SpaceBetween
  <Box key="body" layout={::ui::LayoutStyle{
      .direction = ::ui::FlexDirection::Row,
      .align_items = ::ui::AlignItems::Stretch,
      .flex_grow = 1.0f,
      .gap = ::ui::StyleValue::points(6.0f)}}>
    {left_rail}               // AgentCard + StatColumn, fixed width
    {right_field}             // flex:1 column of two tiled rows
  </Box>
  {emblem_strip}             // Row, SpaceAround, fixed height
</Box>
```
(Confirm `flex_grow`/`flex` field name in `::ui::LayoutStyle` — see
`ui/components/common.h`; `screen_layout.cppx` uses `.height percent` not flex,
so verify the grow field exists or fall back to a percent-height split.)

### Edit B — title bar: group brand+version left, Go Back right
```cpp
::ui::UiElement title_bar =
  <Box key="titlebar" layout={::ui::LayoutStyle{
      .direction = ::ui::FlexDirection::Row,
      .align_items = ::ui::AlignItems::Center,
      .justify_content = ::ui::JustifyContent::SpaceBetween,
      .width = ::ui::Length::percent(100.0f)}}>
    <Box key="brand" layout={::ui::LayoutStyle{
        .direction = ::ui::FlexDirection::Row,
        .align_items = ::ui::AlignItems::Center,
        .gap = ::ui::StyleValue::points(6.0f)}}>
      <ScreenTitle key="b" variant={ScreenTitleVariant::Hero} value="Silencer" />
      <BodyText key="v" variant={BodyTextVariant::Detail} value={version_text} />
    </Box>
    <AppButton key="goback" controlId="LobbyGoBack"
      variant={AppButtonVariant::Chrome} label="Go Back"
      onPress={session.leave_to_menu} />
  </Box>;
```
`version_text` is already built: `use_text_storage("v%s", app.version)`. Golden
shows `v.00053` — the dot is cosmetic; keep `v%s` (matches existing).

### Edit C — new `AgentCard` sub-view (avatar + W/L/XP + Agents oval)
Add a file-local `AgentCard(...)` view. The avatar is a sprite tile (see §3 —
**net-new sprite** `agent_avatar`); fall back to a faint-green Box when not
baked. The W/L/XP block + `LV n` come from the agent summary string. DRAFT:

```cpp
::ui::UiElement AgentCard(const char *name, const char *wl, const char *xp,
                          uint32_t avatar_tex, float aw, float ah,
                          std::function<void()> on_agents) {
  ::ui::UiElement avatar = avatar_tex
    ? Box(BoxProps{.key="av",
        .layout={.width=::ui::Length::points(aw), .height=::ui::Length::points(ah)},
        .style=tokens::image_patch(avatar_tex)})
    : Box(BoxProps{.key="av",
        .layout={.width=::ui::Length::points(48.0f), .height=::ui::Length::points(48.0f)},
        .style=tokens::fill_patch(::ui::Color{20,40,72,255})}); // avatar-blue fallback
  return <Panel key="agent" variant={PanelVariant::Bordered}>
    <Box key="hdr" layout={::ui::LayoutStyle{.direction=::ui::FlexDirection::Row,
        .gap=::ui::StyleValue::points(8.0f)}}>
      {avatar}
      <Box key="stats" layout={::ui::LayoutStyle{.direction=::ui::FlexDirection::Column}}>
        <ScreenTitle key="n" variant={ScreenTitleVariant::Dialog} value={name} />
        <BodyText key="wl" variant={BodyTextVariant::Body} value={wl} />
        <BodyText key="xp" variant={BodyTextVariant::Detail} value={xp} />
      </Box>
    </Box>
    <AppButton key="agents" controlId="OpenAgents"
      variant={AppButtonVariant::Oval} size={AppButtonSize::Sm}
      label="Agents" onPress={on_agents} />
  </Panel>;
}
```
`on_agents` → reuse `session.open_character_create` (the roster picker is the
"Agents" surface). The current screen has a `Leave` button inside the agent
panel that is NOT in the golden — delete it; leaving the lobby is the title bar
`Go Back`.

### Edit D — new `StatColumn` sub-view (six LABEL : value rows)
The golden shows ENDURANCE / SHIELD / JETPACK / TECH SLOTS / HACKING / CONTACTS
with right-aligned values. This needs a **net-new data field** (see §3): six
`{label,value}` pairs on the `Characters` hook (or a new lightweight model).
DRAFT row helper:

```cpp
::ui::UiElement stat_row(const char *key, const char *label, const char *value) {
  return <Box key={key} layout={::ui::LayoutStyle{
      .direction=::ui::FlexDirection::Row,
      .justify_content=::ui::JustifyContent::SpaceBetween,
      .gap=::ui::StyleValue::points(8.0f)}}>
    <BodyText key="l" variant={BodyTextVariant::Detail} value={label} />
    <BodyText key="v" variant={BodyTextVariant::Strong} value={value} />
  </Box>;
}
```
Wrap the six rows in a fixed-width Column (no panel border in the golden — the
stats float beside the card). Each row text built with `use_text_storage` /
`copy_string` at stable call-order (compute all six every frame).

### Edit E — rename panel titles + add the SubPanel float
- `GameSelectPanel`: title `Games` → **`Active Games`** (golden). Keep the list
  but render rows via `ListRow`-style (selected = caret + Strong). The browse/
  join/spectate ActionRows can stay (functional), but the golden's Active Games
  panel is just the list — consider collapsing Prev/Next/Join into the row
  interaction later; for parity v1 keep actions but title it `Active Games`.
- `GameCreatePanel`: title stays `Create Game` (matches the floating
  `Create Game` oval label in the golden — see note).
- Chat panel: title `Chat` → **`Lobby`** (golden labels the chat panel "Lobby").
- Presence: the second `Online` heading + presence list becomes the **`In Lobby`**
  SubPanel float. Add `PanelVariant` for a smaller bordered float, or reuse
  `Bordered` at a narrower fixed width. Title `In Lobby`, body = presence list.
- `StagingPanel`: title `Staging` is fine (no golden for the staging variant of
  this screen — staging has its own golden `game_staging.png`).

Note the golden's top-center **`Create Game` oval**: in the golden this is a
floating oval button that opens the create form (the current `New Game` button
inside GameSelect). Promote it to a top-center floating `AppButton`
(Oval) labelled `Create Game`, wired to `*show_create = true`. The
GameSelect/GameCreate/Staging swap logic stays unchanged underneath.

### Edit F — new `EmblemStrip` sub-view (bottom decorative row)
Whole-sprite glowing green ovals, `SpaceAround`, ~7 cells, pinned bottom. Needs
a **net-new sprite** `emblem` (see §3). DRAFT:

```cpp
::ui::UiElement emblem_cell(const char *key, uint32_t tex, float w, float h) {
  return tex
    ? Box(BoxProps{.key=key,
        .layout={.width=::ui::Length::points(w), .height=::ui::Length::points(h)},
        .style=tokens::image_patch(tex)})
    : ::ui::empty();
}
// strip: Row, justify=SpaceAround, height ~22, 7 cells
```
If only one emblem sprite is baked, repeat it 7x (the golden emblems look
identical/near-identical). Use `image_patch` on a **Box** role (not Button) so
it never trips bug (c).

---

## 3. Data / hooks / sprites: exist vs net-new

### Exist (cite):
- **Hooks** — all data wiring is present:
  `use_games` (`hooks/use_games.h`: entries, bundled_maps, join/spectate/create),
  `use_lobby_chat` (`hooks/use_lobby_chat.h`: scrollback, presence, send),
  `use_staging` (`hooks/use_staging.h`: roster, ready/team/leave),
  `use_characters` (`hooks/use_characters.h`: `selected_summary`, `create`,
  `select`), `use_session` (`leave_to_menu`, `open_character_create`),
  `use_app` (`version`).
- **Primitives** — `Panel`(Bordered) `surfaces/panel.cppx`; `AppButton`
  (Oval/Chrome) `actions/app_button.cppx` + `app_button_variant.h`;
  `BodyText`/`ScreenTitle` `text/`; `ActionRow` `actions/action_row.cppx`;
  `Box`/`Input` `ui/components/`. `tokens::image_patch` / `fill_patch`
  (`components/tokens.h`). `::ui::children` / `::ui::copy_string` /
  `use_text_storage` (`ui/runtime/react.h`).
- **Sprite seam** — `use_chrome()` (`hooks/use_chrome.h`) + `ChromeTextures`
  table; bake in `game/ui/game_ui_pipeline.cpp:174-256` (`BakeChromeTextures`).

### Net-new (minimal additions):

1. **`agent_avatar` sprite (AgentCard tile).** No avatar sprite is baked today
   (ChromeTextures has none; bake table tops out at agency emblems). The golden
   avatar is the steel-blue Mars/agent portrait tile. *Minimal add:* identify
   the legacy avatar bank/idx (search `resources.cpp` / legacy lobby draw for
   the agent portrait — likely a bank-181-adjacent or a dedicated portrait
   bank), add `uint32_t agent_avatar; uint16_t agent_avatar_w/_h;` to
   `ChromeTextures` and one `bake(BANK, IDX, cppxChrome.agent_avatar, &w, &h)`
   call. **Fallback:** if the bank is unclear, ship the faint avatar-blue Box
   fallback (Edit C) — visually close, unblocks the rest. (Mark as a follow-up.)

2. **`emblem` sprite (EmblemStrip).** No emblem-strip sprite is baked. The
   agency emblems (bank 181 idx0-4) are baked but are agency-specific glyphs,
   not the uniform green ovals in the strip. The strip ovals strongly resemble
   the **oval button sprite** (bank 6 idx7/23/28) rendered small + decorative —
   *cheapest path:* reuse `chrome.oval_sm` as the emblem cell (whole-sprite Box,
   no label). If that reads wrong vs the golden's softer glow, add a dedicated
   `bake(6, IDX, cppxChrome.emblem, &w, &h)` for the actual emblem index. Start
   with `oval_sm` reuse; refine after the first pixdiff.

3. **StatColumn data (six stat values).** The per-stat values
   (ENDURANCE/SHIELD/JETPACK/TECH SLOTS/HACKING/CONTACTS) exist in lobby data
   (`Lobby::StatID` enum, used by `lobby.upgrade` at
   `game_ui_pipeline.cpp:530-541`) but are **not surfaced** to the screen —
   `Characters::selected_summary` is a single name/level/W-L string only.
   *Minimal add:* extend the `Characters` model with
   `struct StatRow { std::string label; std::string value; };
   std::vector<StatRow> stats;` (or a fixed `std::array<...,6>`), and populate
   it in `lobby_ui_model.cpp` `BuildLobbyPanels` alongside `lobby_agent`
   (the agency stat array `user->agency[idx]` already gives the values; the six
   labels are constant). Wire it through `use_characters`. **Fallback:** if the
   value source is awkward, render the six labels with placeholder `0` values
   from `selected_summary` parsing — but the proper path is one new field +
   one populate loop.

4. **`In Lobby` SubPanel.** No dedicated SubPanel variant; reuse
   `PanelVariant::Bordered` at a narrower fixed width (golden's float is just a
   smaller bordered box). No new primitive required.

5. **W/L + XP split strings.** `selected_summary` is one blob
   (`name\nagency Level n\nW wins / L losses`). The golden wants separate
   `WINS 0 / LOSSES 0` and `XP 0/100` lines + a bare name heading. *Minimal:*
   either split `selected_summary` in the screen, or add discrete
   `agent_name`, `agent_wl`, `agent_xp` fields to `Characters` populated in
   `BuildLobbyPanels` (cleaner — XP isn't in the current summary string at all,
   so it must be added at the source: `ag.level`/xp from `user->agency[idx]`).

---

## 4. Capture / verify recipe (headless)

The lobby is reached end-to-end through the real stack; the harness already
exists. Build first via the wrapper (never raw cmake):

```bash
bash clients/silencer/build.sh        # macOS/Linux
```

Then drive to LOBBY exactly as `tests/cli-agent/e2e/71_visual_regression_lobby.sh`
(authoritative) and `40_lobby_basic.sh` do:

1. Boot Go lobby (`services/lobby`) on an ephemeral port with
   `-version "$(silencer_version)"`.
2. Boot silencer `--headless --control-port N --lobby-host 127.0.0.1
   --lobby-port LOBBY`.
3. `wait_for_state MAINMENU` → `resize --w 960 --h 720` →
   `click "Connect To Lobby"` → `wait_for_state LOBBYCONNECT`.
4. Type creds: keys `a l i c e`, `tab`, `s e c r e t` → `wait_for_lobby_state
   AUTHENTICATING` → `click "Login/Create"`.
5. `wait_for_state CREATECHARACTER` → `click "Create New Character"` →
   `set_text Alias "Alice"` → `click AliasConfirm` → `click "Noxis"`.
6. `wait_for_state LOBBY` → `wait_for_widget "Send"`.
7. `cli screenshot --out lobby.png` (after `wait_frames --n 3`).

Pixdiff against golden: `tools/pixdiff/build/pixdiff lobby.png golden`. The
suite's `cap lobby_screen 1.0` blesses/compares with threshold 1.0%. To
re-bless after parity: `BLESS=1 bash tests/cli-agent/e2e/71_visual_regression_lobby.sh`.
For iterative eyeballing, capture to a temp PNG and `Read` it.

New `controlId`s introduced (`OpenAgents`, the promoted top-center
`Create Game` oval) must keep `inspect` labels the e2e harness already polls
(`Send`, `NewGame`, `CreateGame`, `StagingReady`, `LeaveGame`) — do NOT rename
those control ids or `40`/`53`/`71` break. The `New Game` → `Create Game`
*label* change: check `40_lobby_basic.sh:184` clicks `--label "New Game"`; if you
relabel the floating button to `Create Game`, update that click or keep the
control id `NewGame` + a11y label `New Game` while showing `Create Game` text.

---

## 5. Risks / unknowns

- **`agent_avatar` sprite bank/idx is unknown.** Highest-risk net-new asset.
  Mitigation: ship avatar-blue Box fallback first; locate the legacy portrait
  bank in `resources.cpp` as a follow-up. (The whole screen otherwise composes
  from existing sprites.)
- **Emblem strip sprite identity.** Reusing `oval_sm` may glow differently than
  the golden's emblems; may need a dedicated bank-6 index bake. Low risk —
  decorative, refine via pixdiff.
- **StatColumn value source.** Surfacing the six stat values cleanly requires
  touching `lobby_ui_model.cpp` + `use_characters.h` (cross-file). Stat order/
  labels must match the golden (ENDURANCE/SHIELD/JETPACK/**TECH SLOTS**/HACKING/
  CONTACTS — note "TECH SLOTS" maps to `STAT_TECHSLOTS`). XP `0/100` is not in
  today's summary — must be added at the source.
- **Flex-grow field name.** `::ui::LayoutStyle` flex semantics — confirm the
  grow/flex field (`screen_layout.cppx` only uses percent sizes). If no
  `flex_grow`, fall back to explicit percent heights for the three bands.
- **e2e label coupling.** Relabeling `New Game`→`Create Game`, `Chat`→`Lobby`,
  `Games`→`Active Games`, dropping the agent-panel `Leave` button: scenarios
  `40`/`53`/`71` poll specific labels/control-ids. Keep control ids stable;
  audit each `--label`/`wait_for_widget` before renaming.
- **`ScreenTitle` Dialog face for the brand/card name** renders in the Title
  face (bank-135 Heading face is BLANK — known bug). Use Hero/Dialog Title
  variants (visible green) as the existing screens do; do NOT point at
  `kFaceHeading`.
- **Density vs threshold.** The dense HUD has many small text rows; the 1.0%
  pixdiff threshold is tight. Expect 1-2 re-bless cycles once layout lands.
