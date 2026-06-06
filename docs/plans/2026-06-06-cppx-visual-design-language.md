# cppx Visual Design Language (origin/main parity)

> Authoritative spec for recreating Silencer's origin/main UI in the cppx
> retained engine. SPRITE-first; nine-slice is the default scale mode;
> whole-sprite fixed-aspect for sculpted chromes (HUD bezels, controls panel,
> Mars backdrop, logo, emblems); clean tokens + composable primitives + Yoga
> flex filling the 640x480 viewport edge-to-edge.
>
> The defining look is **monochrome green phosphor on near-black**. The current
> cppx token theme (cool-blue `#9FC9FF` accent, near-white `#E0E7F1` text,
> slate `#10141C` panels, grey `#565E6F` borders) is a wholesale palette
> regression introduced by SIL-84 and must be replaced. The one non-green
> accent is the in-game economy/files cluster (cool-blue) and the agent
> portrait/silhouette art.

---

## 1. Design tokens

All screens render at a logical **640x480**. Sprites bake index-0 to
transparent; index>0 to `palette[idx].rgb` with `a=255`, premultiplied at
upload. Colors below are the green-phosphor family sampled from the goldens;
they REPLACE the slate/blue theme tokens for every menu/lobby/character-create
surface. The in-game HUD adds its own LCD palette.

### 1.1 Colors

**Green phosphor family (menu / lobby / character-create / chat / scoreboard):**

| Token | Hex | Use |
|---|---|---|
| `green-bright` | `#3CFF3C` / `#5CD05C` | primary strokes, titles, focused labels, caret, active borders |
| `green-label` | `#88C888` | oval/title label text (idle) |
| `green-label-bright` | `#B8E8B8` | focused oval label |
| `green-mid` | `#4FB867` / `#33CC33` | body text, stat values, log lines, idle oval label |
| `green-dim` | `#2E7D45` / `#3C8C3C` | idle borders, version footer, "OR", dim/unselected rows, inner border stroke |
| `green-disabled` | `#1E4A2C` | disabled list rows / buttons |
| `oval-stroke-idle` | `#3CA03C` | oval outline, idle |
| `oval-stroke-focus` | `#5CD05C` | oval outline, focused (brighter, NOT thicker) |
| `oval-glow` | additive ~1px green halo | oval stroke + logo bloom (sprite-baked) |
| `oval-fill-transparent` | `{0,0,0,0}` | main-menu oval interior (starfield shows through) |
| `oval-fill-faint` | `#0A2A0A` / `#0C2A0C` | options/agency oval interior (faint dark green) |
| `oval-fill-selected` | `#1C5C1C` | selected agency-row oval fill |
| `toggle-on-fill` | `#22C04C` | filled half of round split-disc indicator |
| `toggle-off` | `#1E4A1E` | hollow outline half |
| `chrome-border` | `#2E8B2E` | double-stroke green frame (controls panel, agency panels) |
| `surface-menu` | `#000000` | base frame field (NOT slate) |

**In-game HUD LCD palette (overlay over live world):**

| Token | Hex | Use |
|---|---|---|
| `hud-green` | `#3DE83D` | own-player data (ammo/weapon), chat, scoreboard glyphs |
| `hud-green-dim` | `#1E7A1E` | chat message body, inactive slot outlines |
| `hud-blue` | `#3A6BFF`..`#9FC9FF` | economy/files/credits, status dots, team-badge stroke, agent silhouette |
| `hud-red` | `#E03030` | warnings (FATIGUE), enemy blips, health splat, secondary ammo count |
| `hud-amber` | `#A0561E` | radar minimap schematic geometry |
| `hud-black` | `#000000` @ ~85% | scoreboard bar fill, radar viewport, LCD lozenge |
| `team-tint-red` / `team-tint-blue` | washes | left / right dash-half color coding |
| `blip-ally` / `blip-enemy` | `#FFFFFF` / red | radar blips |

**Art-not-token:** `mars-orange ≈ #B0402A` dithered (planet sprite),
`avatar-blue` (agent portrait tile).

**DELETE / REMAP (the SIL-84 regression):** `kAccent #9FC9FF`,
`kTextTitle #E0E7F1`, `kBorderPanel #565E6F`, `kSurfacePanel #10141C`. None of
the green-phosphor screens use blue/slate/grey.

### 1.2 Spacing scale

Base steps (640x480): `2 / 4 / 6 / 10 / 16`.

| Token | px | Use |
|---|---|---|
| `gap-stack-menu` | 33 | main-menu oval stack vertical gap |
| `gap-stack-options` | 18 | options sub-menu oval gap |
| `gap-row` | 12 | Save/Cancel, label↔indicator |
| `gap-agency-row` | 18–24 | agency list rows (generous) |
| `gap-roster-row` | 8 | agent roster rows (tight) |
| `row-height-binding` | 52 | controls binding rows |
| `frame-inset` | 8 (chrome) / 12 (cc panels) | panel inset from black edge |
| `panel-gutter` | 8–12 | between split panels (over scrollbar seam) |
| `panel-pad` | 4–6 (lobby) / 16–20 (cc) | panel interior padding |
| `title-y` | 120 | audio/display floating-title baseline |
| `footer-inset` | `{left 10, bottom 15}` | version footer |
| `titlebar-h` | 18 | lobby title bar |
| `emblem-strip-h` | 22 | lobby bottom emblem strip |
| `hud-top-band-h` | 40 | top HUD bezel |
| `hud-bottom-band-h` | 95 | bottom HUD dashboard |
| `scoreboard-inset` | ~45 L/R/T, 70 tall | F1 player-list bar |
| `stat-col-gutter` | ~48 | scoreboard column spacing |

