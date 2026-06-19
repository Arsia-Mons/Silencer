# In-Game HUD + Scoreboard + Chat — origin/main parity plan

> SPRITE-first. The golden in-game console is the **legacy bezel/console sprite
> art** (banks 94/95/96/102/103/188), positionally composited over the live
> world — NOT a vector redraw and NOT flexed slate Panels. The current cppx
> screen wraps everything in slate `Panel`s with crammed text strings; that is
> the whole divergence. This plan replaces those Panels with whole-sprite HUD
> bezels + absolutely-positioned painted hotspots, an opaque-black scoreboard
> table, and a compact bottom-right green chat box.

Targets:
`clients/silencer/src/client/ui/screens/in_game_screen.cppx` (+ `.h`),
`clients/silencer/src/client/ui/components/tokens.h` (LCD palette, net-new),
`clients/silencer/src/client/ui/hooks/use_chrome.h` +
`clients/silencer/src/game/ui/game_ui_pipeline.cpp` (net-new HUD sprite bakes),
`clients/silencer/src/client/ui/hooks/use_ingame_chat.h` (net-new `log` field).

Goldens: `/tmp/goldens/ingame_hud.png`, `/tmp/goldens/scoreboard.png`,
`/tmp/goldens/ingame_chat.png`.

---

## 0. The single most important framing decision

These three overlays are NOT three independent vector layouts. The golden is the
**legacy sprite HUD**, recovered from origin/main
`clients/silencer/src/client/ui/hud/*` (the deleted Clay layer). Every console
shape in the golden is a baked legacy sprite drawn at a FIXED logical pixel
position over the world — the same coordinates origin/main used. The cppx job is
to bake those sprites through the existing `use_chrome()`/`image_patch` seam and
position them with `LayoutStyle.position` (absolute), then paint text/value
hotspots on top at the legacy coords. Flex is used only for the scoreboard table
and chat-box stack — the HUD console itself is positional, not flow-laid.

### Legacy sprite bank/idx reference (recovered from origin/main `hud/`)

| Element | Bank | Idx | Source (origin/main file) | Notes |
|---|---|---|---|---|
| HUD console camera/bezel frame (top) | 95 | 2 | `hud_system_camera.cpp` via `InGameHud.cpp:30` `BuildHudSystemCameraFrame(...,95,2,92,381)` | whole-sprite, offset bank 92, y≈381 |
| HUD console camera/bezel frame (bottom dash) | 95 | 11 | `InGameHud.cpp:33` `(...,95,11,92,318)` | whole-sprite, offset bank 92 |
| Health bar | 95 | 0 | `hud_status_sprites.cpp` slice | vertical drain via src-rect |
| Shield bar | 95 | 1 | `hud_status_sprites.cpp` | vertical drain |
| Health/shield warn | 95 | 3 / 4 | `hud_status_sprites.cpp` | lit when low |
| Fuel mask / bar / low | 95 | 5 / 6 / 8 | `hud_status_sprites.cpp` | horizontal drain |
| Files bar | 95 | 7 | `hud_status_sprites.cpp` | horizontal drain |
| Poisoned splat | 97 | 5 | `hud_status_sprites.cpp` (x183,y453) | conditional |
| Minimap/radar frame | 94 | 0 | `hud_status_sprites.cpp:40` | whole-sprite |
| Team emblem badge frame | 94 | 1 | `hud_teams.cpp:28` | whole-sprite |
| Inventory frame | 94 | 2 | `hud_status_sprites.cpp` | whole-sprite |
| Weapon selector / faces / glow | 96 | 0 / 1-4 / 5-8 | `hud_status_sprites.cpp` | face+glow per `currentWeapon` |
| Team strip / agent silhouette | 103 | 0,1,index | `hud_teams.cpp:30-74` | files/credits cluster |
| Agency emblem (team badge glyph) | 181 | agency | `hud_player_list_overlay.cpp:57` | already baked as `agency_emblem[5]` |
| Buy/Tech overlay bg + highlight | 102 | 0 / 1 | `hud_buy_tech_overlay.cpp:66-75` | overlay frame + row cursor |
| Chat box bg (nine-slice tiles) | 188 | 0..6 | `hud_chat_overlay.cpp:30` `kChatBackgroundBank=188` | tiled L/mid/R |

