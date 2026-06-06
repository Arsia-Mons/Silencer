# Create Game — sprite-parity implementation plan

Golden: `/tmp/goldens/create_game.png` (640x480). Screen is the `*show_create`
sub-view of `LobbyScreenView` in
`clients/silencer/src/client/ui/screens/lobby_screen.cppx`
(`GameCreatePanel`, lines 108-157). Data: `use_games()`
(`clients/silencer/src/client/ui/hooks/use_games.h` — `Games`,
`CreateGameRequest`, `bundled_maps`).

This is the SAME persistent lobby chrome as `lobby.png` / `game_staging.png`,
with the right column swapped to the create form. The chrome (titlebar, AgentCard
+ StatColumn, lower-left Lobby/chat panel, In-Lobby float, bottom emblem strip)
is shared scaffolding that the current `LobbyScreenView` does NOT yet render in
golden form — so this plan delivers the create-specific regions AND the shared
chrome those regions sit inside. The chrome pieces are net-new and should be
authored as reusable components (Lobby/Staging/Tech reuse them).

---

## 1. Golden layout breakdown (regions filling 640x480)

The whole frame is a dense, edge-to-edge HUD on a near-black starfield. There are
NO large dead margins — it is a tiled grid, ~4px gaps. Root is `ScreenLayout`
`Game` variant (already used), but its 24px padding + 18px gap must shrink to the
HUD's ~8px inset / ~4px gap to match.

Regions (approx fractions of 640x480):

```
┌───────────────────────────────────────────────────────────────┐
│ TITLE BAR: "Silencer  v.0033"  ............... [ Go Back ]      │  ~y0..18
├──────────────┬──────────────────────────┬─────────────────────┤
│ AgentCard    │  GAME OPTIONS            │  SELECT MAP          │  upper band
│ (avatar +    │  Security:  Medium       │  AL1ASGÇ.sil         │  ~y20..150
│  LV0 + W/L/  │  Min Level                │  AL1ASOÇ-gç.sil      │
│  XP + Agents)│  Max Level             2  │  CRIMSÇA.sil         │  Select Map
│ STAT COLUMN  │  Max Players           2  │  EASTÇG.sil          │  is a tall
│  ENDURANCE   │  Max Teams                │  PITTÇA.sil          │  scroll list
│  SHIELD      │                          │  STÇATÇ.sil          │  spanning into
│  JETPACK     │                          │  ...(scroll).....    │  the lower band
│  TECH SLOTS  │                          │  junkyard.sil        │
│  HACKING     ├──────────────────────────┤  (one row selected,  │
│  CONTACTS    │  LOBBY  (chat scrollback)│   bright)            │
│              │  ┌─ In Lobby ─┐          │                      │  lower band
│              │  │  AgentZero  │ (float)  ├─────────────────────┤  ~y150..430
│              │  └────────────┘          │  Game name           │
│              │                          │  New Game            │
│              │                          │  Password [optional] │
│              │  gg ready when you are    │           [ Create ]│  ~y420
├──────────────┴──────────────────────────┴─────────────────────┤
│  ◯   ◯   ◯   ◯   ◯   ◯   ◯   (EmblemStrip, evenly spaced)       │  ~y455..478
└───────────────────────────────────────────────────────────────┘
```

Flex grammar to fill the viewport:

- **Root**: `ScreenLayout Game` (Column, 100%x100%). Reduce padding to ~8px,
  gap ~4px. Children: titlebar row, body row, emblem strip row.
- **Title bar** (Row, width 100%, space-between, align center): left `Box`(Row)
  with `ScreenTitle Hero "Silencer"` + `BodyText Detail "v.0033"`; right
  `AppButton Chrome "Go Back"`. Already present (lines 318-328) — keep, but the
  brand+version must sit as one left group (they already do via the SpaceBetween
  box; the version currently centers — move it adjacent to the brand).