### 1.3 Radii

- `radius-oval` = full capsule (height/2). SPRITE-baked, not a vector token.
- `radius-panel` ≈ 12–16 (cc double-border panels). Sprite-baked.
- `radius-rect-button` ≈ 2–4 (lobby-connect rect buttons, near-square).
- chrome-panel corners = baked 45° notch/bracket detail.
- `radius-slot` ≈ 3–4 (weapon slot cells, ammo LCD box).

### 1.4 Border widths

- oval stroke ~1px (baked, brighter—not thicker—on focus).
- panel single-line ~1px (lobby/cc dialog).
- double-border: outer 2–3px + inner 1px, ~3px apart (cc agency panels,
  controls chrome).
- wire frames (chat, buy/tech, ammo box, slot cells): 1px green.
- emblem badge: 1px blue.

### 1.5 Typography roles + sizes

Pixel/bitmap HUD face, crisp (no AA softness). Greens applied to existing
faces. Sizes are at 640x480 logical scale.

| Role | Size | Color | Use |
|---|---|---|---|
| `title-caps` | 18–20px caps | `green-bright` | cc pill title bars (SELECT AGENT/AGENCY, SILENCER ALIAS) |
| `hero` | ~14px | `green-bright` | lobby brand `Silencer`, weapon name `Blaster` |
| `heading` | 13–17px | `green-label` | screen titles (Audio/Display Options), title tabs, section headings, lobby labels |
| `oval-label` | ~13px | `green-label` idle / `green-label-bright` focus | oval button labels |
| `body` | 11–12px | `green-mid` | stat rows, prose, list rows, agency names |
| `mono/message` | 10–11px | `green-mid` | connection-log lines, field values, chat |
| `stat-small` | ~8px | green/blue | lobby stat labels/values, scoreboard `L:0`, HUD `FILES`/`CREDITS` |
| `detail/footer` | ~5–7px | `green-dim` | version footer, status pills |
| `lcd-ammo` | 16–18px | `hud-green` | ammo digits `99` (monospace LCD) |

### 1.6 Sprite render-mode summary

- **Nine-slice (default):** oval buttons, chrome button, pill title bars,
  bordered/double-border panels, input field chrome, scrollbar track,
  weapon-slot cells, ammo LCD box, status pills, chat/buy wire-rect frames,
  HUD oval rows.
- **Whole-sprite fixed-aspect (the exceptions):** controls chrome panel
  (bank 7 idx 5), lobby-connect dialog chrome (bank 40 idx 2), HUD bezels
  (top+bottom), radar viewport, status lamp, emblem badges/splat, agent
  silhouette, status dots, wordmark logo, agency emblems, Mars/planet backdrop.
- **Whole-sprite stretch:** starfield+Mars backdrop (bank 6 idx 0), full-bleed.
- **Pure opaque rect:** scoreboard bar.

---

## 2. Primitive component catalog

Each primitive: visual spec / sprite source / nine-slice vs whole / variants /
states. See §6 for the full bank/idx table.

### P1. `OvalButton` (the signature primitive)
- **Visual:** green-outline capsule (fully rounded ends), ~1px stroke + faint
  outer glow, centered mint-green label.
- **Sprite:** bank 6 — `oval_md` (idx7, 196x33), `oval_sm` (idx28, 112x33),
  `oval_lg` (idx23, 220x33). Pick by content width. **NINE-SLICE.**
- **Variants:** interior `transparent` (main menu — starfield through) vs
  `filled` (`oval-fill-faint`, options/agency); `as-field` (left-aligned
  editable text + caret instead of centered label — agency alias well).
- **States:** idle = `oval-stroke-idle`; focused/hover = `oval-stroke-focus` +
  stronger glow + `green-label-bright` (focus pins to max-bright, NOT free-run);
  selected (agency) = `oval-fill-selected`; disabled = `green-disabled`.

### P2. `WordmarkLogo`
- Mint-green segmented "SILENCER" with circle-bar endcaps, glow. Reveal/hold/
  retract animation.
- **Sprite:** bank 208 reveal frames (idx 60, 58, 56, 54, 52, 50, 48, 46).
  **WHOLE-SPRITE** native height, width flex-shrinks. Main menu only.

### P3. `Starfield` (backdrop)
- Black field + scattered red/white stars + baked red Mars planet (center
  ~x=480 y=240 r~150, bleeding off right edge). Mars is part of the sprite, not
  a separate element.
- **Sprite:** bank 6 idx 0. **WHOLE-SPRITE stretch** to 640x480 root. Every
  menu screen.