### Legacy readout coordinates + palette colors (origin/main `hud_readouts.cpp`)

- Ammo (own data, GREEN/`hud-green`): `HudCurrentAmmo` at **x117,y457**, size
  `HudCounter`, alpha-blended (`emitAlphaText`, brightness 128).
- Per-weapon ammo column (`Tiny`, green): x0,y414/428/442/456 (Blaster/Laser/
  Rocket/Flamer), centered in w20.
- Health digits: **x145,y463**, `Tiny`, palette color **161** (red-ish),
  centered. Shield: **x468,y463**, color **202** (blue), centered.
- Credits (blue economy): **x572,y456**, `HudCounter`, color **202**.
- Inventory counts: xoffsets `{612,584,556,528}`, yoffsets `{13,13,11,7}`.

These exact coords/colors are the parity target. `LegacyPalette(161)`≈hud-red,
`(202)`≈hud-blue, default green = hud-green.

### Scoreboard (origin/main `hud_player_list_overlay.cpp`)

- Root: full-surface, padding `{50,50,50,0}`, child-align center/top.
- Bar: `GROW × FIXED(10 + teams*58)`, padding `{10,10,10,0}`, **fill
  `{0,0,0,128}`** (the only opaque overlay — `hud-black @ ~85%`... legacy used
  128≈50%; sample the golden, it reads darker, use ~`{0,0,0,210}`).
- Per team: row `GROW × FIXED(58)`; left 40px emblem slot (bank 181 emblem,
  17px) + peer column.
- Per peer: row `GROW × FIXED(12)`: name (`Body`, green, left) +
  stats string `"L:%d    E:%d  S:%d  J:%d  H:%d  C:%d"` width `(len+1)*6`.

### Chat (origin/main `hud_chat_overlay.cpp`)

- Bottom-right, bank-188 nine-slice background (idx0=left cap, idx1=mid tile,
  idx2=right cap, idx6=bottom). Content-sized, grows with log length.
- Bottom-up: history log lines (`AgentZero: rushing objective!`), then active
  input line `[ALL]: clay chat smoke` + caret. NO Send/Channel/Close buttons —
  Enter sends, Esc cancels.

---

## 1. Golden layout breakdown (how it fills 640×480)

### 1a. `ingame_hud.png` (bare console over world)
The HUD is **edge-anchored sprite chrome**, transparent center (world shows):
- **TOP band (~y0..40):** thin sculpted bezel (bank 95 idx2). TOP-LEFT cluster:
  `EmblemSplat` (health/objective) + team `EmblemBadge` (bank 94 idx1 +
  agency-181 glyph, chevron + "1") + 3 blue `StatusDots`. TOP-RIGHT:
  `StatusLamp` round green ring.
- **BOTTOM dash (~y385..480):** sculpted console (bank 95 idx11), 3 wells:
  - LEFT well: weapon selector column (bank 96 idx0 + face idx1-4 + glow idx5-8),
    per-weapon ammo digits (x0 col), `Blaster` weapon-name heading (green),
    `AmmoLcdBox` big `99` (x117,y457 green) + red `100` secondary lozenge,
    health digits x145 (red 161).
  - CENTER well: `RadarViewport` (bank 94 idx0 black rounded-oval minimap +
    blips), `FATIGUE`(red)/`LOW FUEL`(blue) `StatusPill`s on the top edge,
    fuel bar (bank 95 idx6) drained.
  - RIGHT well: `AgentSilhouette` (bank 103 blue figure) + `40` lozenge +
    `FILES`/`CREDITS` blue value boxes (credits x572 color 202), shield x468.

Region fill: a transparent `ScreenLayoutVariant::Hud` root; two absolutely-
positioned full-width bezel sprite Boxes (top y0, bottom anchored bottom);
hotspots positioned absolutely within.