- **Body** (Row, `flex_grow:1`, width 100%, gap ~4px) — three columns:
  - **Col A (left, ~22%)**: `Box` Column gap 4 → `AgentCard` (net-new) then
    `StatColumn` (net-new). `flex_grow:0`, fixed width ~140.
  - **Col B (center, flex_grow:1)**: Column gap 4 → `GameOptionsPanel` (top),
    then `LobbyChatPanel` (Lobby title + scrollback + footer status), with the
    `In Lobby` float overlapping. For v1 parity, render Game Options on top and
    the Lobby/chat panel below it filling remaining height.
  - **Col C (right, ~32%)**: Column gap 4 → `SelectMapPanel` (tall, flex_grow:1,
    scroll list) on top, `GameNamePanel` (Game name field + Password field +
    Create button bottom-right) pinned to the bottom.
- **Emblem strip** (Row, width 100%, justify SpaceAround, height ~24): 7
  `agency_emblem` whole-sprite images. (Net-new; low priority — can be a
  follow-up since only 5 emblems are baked (`agency_emblem[0..4]`); v1 may show
  5 or repeat.)

Text roles (map to existing variants):

- Panel headings ("Game Options", "Select Map", "Game name") → `ScreenTitle`
  `Dialog` (already the panel-title convention).
- Stat/option **label** (left, dim, uppercase) → `BodyText Detail`.
- Stat/option **value** (right, brighter) → `BodyText Strong`.
- Map rows → `BodyText Body` (unselected) / `BodyText Strong` (selected) — same
  pattern as `game_line`/`roster_line` helpers already in the file.
- Footer "gg ready when you are" → `BodyText Detail` (muted tone).

---

## 2. Concrete ordered edits to `lobby_screen.cppx`

The current `GameCreatePanel` (lines 119-157) is a single bordered panel with a
name Input, a `Map: <name>` line, Prev/Next-map buttons, and Create/Back. The
golden splits create into TWO right-column panels (Select Map + Game name) plus a
shared **Game Options** panel, and drops the Prev/Next-map buttons in favor of a
selectable map LIST. Keep `controlId`s `GameName`, `MapPrev`, `MapNext`,
`CreateGame`, `CreateBack` so e2e `53_lobby_create_options_scroll.sh` stays green
(it asserts those five ids exist & are in-bounds — see test lines 197, 159-161).

### Edit 1 — Game Options panel (net-new component, screen-local)

A label:value `StatRow` list. Values come from `CreateGameRequest` defaults
(static for v1 — the hook has no per-field setters yet, §3). Add a screen-local
`stat_row` helper mirroring `game_line`:

```cpp
// label left (dim) + value right (bright), justified apart.
::ui::UiElement stat_row(const char *key, const char *label, const char *value) {
  ::ui::UiElement l = <BodyText key="l" variant={BodyTextVariant::Detail} value={label} />;
  ::ui::UiElement v = <BodyText key="v" variant={BodyTextVariant::Strong} value={value} />;
  return <Box key={key} layout={::ui::LayoutStyle{
      .direction = ::ui::FlexDirection::Row,
      .justify_content = ::ui::JustifyContent::SpaceBetween,
      .width = ::ui::Length::percent(100.0f)}}>
    {l}{v}
  </Box>;
}

::ui::UiElement GameOptionsPanel() {
  return <Panel key="opts" variant={PanelVariant::Bordered} size={PanelSize::Auto}>
    <ScreenTitle key="h" variant={ScreenTitleVariant::Dialog} value="Game Options" />
    {stat_row("sec", "Security:",   "Medium")}
    {stat_row("min", "Min Level",   "")}
    {stat_row("max", "Max Level",   "99")}
    {stat_row("mp",  "Max Players", "24")}
    {stat_row("mt",  "Max Teams",   "6")}
  </Panel>;
}
```