### P4. `ChromePanel` (controls-screen frame) — nine-slice EXCEPTION
- Double green border, notched/beveled 45° corners with bracket details, baked
  inner wells. Sized to native dims (~628x441), centered, content overlays.
- **Sprite:** bank 7 idx 5. **WHOLE-SPRITE fixed-aspect** — do NOT stretch or
  the corner brackets/double-line border misalign.

### P5. `TitleTab` / `TitleBar` (pill header)
- Rounded lozenge/pill seated on a panel's top edge, centered all-caps heading.
- **Sprite:** pill chrome. **NINE-SLICE horizontal** (fixed end-caps, stretch
  middle). Used by controls ("Configure Controls"), cc SELECT AGENT/AGENCY,
  SILENCER ALIAS.

### P6. `ScreenTitle` (free-floating text)
- Plain mint-green heading-face text, centered near top of content (~y=120).
  NOT a sprite. Used by Audio/Display ("Audio Options"/"Display Options").
  Main menu, Options sub-menu, and cc roster have NO floating title.

### P7. `ToggleIndicator` (round split-disc)
- ~36px round disc split vertically into half-moons. ON = right half solid
  bright green, left half hollow-dark. OFF = both hollow. Sits RIGHT of a
  setting's label oval.
- **Sprite:** bank 6 — OFF idx 12|13, ON idx 14|15. Rendered on a **Box** role
  via `image_patch` (does NOT hit the control-fill bug). Size to native ~36px,
  not the 16px square fallback.

### P8. `SettingRow`
- Horizontal: label `OvalButton` (left) + `ToggleIndicator` (right), gap ~12.
  NO surrounding panel. Audio (1 row), Display (2 rows).

### P9. `BindingRow` (controls)
- Horizontal: right-aligned label text (col1 ~140px) + value `OvalButton`
  (col2, primary binding) + dim `OR` text (col3) + empty `BindingWell` (col4,
  alternate). Preset row is degenerate: label + single value oval, no OR/well.

### P10. `BindingWell`
- Empty rounded green-outline well holding/awaiting an alternate binding. Same
  green outline + faint fill as a filled oval, blank.

### P11. `ScrollBar` (composite)
- Vertical: top rounded end-cap nub + long double-line track + draggable thumb
  (capsule w/ grip) + bottom end-cap nub.
- **Sprite:** track = **NINE-SLICE vertical**; nubs + thumb = **WHOLE-SPRITE**.
  Used by controls list, Select Map, Tech list, cc roster (center seam), cc
  agency list (inside left panel).

### P12. `ActionRow`
- Horizontal row of 2 OvalButtons (`Save`+`Cancel`), centered, gap ~12.
  Standard footer on Audio/Display/Controls.

### P13. `VersionFooter`
- Absolute bottom-left text, dim-green tiny face. Main menu (`Silencer v00053`).

### P14. `Panel` (Bordered / DoubleBorder)
- **Bordered (lobby):** thin dim-green ~1px outline, transparent fill
  (starfield through), title band top. **NINE-SLICE.**
- **DoubleBorder (cc):** heavy double rounded-rect (outer thick + inner thin)
  with corner flourishes + mid-edge nubs, dark/transparent fill, bright green
  strokes + glow. **NINE-SLICE** (fixed corners, stretch edges).
- States: static; focused panels show brighter border.

### P15. `SubPanel / Float` (`In Lobby`, `Pregame`)
- Smaller bordered box overlapping panels below it; same chrome as Panel,
  visually layered on top. **NINE-SLICE.**

### P16. `AgentCard`
- Bordered cell: square avatar sprite tile (steel-blue Mars portrait) + `LV n`
  caption + W/L/XP block (`WINS 0 / LOSSES 0 / XP 0/100`) + `Agents` oval.
- avatar = **WHOLE-SPRITE**; frame = nine-slice; button = nine-slice. Static.

### P17. `StatColumn / StatRow`
- Vertical list of `LABEL` (left, dim, uppercase) : `value` (right, brighter).
  Agent attributes (`ENDURANCE…CONTACTS`) and `Game Options` label:value.

### P18. `ListRow`
- Single line: text left, optional right suffix (`[$3]`, map ext). Selected =
  bright + caret/highlight; disabled = dim. Select Map, Tech list, Active
  Games, roster.

### P19. `Field` (Input)
- Inset bordered text box, bright value + caret on focus. **NINE-SLICE** inset
  chrome — OR baked into a dialog sprite (lobby-connect), where the component
  contributes only chromeless text + caret over the baked well.

### P20. `EmblemBadge` / `EmblemStrip`
- One glowing green oval emblem, fixed aspect, soft outer glow.
  **WHOLE-SPRITE** (NOT nine-sliced). Strip = full-width bottom row of ~7,
  evenly spaced (SpaceAround), pinned to bottom edge.

### P21. `RectChromeButton` (lobby-connect)
- Small rectangular framed button (near-square corners), centered green label.
  Distinct from OvalButton. `Login/Create`, `Cancel`.
- **Sprite:** rect chrome (bank 7 idx 24 idle / 28 focus). NINE-SLICE caps
  `{l12,r12,t4,b4}`.