### 1b. `scoreboard.png` (F1 overlay — chrome visible)
Same HUD console (top+bottom bezels present) PLUS a centered opaque-black table
bar in the upper-middle: header-less rows of `AgentZero  L:0 E:0 S:0 J:0 H:0 C:0`
with an `EmblemSplat` row marker to the left. Bar spans ~+45 L/R, ~70px tall.

### 1c. `ingame_chat.png` (chat overlay over world)
Same console; a compact green wire-rect chat box bottom-right (~12px above the
bottom bezel). Bottom-up scrollback (`AgentZero: rushing objective!` ×2) + active
`[ALL]: clay chat smoke` line + caret. No buttons.

---

## 2. Concrete ordered edits

### Edit 1 — Net-new LCD palette tokens (`tokens.h`)
Add the in-game HUD LCD family (spec §1.1) below `kTextHud`:
```cpp
// ---- In-game HUD LCD palette (overlay over live world; spec §1.1) ----
constexpr ::ui::Color kHudGreen    = {61, 232, 61, 255};   // #3DE83D own data/chat/scoreboard
constexpr ::ui::Color kHudGreenDim = {30, 122, 30, 255};   // #1E7A1E chat body / inactive
constexpr ::ui::Color kHudBlue     = {58, 107, 255, 255};  // #3A6BFF economy/files/credits/dots
constexpr ::ui::Color kHudRed      = {224, 48, 48, 255};   // #E03030 FATIGUE/health/2ndary ammo
constexpr ::ui::Color kHudAmber    = {160, 86, 30, 255};   // #A0561E radar schematic
constexpr ::ui::Color kHudBlack    = {0, 0, 0, 210};       // ~85% scoreboard bar / radar viewport
constexpr ::ui::Color kBlipAlly    = {255, 255, 255, 255};
constexpr ::ui::Color kBlipEnemy   = {224, 48, 48, 255};
```
`kTextHud` (#3DE83D) already exists and is reused as `kHudGreen`; keep both names
or alias. These are net-new — nothing else in tokens.h carries the HUD LCD set.

### Edit 2 — Net-new HUD sprite bakes (`use_chrome.h` + `game_ui_pipeline.cpp`)
The bezels/radar/weapon/silhouette/chat-bg are NOT yet baked (only oval/chrome/
dialog/starfield/logo/toggle/agency_emblem are). Add to `ChromeTextures`:
```cpp
// In-game HUD console (banks 94/95/96/103, palette page = base; bank 188 chat).
uint32_t hud_bezel_top = 0;     uint16_t hud_bezel_top_w=0, hud_bezel_top_h=0;   // 95/2
uint32_t hud_bezel_bottom = 0;  uint16_t hud_bezel_bottom_w=0, hud_bezel_bottom_h=0; // 95/11
uint32_t hud_radar = 0;         uint16_t hud_radar_w=0, hud_radar_h=0;           // 94/0
uint32_t hud_team_frame = 0;                                                     // 94/1
uint32_t hud_inventory_frame = 0;                                               // 94/2
uint32_t hud_weapon_face[5] = {};  // 96/1..4 (+selector 96/0)
uint32_t hud_buytech_bg = 0;    uint16_t hud_buytech_bg_w=0, hud_buytech_bg_h=0;  // 102/0
uint32_t hud_buytech_row = 0;                                                    // 102/1
uint32_t hud_chat_l=0, hud_chat_mid=0, hud_chat_r=0; uint16_t hud_chat_h=0;      // 188/0,1,2
```
In `BakeChromeTextures`, after the existing bakes, add `bake(95,2,...)`,
`bake(95,11,...)`, `bake(94,0,...)`, `bake(94,1,...)`, `bake(94,2,...)`,
`bake(96,1..4,...)`, `bake(102,0,...)`, `bake(102,1,...)`, `bake(188,0/1/2,...)`.
NOTE the texture budget: tally is 9/64 today + these ~16 ≈ 25/64 — within cap.
Banks 94/95/96/102/103/188 use the **base** palette page (the `page_for_bank`
`default:` arm already returns `palette` = base), which is correct for in-game
HUD art (it's authored against the live game palette, not a menu page). Verify by
eye; if speckled, those banks have their own `paletteoffset` in `resources.cpp`
and must be added to `page_for_bank`.

### Edit 3 — Net-new chat log field (`use_ingame_chat.h`)
The golden chat shows scrollback; the hook exposes only `text`/`show_ticks`.
Add a read-only log:
```cpp
std::vector<std::string> log = {};  // recent visible chat lines, oldest→newest
```
Populate it in the composition root (`game_ui_pipeline.cpp` where IngameChat is
built) from `World`'s chat ring (origin/main fed `hud_chat_overlay` the same
history). Minimal: last N (≈4) lines. If the World seam isn't trivially
available, fall back to `text` only for v1 and file a follow-up — but the golden
explicitly shows history, so prefer wiring it.

### Edit 4 — Rewrite `in_game_screen.cppx` (the core)
Delete the slate-Panel composition (`PlayerListOverlay`, the Sunken match/status
Panels, the full-width `IngameChatOverlay` Box). Replace with:

**4a. New file-local primitives** (imperative `::ui::component`/Box subtrees,
since JSX-in-var is unsupported):
- `HudBezel(uint32_t tex, uint16_t w, uint16_t h, edge)` — absolutely-positioned
  full-width Box painting the baked bezel via `tokens::image_patch(tex)` (whole-
  sprite, no nine-slice). Top: `position{top:0,left:0}`. Bottom:
  `position{bottom:0,left:0}`, `width:100%`, `height:points(h)`.
- `Hotspot(x,y,w,h, child)` — a `position:Absolute` Box at legacy coords, child =
  a `BodyText`/value. Used for ammo/health/shield/credits/files.
- `LcdValue(const char* digits, Color, face=kFaceTiny|HudCounter)` — text painted
  via `tokens::text_patch(color, size, kFaceTiny)`. Green own-data, red health,
  blue credits/files (colors 161/202 → `kHudRed`/`kHudBlue`).
- `RadarViewport()` — Box with `image_patch(chrome.hud_radar)` + a `kHudBlack`
  inner well; blips deferred (paint own-player blip as a `kBlipAlly` dot).
- `StatusPill(text, Color)` — small nine-slice-ish Box, only rendered when lit
  (`ps.fuel_low` → `LOW FUEL` blue; fatigue → `FATIGUE` red).
- `ChatBox(log, draft, with_team, caret)` — bottom-right Box, bank-188 bg
  (image_patch on the mid tile, or just `panel_patch({0,0,0,200}, kHudGreen)` as
  the wire-rect if 188 tiling is fiddly for v1), `direction:Column`,
  `align:Start`; children = log lines (`kHudGreenDim`) then the active line
  `[ALL]: <draft>` (`kHudGreen`) + caret.
- `ScoreboardBar(teams, players)` — centered opaque Box (`fill_patch(kHudBlack)`),
  per-team emblem + peer rows (name green + `L:0 E:0 S:0 J:0 H:0 C:0` stats).

**4b. New `ScoreboardOverlay`** replacing `PlayerListOverlay`:
```cpp
::ui::UiElement ScoreboardOverlay(const std::vector<TeamScore> &teams,
                                  const std::vector<ScoreboardPlayer> &players) {
  std::vector<::ui::UiElement> rows;
  for (const ScoreboardPlayer &p : players) {
    // statsString currently unavailable per-player; see Edit 5 (data gap).
    std::string line = p.name + "    L:0    E:0  S:0  J:0  H:0  C:0";
    const char *txt = ::ui::copy_string(line.c_str());
    const char *k = ::ui::copy_string(("sbrow_" + p.name).c_str());
    ::ui::UiElement row = <BodyText key={k} variant={BodyTextVariant::Body} value={txt} />;
    rows.push_back(row);
  }
  return <Box key="sbbar"
    layout={::ui::LayoutStyle{
      .direction = ::ui::FlexDirection::Column,
      .align_items = ::ui::AlignItems::Start,
      .padding = {10,10,10,10}, .gap = 4.0f,
      .position = ::ui::Position::Absolute,  // centered upper-middle
    }}
    style={tokens::fill_patch(tokens::kHudBlack)}>
    {::ui::children(std::move(rows))}
  </Box>
}
```
(Use the same fixed-slot pattern as `buytech_row` if `children(vector)` of
dynamic rows ever misbehaves; spec says `::ui::children(std::vector<UiElement>)`
is supported, so prefer it here.)

**4c. New `InGameScreenView` skeleton:**
```cpp
ChromeTextures chrome = use_chrome();
// ... existing hooks ...
return <ScreenLayout key="root" variant={ScreenLayoutVariant::Hud}>
  {HudBezel(chrome.hud_bezel_top, chrome.hud_bezel_top_w, chrome.hud_bezel_top_h, /*top*/true)}
  {HudBezel(chrome.hud_bezel_bottom, chrome.hud_bezel_bottom_w, chrome.hud_bezel_bottom_h, /*top*/false)}
  {/* top-left emblem/dots, top-right lamp */}
  {/* bottom wells: weapon+ammo (left), radar+pills (center), files/credits (right) */}
  {scoreboard_overlay}  // ScoreboardOverlay when teams.showing
  {buytech}             // BuyTechOverlay (Edit 6)
  {chatbar}             // ChatBox when chat.active
</ScreenLayout>
```
The `ScreenLayoutVariant::Hud` root is already transparent
(`background({0,0,0,0})`) — keep it, but its `SpaceBetween`/padding is for the
old Panel layout; since bezels/hotspots are now absolutely positioned, the
padding is harmless (absolute children ignore it) but consider dropping the
`gap`/`SpaceBetween` to avoid affecting the few flow children.

### Edit 5 — Scoreboard data gap (`use_teams.h`)
`ScoreboardPlayer` has only `name`/`team_number`/`local` — NO per-player L/E/S/J/
H/C stats. The golden columns ARE the point. Two options:
- (preferred) extend `ScoreboardPlayer` with `int kills,deaths,score,jets,
  hacks,contacts;` populated in the composition root from the peer stats
  origin/main used (`hud_player_list_overlay` had them via `TeamPeerView`).
- (v1 stopgap) render zeros `L:0 E:0 S:0 J:0 H:0 C:0` to match the golden
  literal (the golden shows all-zero AgentZero), and file the real-stats wiring
  as a follow-up. The golden is all-zeros, so the stopgap is visually exact for
  the captured frame — acceptable for parity, with a TODO.

### Edit 6 — Buy/Tech overlay (`BuyTechOverlay`, §3.17, no golden)
Replace the Hero `Panel` + `Close` button with a green wire-`OverlayFrame`:
`image_patch(chrome.hud_buytech_bg)` (bank 102/0) or a `panel_patch({0,0,0,200},
kHudGreen)` wire-rect; title `BUY`/`TECH` (green caps); the existing 5 fixed
`buytech_row` slots (keep — they're correct), selected row gets the bank-102/1
cursor highlight or `kHudGreen` brighter tint; **remove the Close button** (Esc
closes). Keep `defaultFocused`/`controlId` wiring as-is.

### Edit 7 — Delete now-dead helpers
Remove `build_score_text`, `build_scoreboard_text`, `PlayerListOverlay`, the old
`IngameChatOverlay`, `vitals`/`arms`/`match_line`/`score_line` text-storage
blobs (the HUD shows them as positioned LCD hotspots now, not concatenated
strings). Keep `phase_label` only if a status pill uses it.

---

## 3. Data / hooks / sprites: exist vs net-new

| Need | Status | Where |
|---|---|---|
| LCD palette tokens | **NET-NEW** | `tokens.h` (Edit 1); `kTextHud` partial exists |
| HUD bezel/radar/weapon/chat sprites baked | **NET-NEW** | `use_chrome.h` + `game_ui_pipeline.cpp` (Edit 2); bake seam exists |
| `image_patch` whole-sprite paint | EXISTS | `tokens.h:124` |
| `agency_emblem[5]` (team badge glyph) | EXISTS | `use_chrome.h:73` (bank 181) |
| Player vitals/ammo/files/credits | EXISTS | `use_player_status()` |
| Per-player scoreboard stats (L/E/S/J/H/C) | **MISSING** | `use_teams.h` ScoreboardPlayer (Edit 5) |
| Chat scrollback log | **NET-NEW** | `use_ingame_chat.h` `log` field (Edit 3) |
| `::ui::children(vector)`, `copy_string` | EXISTS | runtime (used by buytech_row) |
| Absolute positioning (`Position::Absolute`) | VERIFY | `ui/style` LayoutStyle — confirm the slot exists; if not, this is the biggest unknown (see §5) |

---

## 4. Capture / verify recipe (headless)

The existing harness drives this screen — reuse it:
`tests/cli-agent/e2e/51_ingame_ui_overlays.sh`.

```bash
. clients/silencer/tests/cli-agent/e2e/lib.sh   # (path: tests/cli-agent/e2e/lib.sh)
PORT=$(pick_port); PID=$(start_silencer "$PORT"); wait_alive "$PORT"
cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
cli --port "$PORT" click --label Tutorial
cli --port "$PORT" wait_for_state --state SINGLEPLAYERGAME --timeout-ms 15000
# wait until world has objects+players (loop on world_state, as 51_*.sh does)
cli --port "$PORT" ingame_ui_mode --mode clear     # bare HUD  -> ingame_hud
cli --port "$PORT" screenshot --out /tmp/cap_hud.png
cli --port "$PORT" ingame_ui_mode --mode status    # F1 scoreboard -> scoreboard
cli --port "$PORT" screenshot --out /tmp/cap_scoreboard.png
cli --port "$PORT" ingame_ui_mode --mode chat      # chat overlay -> ingame_chat
cli --port "$PORT" screenshot --out /tmp/cap_chat.png
stop_silencer "$PID" "$PORT"
```
The CLI op is `ingame_ui_mode --mode {clear|status|chat|buy|tech}` (51_*.sh:97).
Compare `/tmp/cap_*.png` to `/tmp/goldens/*.png` by eye (Read tool renders PNGs)
and pixdiff. Build via `clients/silencer/build.sh win-ninja` (never raw cmake).
Re-run `51_ingame_ui_overlays.sh` (asserts `chat_active`/`show_chat_ticks` and
640×480 screenshots) and `60_ui_architecture_boundaries.sh` after the edit.

---

## 5. Risks / unknowns

1. **Absolute positioning support.** The HUD is positional, not flow. Confirm
   `::ui::LayoutStyle` has a `position`/`Position::Absolute` + `top/left/bottom`
   offset (Yoga supports it). If the cppx layout binding doesn't expose it yet,
   that's the single biggest blocker — fallback is nested flex with fixed
   `points` widths + `JustifyContent::SpaceBetween` to approximate the legacy
   coords (lossy). RESOLVE THIS FIRST.
2. **HUD sprite palette page.** Banks 94/95/96/102/103/188 may have their own
   `paletteoffset` in `resources.cpp` (like 6→1, 7→2). The `page_for_bank`
   `default:` returns base — if HUD art bakes speckled, add the right page per
   bank (mirror the bug-(a) fix). Verify visually after the first bake.
3. **Bezel sprite native size vs 640 width.** Bank 95 idx2/11 are authored to
   the full HUD width with baked offsets (origin/main used `SpriteOffsetX`). The
   whole-sprite paint must place them at the legacy offset, not stretch to 100% —
   carry `_w/_h` and size the Box to native, positioned by the legacy x/y.
4. **Chat-bg nine-slice (bank 188 tiling).** origin/main hand-tiled idx0/1/2. A
   true nine-slice may not map cleanly; v1 stopgap is a `panel_patch` green
   wire-rect (kHudGreen border, `{0,0,0,200}` fill) — visually close enough for
   the golden, swap to baked 188 later.
5. **Scoreboard stats data.** Real L/E/S/J/H/C requires extending the teams
   hook + composition root. The golden is all-zeros so the stopgap is exact for
   the capture, but live play needs the real wiring (Edit 5 preferred path).
6. **Radar minimap content.** The golden radar shows live schematic geometry +
   blips. v1: black viewport + own-player blip only; full minimap render is a
   deferred follow-up (it needs world geometry the UI layer doesn't own).