(Values match `CreateGameRequest` defaults: security=2→"Medium", max_level=99,
max_players=24, max_teams=6. The golden shows "Medium" + small "2"s; exact value
text is cosmetic — match the golden labels, real values follow when setters land.)

### Edit 2 — Select Map panel (replace Prev/Next with a list)

Reuse the existing `game_line` row pattern over `games.bundled_maps`, driven by
the existing `*map_index` cursor + a `ScrollView` for the dense list. Keep
`MapPrev`/`MapNext` as the cursor controls but render them compactly (or move the
cursor to clicking rows — but keep the two ids present for the e2e guard). Draft:

```cpp
::ui::UiElement SelectMapPanel(const std::vector<std::string> &maps, int sel,
                               std::function<void()> on_prev,
                               std::function<void()> on_next) {
  std::vector<::ui::UiElement> rows;
  for (int i = 0; i < (int)maps.size(); ++i) {
    const bool on = i == sel;
    std::string s = std::string(on ? "> " : "  ") + maps[i];
    rows.push_back(game_line(::ui::copy_string(("m" + std::to_string(i)).c_str()),
                             ::ui::copy_string(s.c_str()), on));
  }
  ::ui::UiChildren map_rows = ::ui::children(rows);
  return <Panel key="map" variant={PanelVariant::Bordered}>
    <ScreenTitle key="h" variant={ScreenTitleVariant::Dialog} value="Select Map" />
    {map_rows}
    <ActionRow key="mapnav">
      <AppButton key="prevmap" controlId="MapPrev" label="Prev" onPress={on_prev} />
      <AppButton key="nextmap" controlId="MapNext" label="Next" onPress={on_next} />
    </ActionRow>
  </Panel>;
}
```

If the bundled-map list is long, wrap `{map_rows}` in `ScrollView` (virtualized,
`row_height` ~ `tokens::kLineBody`) so it scrolls inside the panel as the golden
shows (~12 visible rows + clipping). Confirm `ScrollView` height is constrained
(`flex_grow:1` on the panel column) so it clips rather than overflows.

### Edit 3 — Game name panel (name + password + Create)

```cpp
::ui::UiElement GameNamePanel(const char *name, bool can_create,
                              std::function<void(const std::string&)> on_name,
                              std::function<void(const std::string&)> on_pw,
                              std::function<void()> on_create,
                              const char *pw) {
  return <Panel key="gname" variant={PanelVariant::Bordered}>
    <BodyText key="lbl" variant={BodyTextVariant::Detail} value="Game name" />
    <Input key="name" id="GameName" accessibility={{.label = "Game Name"}}
           value={name} onChange={on_name} />
    <BodyText key="pwl" variant={BodyTextVariant::Detail} value="Password [optional]" />
    <Input key="pw" id="GamePassword" accessibility={{.label = "Password"}}
           value={pw} onChange={on_pw} />
    <Box key="row" layout={::ui::LayoutStyle{
        .direction = ::ui::FlexDirection::Row,
        .justify_content = ::ui::JustifyContent::FlexEnd,
        .width = ::ui::Length::percent(100.0f)}}>
      <AppButton key="create" controlId="CreateGame" variant={AppButtonVariant::Oval}
                 disabled={!can_create} label="Create" onPress={on_create} />
    </Box>
  </Panel>;
}
```

Add a `*create_pw` `use_state<std::string>` next to `create_name` (line 214) and
thread it into `CreateGameRequest.password` in `cp.on_create` (line 287-296;
`req.password = *create_pw;`). `req.password` already exists in the struct
(use_games.h:30).

### Edit 4 — recompose `GameCreatePanel` from the three sub-panels

Replace the body of `GameCreatePanel` (119-157) so the create sub-view returns a
fragment placing Game Options in the center column and Select Map + Game name in
the right column. Because the create form shares Col A (AgentCard) + the
center/right grid with the browse/staging views, the cleanest shape is to lift
the three-column body INTO `LobbyScreenView` and let the `right`/center contents
swap by mode. Concretely:

- Keep the `*show_create` branch (line 277) but have it set the **center column =
  GameOptions + chat**, **right column top = SelectMap**, **right column bottom =
  GameName**. The browse mode keeps today's `GameSelectPanel`; staging keeps
  `StagingPanel`.
- Keep `CreateBack` somewhere reachable (golden has no explicit Back — the
  titlebar `Go Back` covers it, but the e2e guard wants the id present). Render a
  small `AppButton controlId="CreateBack"` in the Game name panel footer or as a
  Ghost next to Create.

### Edit 5 — net-new shared chrome components (AgentCard, StatColumn, EmblemStrip)

These are shared with Lobby/Staging — author under
`clients/silencer/src/client/ui/components/surfaces/` (new `agent_card.cppx/.hx`,
`stat_column.cppx/.hx`) so other screens reuse them. Each is a `Panel Bordered`
with structured children:

- **AgentCard**: Column → avatar (whole-sprite `image_patch` over a baked avatar
  texture; if none baked yet, a Sunken `Box` placeholder), `BodyText Strong`
  "LV 0", a W/L/XP `BodyText Detail` block, and an `AppButton Oval "Agents"`.
  Data: `use_characters().selected_summary` already supplies an agent summary
  string (line 207, 239-241) — split it into LV/W/L/XP rows or render the summary
  as-is for v1.
- **StatColumn**: Column of `stat_row(label,value)` for ENDURANCE/SHIELD/JETPACK/
  TECH SLOTS/HACKING/CONTACTS. Net-new data (§3) — values are character
  attributes not currently in `use_characters`; for v1 render the labels with
  placeholder/zero values, then wire real stats when the hook grows.
- **EmblemStrip**: Row SpaceAround of `agency_emblem[i]` whole-sprite images from
  `use_chrome()` (already baked, indices 0-4). Lowest priority.

> Scope guard (CLAUDE.md rule 6): AgentCard/StatColumn/EmblemStrip are shared
> chrome. If this ticket is strictly the create FORM, deliver Game Options +
> Select Map + Game name (Edits 1-4) and stub Col A / emblem strip, leaving the
> full AgentCard to the Lobby ticket. Confirm scope before building Edit 5.

---

## 3. Data / hooks / sprites: exist vs net-new

EXISTS (cite):

- `use_games()` → `Games{entries, bundled_maps, join/spectate/create, can_join}`
  and `CreateGameRequest{name, map, password, security, min_level, max_level,
  max_players, max_teams, spectatable}` — `use_games.h:14-55`. The create-form
  needs: `bundled_maps` (map list ✓), `create_game` intent ✓, `password` field ✓.
- Screen-local cursors `*create_name`, `*map_index`, `*show_create` —
  `lobby_screen.cppx:214-217`. Clamping logic for `map_index` already present
  (227-231).
- `use_characters().selected_summary` for the agent label — used at 207/239.
- Primitives: `Panel` (Bordered ✓, surfaces/panel.cppx), `ScreenTitle`,
  `BodyText` (Body/Strong/Detail variants ✓, body_text.hx:9), `Input`,
  `AppButton` (Oval/Chrome ✓, app_button_variant.h), `ActionRow`, `ScreenLayout`
  (Game ✓), `Box`, `ScrollView`.
- Sprites via `use_chrome()`: oval/chrome buttons, `chrome_panel`, `starfield`,
  `agency_emblem[0..4]` — `use_chrome.h:32-75`. No bake work needed for the
  create form itself (panels are vector Bordered; buttons are baked ovals).

NET-NEW (and minimal add):

- **Game Options values as real, editable settings** — the hook only carries
  `CreateGameRequest` DEFAULTS, no per-field setters. v1 renders the golden's
  static labels/values (cosmetic parity). Real Security/Level/Players/Teams
  cycling needs new setters on `Games` (or screen-local state mirroring
  `CreateGameRequest`) — defer unless asked; the golden shows them as a static
  options readout anyway.