### P22. `DialogPanel` (lobby-connect) — whole-sprite EXCEPTION
- Single thin rounded-rect green border, baked input wells. Simpler than the
  cc double-border. **Sprite:** bank 40 idx 2. **WHOLE-SPRITE fixed-aspect**
  (wells baked in).

### P23. `StatRow` (Advantages) + `SectionHeading` (cc agency detail)
- StatRow: stat name (left) + signed bonus `[+N]`, body green. No bar/grid.
- SectionHeading: left-aligned subheading (`Advantages`, `Description`),
  brighter/larger than body.

### P24. `PlanetBackdrop` (Mars, cc panels)
- Large red-orange dithered hemisphere, bottom-right anchored, clipped to panel
  content region, behind text. **WHOLE-SPRITE fixed-aspect.**

### In-game primitives

| Primitive | Render mode | Fill | Border | States |
|---|---|---|---|---|
| **HudBezel** (top+bottom) | whole-sprite, edge-anchored, NO nine-slice | sculpted console, transparent well cutouts | sculpted edge | team-tint red left / blue right |
| **HudWell** (weapon/radar/files) | inset region of bezel | dark blue-black | implied | — |
| **WeaponSlotCell** | small rounded-square sprite, nine-slice | dark | green stroke | active = brighter + value; empty = dim |
| **AmmoLcdBox** | green-outlined rounded box, nine-slice | transparent/dark | bright green | green LCD digits; red secondary in black lozenge |
| **StatusPill** (FATIGUE/LOW FUEL) | small pill, nine-slice | transparent | red(warn)/blue(info) | lit = colored / off = hidden |
| **RadarViewport** | black rounded-oval, whole-sprite | black | dark rim | dynamic minimap + blips |
| **StatusLamp** (top-right) | round green ring, whole-sprite | green | green ring | on/off brightness |
| **EmblemBadge** (team) | square blue-outline tile + glyph | dark | blue | chevron + team number |
| **EmblemSplat** (health/agent) | small red insignia, whole-sprite | red | none | scoreboard row marker / HUD status |
| **StatusDots** | row of blue dots | blue | none | lit/unlit |
| **ScoreboardBar** | opaque black rect (rounded top) | ~85% black | none | only opaque overlay |
| **ScoreboardRow** | flex: emblem + name + N stat cols | transparent | none | local = emblem marker; fixed-width cols |
| **ChatBox** | green wire-rect, bottom-right, content-sized | transparent | 1px bright green | grows with log length |
| **ChatLogLine / ChatInputLine** | green text | transparent | none | name/tag = emphasis; `[ALL]:`/`[TEAM]:` tag + caret |
| **AgentSilhouette** | blue body figure, whole-sprite | blue ghost | none | — |
| **OverlayFrame / HudOvalRow** (buy/tech) | green wire-rect / oval row, nine-slice | transparent/dark | bright green | selected = cursor/brighter; locked = dimmed |

---

## 3. Per-screen layout grammar

Every menu/lobby/cc screen: full-bleed `Starfield` root (P3). Content uses Yoga
flex to fill 640x480 edge-to-edge — no large dead margins. Goldens noted inline.

### 3.1 Main menu (`mainmenu.png`)
- Single horizontal band, vertically centered. LEFT half = `WordmarkLogo`
  (~x=0..345, baseline at mid-height). RIGHT half = staggered stack of 4
  `OvalButton` (transparent interior) over Mars.
- Order top→bottom: **Tutorial, Connect To Lobby, Options, Exit**. Horizontal
  zig-zag stagger (Connect-To-Lobby widest, pokes left; Exit shifted left),
  vertical gap ~33px. `VersionFooter` bottom-left.
- **Golden notes:** oval interior MUST be transparent (verify bank-6 art is
  hollow outline). Stagger is *horizontal* offset — current cppx mixes a
  vertical margin on `w2`; re-tune to this golden.

### 3.2 Options (`options.png`)
- Vertically-centered column of 4 `OvalButton` (faint dark-green fill),
  centered-left (~x=318) of Mars, equal width ~195, gap ~18. NO title, NO
  panel/chrome.
- Order: **Controls, Display, Audio, Go Back**.
- **Golden notes:** current cppx is reversed (Audio/Display/Controls), has an
  extra "Options" ScreenTitle + "Unsaved changes" subtitle + Done/Cancel
  ActionRow — ALL absent in golden. Remove; commit/revert folds into Go Back.

### 3.3 Audio Options (`options_audio.png`)
- `ScreenTitle` "Audio Options" (~y=120). One `SettingRow`: `Music` oval
  (~x=282 w~215) + `ToggleIndicator` (~x=440, ON=right half green). `ActionRow`
  `Save`+`Cancel` (~y=224).
- **Golden notes:** current wraps in Hero `Panel` (golden has NONE — floats on
  starfield), uses single "Back" (golden = Save+Cancel), title "Audio" not
  "Audio Options", indicator is 16px square (golden ~36px round disc).

### 3.4 Display Options (`options_display.png`)
- Same as Audio, 2 `SettingRow`s: `Fullscreen` + indicator, `Smooth Scaling` +
  indicator (both right-half lit). `ActionRow` Save+Cancel (~y=277).
- **Golden notes:** same family as Audio — no panel, Save+Cancel, "Display
  Options" title, round-disc indicators.

### 3.5 Configure Controls (`options_controls.png`) — THE CHROME SCREEN
- `ChromePanel` (P4, whole-sprite, ~x8..632/y8..472). `TitleTab` "Configure
  Controls" on top edge.
- Inner: `BindingRow`s — Preset row (`Preset:` + `Default` oval, no OR/well);
  then `Move Up/Down/Left/Right…` (right-aligned label ~x75 + value oval ~x323
  + dim `OR` ~x415 + `BindingWell` ~x460..555). `ScrollBar` far right
  (~x555..575). Footer: `Save` (~x215) + `Cancel` (~x423) ovals ~y420. Row
  height ~52px.
- **Golden notes:** current cppx uses plain Overlay (NO chrome frame — the
  single biggest miss), title is plain "Controls", rows are single text rows
  (no oval/second well), has a Cycle-Preset button + a 4-deep rebind ActionRow
  stack (Rebind/Clear/Confirm/Cancel/Save/Revert/Back) — all a migration
  invention. Parity = chrome frame + two-column binding rows + single Save+
  Cancel + visible scrollbar.

### 3.6 Lobby (`lobby.png`)
- Persistent chrome: `Starfield` + `TitleBar` (brand `Silencer` + `v.0033`
  suffix left, `Go Back` hard-right) + upper-left `AgentCard`+`StatColumn`
  (ENDURANCE/SHIELD/JETPACK/TECH SLOTS/HACKING/CONTACTS) + bottom `EmblemStrip`
  (~7 emblems).
- Body (asymmetric tiled HUD, ~4px gaps): top-center floating `Create Game`
  oval; top-right `Active Games` Panel (empty list); lower-left `Lobby` Panel
  (chat scrollback); lower-center `In Lobby` SubPanel float (present agents).
- **Golden notes:** current has NO persistent chrome, slate buttons, wrong
  titles (Agent/Chat/Games), no StatColumn/avatar/emblem strip, lists are plain
  text, version mis-placed. Full reframe needed.

### 3.7 Create Game (`create_game.png`)
- Same persistent chrome. Right region = three panels: top-center `Game Options`
  (StatRows: Security/Min Level/Max Level/Max Players/Game Type, right-aligned
  values); top-right `Select Map` (dense map `ListRow`s + ScrollBar, one
  selected); bottom-right `Game Name` (`Field` `New Game` + `Password
  (optional)` field + `Create` oval bottom-right).

### 3.8 Game Staging (`game_staging.png`)
- Persistent chrome. Left `AgentCard`#1 + StatColumn. Top-center button stack:
  `Choose Tech` / `Change Team` / `Ready`. Top-right `AgentCard`#2
  (`AgentZero-1`). Mid-left `New Game-1` Panel. Center `Pregame` float. Footer
  status `gg ready when you are` (dim).
- **Golden notes:** current is single roster list — missing dual AgentCards.

### 3.9 Tech Select (`tech_select.png`)
- Persistent chrome + `New Game-1`/`Pregame` float. Top-center `Back To Teams`
  oval. Top-right `Tech slots left: 1` header + tech-list Panel: `Name [$cost]`
  `ListRow`s (Laser/Rocket/… `[$3]`), affordable=bright, exhausted=dim, +
  ScrollBar.
- **Golden notes:** current has NO tech screen at all — build it.

### 3.10 Select Agent (`cc_select_agent.png`)
- Two-panel split filling frame, ~12px inset, black gutter. LEFT (~48%):
  `TitleBar` pill `SELECT AGENT` + roster `OvalButton` list +
  `Create New Character` row. RIGHT (~48%): empty `Panel` with `PlanetBackdrop`
  (Mars bottom-right). Center seam: `ScrollBar` assembly.
- **Golden notes:** current is single slate Hero panel with "Step 1 of 3"
  subtitle, no right Mars panel, no scrollbar, invented Back button — all wrong.

### 3.11 Silencer Alias (`cc_alias.png`)
- Same background as SELECT AGENT (both panels + Mars + scrollbar visible) +
  centered floating `Panel` (DoubleBorder), wider than tall. `TitleBar` pill
  `SILENCER ALIAS` + single `OvalButton`-as-field (left-aligned `AgentZero` +
  caret). No visible buttons (Enter confirms).
- **Golden notes:** current is standalone slate overlay, "New Agent" title,
  "Step 2 of 3" subtitle, bare Input, Continue/Cancel row — all wrong.

### 3.12 Select Agency (`cc_select_agency.png`)
- Two-panel split filling frame, ~12px inset. LEFT (~48%): `TitleBar`
  `SELECT AGENCY` + 5 agency `OvalButton`s (Noxis/Lazarus/Caliber/Static/Black
  Rose), generous gaps, selected=filled+bright + inner `ScrollBar`. RIGHT
  (~48%): detail surface (Mars behind text) — `SectionHeading` `Advantages` +
  `StatRow`s (`Endurance [+3]`, `Jump [+5]`) + `SectionHeading` `Description` +
  wrapping lore prose. No buttons on right.