- **Password field** — add `*create_pw` state + thread into `req.password`
  (struct field exists). ~3 lines.
- **`stat_row` / `GameOptionsPanel` / `SelectMapPanel` / `GameNamePanel`** —
  net-new screen-local helpers in `lobby_screen.cppx` (drafts above).
- **AgentCard / StatColumn / EmblemStrip** — net-new shared components; agent
  stat values are not in `use_characters` yet (only `selected_summary`). v1 uses
  the summary string + placeholder stat values. NO new sprite bakes required
  (emblems already baked; avatar can be a placeholder Box until an avatar bake
  lands).

No serializer / wire / lobby-protocol changes (the create intent already exists
and round-trips per `53_lobby_create_options_scroll.sh`).

---

## 4. Capture / verify recipe (headless)

Reuse `tests/cli-agent/e2e/53_lobby_create_options_scroll.sh` verbatim to reach
the screen — it boots the Go lobby + a headless silencer, auths, creates a
character, lands in LOBBY, then clicks "New Game" to mount the create form
(lines 47-161). To capture the golden-parity screenshot, after it reaches the
create panel (`wait_for_widget "CreateBack"`, line 161) add:

```bash
cli --port "$CTRL_PORT" resize --w 640 --h 480 >/dev/null
cli --port "$CTRL_PORT" wait_frames --n 3 >/dev/null
cli --port "$CTRL_PORT" screenshot --out /tmp/create_game_current.png
```

`screenshot --out` is documented in `shared/skills/cli/SKILL.md:80`. Diff
`/tmp/create_game_current.png` against `/tmp/goldens/create_game.png` visually
(Read both). Build first via `clients/silencer/build.sh` (never raw CMake).
Then re-run the e2e suite: `bash tests/cli-agent/run.sh` — `53_`, `31_`, `40_`
must stay green (they gate the create/lobby flow + the five create controlIds).

Self-verification: drive it via the control socket + screenshot above; do NOT
depend on a human browser.

---

## 5. Risks / unknowns

1. **Three-column HUD density vs. flex defaults.** `ScreenLayout Game` has 24px
   padding / 18px gap and the body is an `ActionRow` (which centers/rows its
   children). Filling 640x480 edge-to-edge with ~4px gaps needs explicit Box
   columns with `flex_grow` + reduced padding — `ActionRow` (line 329) is the
   wrong container for the body and must become a `Box` Row. Risk: collapsing /
   off-canvas controls (the e2e in-bounds guard will catch this).
2. **Bordered panel min-widths.** `Panel` Bordered defaults to 260px width
   (panel.cppx:86). Three 260px columns overflow 640. Must pass `PanelSize::Auto`
   + `flex_grow`/`flex_basis` so columns size to the grid, not the canonical
   width. Verify `Panel` honors a width override when `size==Auto` (it only
   overrides when `size != Auto`, panel.cppx:117 — so Auto keeps the variant's
   260; need to set width via an outer Box or extend Panel to accept layout).
3. **ScrollView clipping height.** Select Map must clip+scroll, not grow the
   column. Needs a constrained height (`flex_grow:1` parent + `overflow` clip).
   Unproven for this list length; fall back to a fixed-height Box if ScrollView
   misbehaves.
4. **AgentCard/StatColumn data gap.** No per-stat character data in
   `use_characters` — v1 placeholders risk looking empty vs. the golden's
   populated ENDURANCE/…/CONTACTS. Decide scope (Edit 5 note) before building.
5. **Heading face is BLANK** (known bug) — keep panel titles in `ScreenTitle
   Dialog` (Title face), NOT the bank-135 Heading face.
6. **e2e controlId contract**: must preserve `GameName, MapPrev, MapNext,
   CreateGame, CreateBack` (test line 197). Redesigning the map picker as a list
   must still expose `MapPrev`/`MapNext` or the guard fails.