- **Golden notes:** current uses slate Chrome+Sunken, "Choose Agency"/"Step 3
  of 3", magic-number columns (gap 78, w 236/196), wrong data model
  (tagline/security/4-line stat grid + agency emblem). REPLACE data model with
  `{advantages:[{name,delta}], description:prose}`; flex-fill, no emblem, no
  buttons, add scrollbar.

### 3.13 Lobby Connect (`lobby_connect.png`)
- Single ~280×280 `DialogPanel` (P22, whole-sprite, baked wells), centered
  horizontally, slightly below center, on PURE BLACK (no starfield/Mars/slate).
  Top ~60% = connection-log clip region (blank until connecting; fills with
  green status lines). Bottom ~40%: `Username` label + field (`AgentZero`),
  `Password` label + field (`*******` masked), `RectChromeButton` row
  `Login/Create` + `Cancel`.
- **Golden notes:** current is largely CORRECT (black field, centered sprite,
  baked-well rows, log clip, two chrome buttons). Verify field rect lands on
  baked wells (position, not flex-center), Chrome variant resolves to RECT (not
  oval) sprite, labels use heading/strong role brighter than dim log text.

### 3.14 In-game HUD (`ingame_hud.png` bare; chrome in `scoreboard.png`)
- Overlay over live world; transparent background. Two `HudBezel` sprites
  edge-anchored top (~40px, team-tint red-left/blue-right) and bottom (~95px).
- TOP-LEFT: `EmblemSplat` (health/objective) + `EmblemBadge` (team, chevron +
  "1") + `StatusDots` (3 blue). TOP-RIGHT: `StatusLamp` (round green ring).
- BOTTOM bezel wells: LEFT weapon/ammo (`WeaponSlotCell` column w/ `99`/`5`,
  weapon wireframe + mini-label, `Blaster` heading, `AmmoLcdBox` `99` + red
  `100` lozenge); CENTER `RadarViewport` (minimap + blips, `FATIGUE` red /
  `LOW FUEL` blue `StatusPill`s on top edge); RIGHT files/credits
  (`AgentSilhouette` blue + `40` lozenge + `FILES`/`CREDITS` blue value boxes).
- **Golden notes:** current is two flexed slate `Panel`s with crammed text
  strings — must be the sculpted bezel SPRITES with positional painted
  hotspots (green=own data, blue=economy, red=warnings). No centered slate
  match-bar in plain HUD (that's the F1 scoreboard).

### 3.15 Scoreboard (`scoreboard.png`) — F1 overlay
- Opaque-black `ScoreboardBar` across upper-middle (~+45/+45 to +595, ~70px),
  rounded top following the bezel curve. `EmblemSplat` row marker (left,
  outside bar). `ScoreboardRow`: agent name green (~x95) + stat columns
  `L:0  E:0  S:0  J:0  H:0  C:0` (fixed ~48px gutters). One row per agent.
- **Golden notes:** current is slate Overlay Panel + "Scoreboard" title + one
  multi-line text blob with `[T1]`/`(you)` decorations. Golden = black table
  bar, the L/E/S/J/H/C columns ARE the point, no title/decorations.

### 3.16 In-game chat (`ingame_chat.png`)
- `ChatBox` green wire-rect, bottom-right (~12px inset above bottom bezel),
  content-sized. Bottom-up log: `AgentZero: rushing objective!` history lines;
  active input line `[ALL]: clay chat smoke` + caret.
- **Golden notes:** current is full-width slate band + ScreenTitle + Input +
  Send/Channel/Close buttons. Golden = compact bottom-right green box, NO
  buttons (Enter sends, Esc cancels), scrollback history present, `[ALL]:`/
  `[TEAM]:` inline prefix not a separate heading.

### 3.17 Buy/Tech overlay (no golden — inferred)
- Green wire-`OverlayFrame` over world, title `BUY`/`TECH`, fixed window of
  `HudOvalRow`s (item + cost green, locked dimmed), green focus cursor, Esc
  closes. NOT slate Hero panel, NO Close button.

### 3.18 Mission summary (no golden — flagged)
- Out of cluster. Currently menu ScreenLayout + slate Hero + UpgradeRows. If
  the original follows menu language (green ovals, green text, sprite chrome),
  reconcile with menu cluster — needs-golden.

---

## 4. Known rendering BUGS + suspected root causes

Three confirmed root causes in the bake→IR→executor pipeline. The pipeline is
structurally sound — these are 3 surgical fixes, no pipeline deletion.

### Bug (a) — Rainbow-speckle Mars / starfield: WRONG PALETTE PAGE
- **Symptom:** the Mars planet/starfield bakes to multicolored noise.
- **Root cause:** `BakeChromeTextures` reads
  `game.renderer.palette.GetColors()` =
  `colors[currentpalette]` — and `currentpalette` is **0** in the menu
  (`SetPalette(0)` at init; `SetParallaxColors()` populating indices 226–255
  is never called for the menu). Bank 6 is authored against palette **page 1**,
  bank 7 against **page 2** (per-bank `paletteoffset`: banks 0–3→5–8, 6→1,
  7→2). origin/main does `ResetPresentation(1)` → `SetPalette(1)` before baking
  bank-6 art; the current code bakes against page 0, so the planet's high
  parallax/ramp indices resolve to stale garbage.
- **Where:** `clients/silencer/src/game/ui/game_ui_pipeline.cpp:185` (palette
  read), bake loop `:174-239`. Page source `palette.cpp:111-113`;
  per-bank pages `resources.cpp:135-155` (`:149-150`). origin/main authority
  `client/ui/screens/screen_context.cpp:50` +
  `.../main_menu/main_menu_screen.cpp:144` (`ResetPresentation(1)`).
- **Fix direction:** bake each bank against its authored page (bank 6→page1,
  bank 7/40→page2, banks 0–3→pages 5–8) — `SetPalette` per bake group or read
  `colors[page]` directly. Keep the base-not-display-cache decision.

### Bug (b) — Glitchy / double-struck button text: STRAIGHT-ALPHA TEXT IN A PREMULTIPLIED BUFFER
- **Symptom:** light fringe/halo + double-struck noise on button labels.
- **Root cause:** `TTF_RenderText_Blended` produces a **straight-alpha**
  texture left at SDL default `SDL_BLENDMODE_BLEND`, while sprite textures are
  `adopt()`-set to `PREMULTIPLIED`. Per-texture blend mode wins for
  `SDL_RenderTexture`, so text blends straight-over into the (0,0,0,0) surface.
  The whole buffer is then GPU-composited as premultiplied (src=ONE) —
  straight-alpha AA edges get over-bright RGB and overlapping coverage
  double-blends. `unpremultiply()` fixes the input color but the surface output
  is never re-premultiplied.
- **Where:** `clients/silencer/src/render/cppx_ui/font_registry.cpp:126-156`
  (raster + cache, blend at `:128`); `draw_executor.cpp:67-112` (render_text,
  unpremultiply `:55-65`, uncached `:101`); GPU composite
  `sdl3gpubackend.cpp:507-513`.
- **Fix direction:** premultiply each rasterized text texture (or set its blend
  mode so the blit writes premultiplied output) so the whole UI buffer is
  uniformly premultiplied before the GPU composite. Localized to
  `font_registry.cpp` cache + `draw_executor.cpp` uncached fallback.

### Bug (c) — Oval buttons solid-filled dark green: CONTROL-FILL RECT BEHIND THE SPRITE
- **Symptom:** transparent-interior oval shows a solid dark-green
  (`#1C1E24`-ish) center instead of the starfield/hollow outline.
- **Root cause:** paint order is `append_rect` BEFORE `append_image`.
  `tokens::image_patch` sets `background={0,0,0,0}` and clears
  gradient/border/radius but does NOT set `chromeless`. In `append_rect`,
  `control_box` is true (role==Button) and `has_color(background)` is false, so
  `fill = control_box && !chromeless ? control_fill() : transparent` returns
  `kButtonFill {24,28,36,255}` — a solid dark Rect pushed at the node box,
  BEHIND the oval Image. The oval interior is transparent (index 0), so the
  dark fill shows straight through. (The chrome button has the same latent
  issue but its nine-slice center is opaque, hiding it — only the oval
  regresses.) `append_frame` already guards on `image.texture_id != 0`;
  `append_rect` was never given the parallel guard.
- **Where:** `clients/silencer/src/client/ui/components/tokens.h:122-135`
  (`image_patch`); `draw_command_builder.cpp:247-273` (append_rect fill select,
  `kButtonFill` `:17,48-56`), order `:545-548`, append_image `:222-242`;
  `append_frame` guard pattern `:294,312`. `chromeless` slot
  `style_patch.h:36/63`.
- **Fix direction:** set `.chromeless(true)` in `tokens::image_patch` (one line,
  covers every sprite-backed control) OR guard `append_rect`'s control-fill on
  `v.image.texture_id == 0` mirroring `append_frame`.

### Note — toggle cells are safe
`boolean_setting_row.cppx:24-49` renders toggle cells via `image_patch` on a
**Box** role (not Button), so they do NOT hit bug (c). Only Button-role sprite
controls (oval/chrome) regress.

---

## 5. Recreate order + foundation work plan

**Phase 0 — Pipeline fixes (unblocks everything; do first):**
1. Bug (a) palette page — `game_ui_pipeline.cpp` BakeChromeTextures.
2. Bug (b) text premultiply — `font_registry.cpp` + `draw_executor.cpp`.
3. Bug (c) `.chromeless(true)` in `tokens::image_patch`.
   After this, baked sprites + transparent ovals + clean text all render
   correctly — verify against Mars/starfield + a single oval + a text label.

**Phase 1 — Tokens + core primitives:**
4. Replace slate/blue tokens with the green-phosphor family (§1.1); add HUD LCD
   palette. `tokens.h`.
5. `OvalButton` (P1) with transparent/filled/selected/as-field variants +
   focus-pins-bright state. `Starfield` (P3) confirm. `WordmarkLogo` (P2) keep.
6. `ToggleIndicator` round-disc (P7) at native ~36px; `SettingRow` (P8) drop
   the wrapping Hero panel.

**Phase 2 — Simple screens (validate primitives):**
7. Main menu (3.1) — staggered ovals, version footer.
8. Options (3.2), Audio (3.3), Display (3.4) — floating titles, SettingRows,
   Save/Cancel ActionRow, no panels.

**Phase 3 — Chrome + composite primitives:**
9. `ChromePanel` (P4) whole-sprite + `TitleTab` (P5) + `BindingRow` (P9) +
   `BindingWell` (P10) + `ScrollBar` (P11) → Configure Controls (3.5).
10. `Panel`/DoubleBorder (P14), `PlanetBackdrop` (P24), `DialogPanel` (P22),
    `RectChromeButton` (P21), `Field` (P19) → cc cluster (3.10–3.13). Replace
    `character_create_data.h` schema with `{advantages, description}`.

**Phase 4 — Lobby cluster:**
11. Persistent chrome: `TitleBar`, `AgentCard` (P16), `StatColumn` (P17),
    `EmblemStrip` (P20), `FooterStatus`. `ListRow` (P18) with selected/disabled
    + scrollbar.
12. Lobby (3.6), Create Game (3.7), Game Staging (3.8, dual AgentCards), Tech
    Select (3.9, new screen).

**Phase 5 — In-game cluster:**
13. `HudBezel` sprites + wells + WeaponSlotCell/AmmoLcdBox/StatusPill/
    RadarViewport/StatusLamp/EmblemBadge/EmblemSplat/StatusDots/AgentSilhouette
    → HUD (3.14).
14. `ScoreboardBar`+`ScoreboardRow` (3.15), `ChatBox` (3.16), buy/tech
    `OverlayFrame` (3.17).

**Phase 6 — Flagged / needs-golden:**
15. Mission summary (3.18) — obtain golden, reconcile with menu language.

Throughout: keep the working bake→upload→IR→executor seams
(`bake_indexed_rgba`, `TextureRegistry` premultiply+adopt, `ChromeTextures`/
`use_chrome` provider, `image_patch`/`image_patch_sub`, three-mode
`render_image`, index-0-transparent convention, one-time-bake guard). Verify
each phase with the pixdiff build against the goldens.

---

## 6. Sprite bank/idx reference table

From code analysis (`game_ui_pipeline.cpp`, `app_button_variant.h`,
`panel.cppx`, `screen_layout.cppx`, `main_menu.cppx`,
`boolean_setting_row.cppx`, `resources.cpp`).

| Sprite | Bank | Idx | Native | Draw mode | Caps / notes | Authored palette page |
|---|---|---|---|---|---|---|
| Starfield + Mars backdrop | 6 | 0 | full | whole, stretch full-bleed | bug (a) lives here | **1** |
| Oval button — Md | 6 | 7 | 196×33 | whole/stretch (→ should honor caps) | role=Button; bug (c) | 1 |
| Oval button — Sm | 6 | 28 | 112×33 | whole | role=Button | 1 |
| Oval button — Lg | 6 | 23 | 220×33 | whole | role=Button | 1 |
| Toggle cell OFF (L\|R) | 6 | 12 / 13 | small | whole, 2-cell | role=Box (safe) | 1 |
| Toggle cell ON (L\|R) | 6 | 14 / 15 | small | whole, 2-cell | role=Box; right half green | 1 |
| Chrome button — idle | 7 | 24 | — | nine-slice | caps `{l12,r12,t4,b4}` | **2** |
| Chrome button — focus | 7 | 28 | — | nine-slice | 2-state via separate frame + `lit` | 2 |
| Chrome panel (controls) | 7 | 5 | ~628×441 | **whole, native (nine-slice exception)** | PanelVariant::Chrome | 2 |
| Dialog message frame | 40 | 4 | — | whole | message modal | 2 |
| Dialog password/lobby-connect | 40 | 2 | — | whole | baked wells | 2 |
| SILENCER logo (final) | 208 | 60 | — | whole | reveal frame | — |
| SILENCER reveal frames | 208 | 58,56,54,52,50,48,46 | — | whole | animated via use_clock | — |
| Agency emblems ×5 | 181 | 0–4 | — | whole | cc detail (NOT in agency golden) | — |

**Per-bank palette page (`resources.cpp:135-155`):** banks 0–3 → pages 5–8;
bank 6 → page 1; bank 7 → page 2 (and bank 40 dialog also page 2). The bake
must select the page per bank, not `currentpalette` (the bug-(a) fix).

**Bake/emit seam (keep):**
`GameUiPipeline::RenderCppxClientUiFrame` → `BakeChromeTextures`
(`game_ui_pipeline.cpp:174-239`, palette read `:185`) →
`PipelineHost::bake_chrome_sprite` (`pipeline_host.cpp:70`) →
`bake_indexed_rgba` (`sprite_bake.cpp:5`, idx0→transparent) →
`TextureRegistry::upload_rgba` (`texture_registry.cpp:18`, premultiply + NEAREST
+ adopt PREMULTIPLIED). Read: `use_chrome()` → `tokens::image_patch`
(`tokens.h:122`) → `append_image` (`draw_command_builder.cpp:222`) →
`render_image` (`draw_executor.cpp:125-225`: nine-slice / rounded / plain).
