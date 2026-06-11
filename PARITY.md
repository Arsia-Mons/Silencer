# PARITY.md — origin/main parity ledger

States: `PASS` / `DIVERGED` / `UNVERIFIED`. Evidence = tool run from the session that set the state.
Gate: `pixdiff_tolerant.py` printed PASS (global <1%, zero tiles >5%) + `visual_parity_gate.js` overall=PASS.
Goldens: `tests/cli-agent/e2e/golden/` (origin/main @ af4c50c5, 1920×1080). Renders: `/tmp/cppx_renders/`.

## Visual — menu/lobby targets (goldens exist)

All measured 2026-06-11 (iteration 0) with recalibrated tile gate, fresh captures, build @ d3d3f9d4+dirty.

| Surface | State | Evidence (global% / hot tiles / worst tile) | Notes |
|---|---|---|---|
| mainmenu | **PASS** | pixdiff PASS 0.12%/0 hot (3.1 max) 2026-06-11 (was 0.83/18/10.0) | OVAL+LOGO PER-PHASE VARIANTS landed (see systemic entry below); stack inset 48.75% (=x624) lands pills on the virtual grid (vx 416/456/496); logo frames registered too (dark-brick interior striping was hot) |
| options | DIVERGED (1 tile) | 0.0735% / 1 / 5.4% @936,546 (was PASS 0.2578/0/4.2) | ovals + 3 of 4 labels now byte-exact (corr 1.0000); the one hot tile is the "Audio" label — GLYPH-PHASE family (golden text sits at a different device phase than the single-phase atlas; corr maxes 0.94 at any shift). Global 3.5x better; tile gate flipped red on this one tile |
| options_audio | **PASS** | pixdiff PASS 0.0658%/0 hot (3.2 max) 2026-06-11 (was 0.61/21/12.0) | ovals+toggles per-phase exact; row wrappers margin-right 1/2 land cells on the grid; title nudge ml=1 (+1 device) |
| options_display | DIVERGED | 0.1636% / 1 / 6.3% @702,468 (was 1.48/40/23.1) | action-row y fixed (392 -> vy 261, was ~4px low); single hot tile = "Smooth Scaling" label, glyph-phase + irreducible ±1 (same-frac labels want opposite shifts); <10% target met |
| options_controls | DIVERGED | 0.3856% / 5 / 5.8% @*,0 (was 1.29/30/10.1) | bind/preset/save ovals per-phase exact (lanes x561/x833, save 335/608); residual = the 5 title-row tiles ("Configure Controls", dx=0, glyph-phase) at 5.1-5.8% |
| lobby_connect | DIVERGED | 1.46% / 41 / 25.9% @780,780 | CORRECTED DIAGNOSIS: button row IS centered right (origin's floating row escapes pad-84 and centers across panel); real diff = button chrome detail (inner inset frame) + widths (L/C 275 vs 250 device px) |
| character_create | DIVERGED | 1.59% / 41 / 17.9% @468,234 (was 2.17/58) | portrait row + panel chrome |
| cc_alias | DIVERGED | 2.20% / 56 / 21.2% @624,468 (was 2.77/73) | alias dialog region |
| cc_select_agency | DIVERGED | 7.09% / 125 / 40.7% @1092,546 | agency Description paragraph: glyph metrics/wrap slightly off → dense text amplifies |
| lobby_screen | DIVERGED | 3.68% / 94 / 31.9% @234,312 (was 4.00/114) | VERIFIED BY EYE: agent-card emblem scale/pos, WINS/LOSSES/XP row spacing, Agents button high + wrong chrome. ORIGIN SPEC (character_panel.cpp, virtual px → ×1.5 logical): content pad 6, emblem↔info gap 10; left rail emblemBoxW=clamp(inner*18%,40,64) square Contain emblem + LevelBadge(bodyLineH, centered); info col gap 5: name(heading lineH) → details row: stats col gap 10 [stat table gap 2: WINS,LOSSES rows; label col = w("LOSSES")+4] → XP line → actions row h21 (Chrome "Agents" minW 92 padX 12). The LOSSES↔XP gap = 10 virtual (15 logical) — that's the visible golden gap before XP |
| create_game | DIVERGED | 5.01% / 113 / 31.9% @234,312 | map-list pitch fixed (21 logical = origin kMapListLineH 14 virtual; was 16.5+1) — right-panel hot column (38.4 @1248,*) cleared; worst tile is now the shared agent-card region (see lobby_screen spec) |
| game_staging | DIVERGED | 4.20% / 93 / 32.9% @1248,156 | shares lobby panels + right panel |
| tech_select | DIVERGED | 5.38% / 106 / 34.3% @1326,312 | tech grid region divergent |

Menu-cluster rows re-measured 2026-06-11 after the per-phase oval/logo variants landed;
lobby/cc rows re-verified same day (all equal or marginally better, no regressions).

ELEMENT TWO-HOP DESIGN — IMPLEMENTED 2026-06-11 (sprite_bake.cpp bake_element_rgba +
PipelineHost::bake_element_sprite; first consumer chrome_controls in BakeChromeTextures):
origin stretches the sprite into its virtual element box (bx,by,bw,bh virtual) then the
global magnify maps device px src=int(gx/s) — an element's internal pixel phase depends on
its virtual position, so the bake evaluates the full chain at ABSOLUTE device pixels and
emits a texture covering the element's device footprint (index 0 → transparent, composites
over the separately-baked two-hop backdrop). Draw contract: the texture rect snaps OUTWARD
to logical points that land on integer device px (Yoga rounds layout to whole logical px —
at scale 1.5 only EVEN logical coords are device-integral; uncovered fringe bakes
transparent) and the consumer absolutely positions a box of exactly that logical rect
(Yoga abs inset = parent border edge, padding excluded — verified in yoga-src
AbsoluteLayout.cpp). Validated: rail/left/bottom frame bands byte-identical vs golden.

### [systemic] Per-phase legacy-sprite variants — ovals/toggles/logo SOLVED 2026-06-11

The 1:1-virtual chrome sprites (green ovals bank6 idx7/28/23, toggle cells idx12-15,
logo bank208) get their device striping phase from origin's whole-frame magnify at
their ABSOLUTE position; the SW renderer FLOORS float dst rects (verified: ink starts
x=1027 for dst.x=1027.5) and center-samples, so no rect/src offset trick can reproduce
int(dx/s) — and mainmenu alone uses all four y-phases, so a canonical phase is
impossible. Landed: TextureRegistry::register_legacy_sprite + resolve_legacy_variant
(texture_registry.cpp) — the executor (draw_executor render_image, plain path) swaps a
qualifying draw (sprite at 1:1 virtual scale, no src/flip) for a lazily-baked
bake_element_rgba variant covering the sprite's exact device cell, drawn 1:1; memo key
(base_id, X%18, Y%18) — the pattern period divides 18 for s in {1,1.5,2.25,3,4.5}.
No IR change; ~22 variant textures used (64-cap: watch it). Authoring contract: the
box must sit ON the golden cell's virtual grid — nudges of <=1 logical px recorded per
screen (mainmenu 48.75% inset; options margin-right 2; audio/display row wrappers
mr 1/2, display row2 mt -0.5, actionpad mt 3.5; controls content mr 2, kOrCol 68,
preset pad-b 2, actiongap 48.5, actions ml 1). vx = round(floor(dev_x)/2.25) must
recover the golden cell (measure golden bboxes first).

### [systemic] NEXT FAMILY: glyph-atlas phase (text striping)

The exact-color glyph atlas (origin's rendered text pixels) is captured at ONE device
phase; origin re-stripes text per absolute position like every sprite. Same-screen
proof (options): "Controls" label corr 1.0000 (byte-exact) while "Audio" maxes 0.94 at
any integer shift — the golden's text phase differs and the atlas can't express it.
Binding residuals: options 5.4% tile (Audio lbl), controls title row 5.1-5.8% x5,
display Smooth lbl 6.3%. Fix shape: per-phase text raster (string texture baked
through the two-hop chain at its absolute device position — same mechanism as
resolve_legacy_variant, applied to the text path in glyph_fonts/draw_executor).
Label PLACEMENT is already floor-tuned: oval label padding {16.75,15.25,8.0,4.0}
(+0.375/+0.75 device bias moves only the .75-fraction labels across the floor;
remaining per-label ±1s are origin int-chain artifacts, irreducible by global bias).

### [systemic] Backdrop scanline-striping arithmetic — root of most menu hot tiles

ROOT-CAUSED + FIX IN FLIGHT 2026-06-11. Run-length analysis (golden cols AND rows = {2,2,5}
repeating = avg 3.0/src px two-hop; render = uniform 3) plus origin source
(game_ui_pipeline.cpp MenuUiScaleForSurface, clay_ui_compositor.cpp DrawImage + Render):
origin menus lay out at virtual int(W/s)×int(H/s), s=min(W/640,H/480) (853×480 @1080p),
cover-blit the backdrop sprite into that canvas (×1.333 NEAREST), then magnify the whole
frame by s (×2.25 NEAREST, centered). cppx's single-hop ×3 cover can never reproduce the
two-hop run pattern. Fix shipped to build: bake_backdrop_rgba (sprite_bake.cpp) replicates
both integer hops at device res; starfield (cover) + starfield_stretched (Options·Controls,
PackImageStretch(6,0)) + lobby_backdrop (stretch) all baked this way, drawn 1:1 full-bleed.
Earlier intermediate attempt (fit=Stretch single-hop) measured WORSE (mainmenu 2.32→3.28) —
geometry must stay cover; only the resample chain was wrong.

## Visual — no origin golden (functional checks only)

| Surface | State | Notes |
|---|---|---|
| gallery | n/a (visual) | cppx-only showcase; golden is a stale cppx render — do not gate |
| message_modal | n/a (visual) | no standalone origin trigger; golden stale — do not gate |
| password_modal | n/a (visual) | no standalone origin trigger; golden stale — do not gate |

## Visual — in-game (goldens MISSING — capture from origin first)

Enumerated from origin source 2026-06-11 (Explore agent over .worktrees/origin-capture).
Headless capture skips the HUD composite layer (ORIGIN_GOLDENS.md) — needs a capture-build
patch or windowed capture before goldens can exist.

| Surface | State | Origin anchor |
|---|---|---|
| hud_status_bar (minimap frame, fuel/health/shield/files gauges, poison, weapon glow+bracket, inventory) | UNVERIFIED | ui/hud/hud_status_sprites.cpp |
| hud_readouts (ammo counter, per-weapon ammo, credits, health/shield numerics) | UNVERIFIED | ui/hud/hud_readouts.cpp:14-88 |
| hud_team_strip (per-team peer sprites, in-base/secret pulses, secret slots, beaming) | UNVERIFIED | ui/hud/hud_teams.cpp |
| hud_secret_overlay (9-line hack progress, highlight pulses) | UNVERIFIED | ui/hud/hud_secret_overlays.cpp:73-110 |
| hud_trace_time ("Government Trace Time: NNN") | UNVERIFIED | ui/hud/hud_readouts.cpp:90-103 |
| hud_system_camera (inset frames ×2) | UNVERIFIED | ui/hud/hud_system_camera.cpp:13-40 |
| chat_overlay (history 5 lines, input + caret, ALL/TEAM toggle; T/Enter/Esc) | UNVERIFIED | ui/hud/hud_chat_overlay.cpp:142-236 |
| buy_tech_overlay (5-row scroll list, credits/viruses footer; Up/Down/Enter/Esc) | UNVERIFIED | ui/hud/hud_buy_tech_overlay.cpp:58-190 |
| player_list_overlay (F1 hold; per-team emblem + peer stats) | UNVERIFIED | ui/hud/hud_player_list_overlay.cpp:16-98 |
| ingame_messages (center reveal text, typed colors) | UNVERIFIED | ui/hud/InGameOverlays.cpp:58-145 |
| top_ticker (scrolling top message) | UNVERIFIED | ui/hud/InGameOverlays.cpp:185-209 |
| status_lines (bottom-center stack, fading) | UNVERIFIED | ui/hud/InGameOverlays.cpp:147-183 |
| quit_prompt ("Hit Enter To Quit"; Enter/Esc state machine) | UNVERIFIED | ui/hud/InGameOverlays.cpp:211-227 |

Hardcoded in-game bindings to verify functionally: T chat, F1 player list, F2 team colors,
F4 music toggle, F5 random music, Enter quit-confirm flow.

## Functional — e2e suite (run 2026-06-11, /tmp/e2e_run_iter0.log: 13 PASS / 10 FAIL)

| Scenario | State | Evidence |
|---|---|---|
| 00,10,11,12,13,14,15,16,18,19,20,22,51,60 | PASS | e2e run 2026-06-11 |
| 17,30,31,40,50,52 | PASS | repaired by workflow 2026-06-11 (origin-correct flows: Enter submits alias, no Continue btn) |
| 21_main_menu_layout | PASS | root cause was the content-scale ≥1 clamp (UI 1.5× oversized at 640×480); unclamped to floor 480/720 → origin-native proportions; test now bounds-checks in logical space, stagger band 130 (origin fan = 120 logical) |
| 53_lobby_create_options_scroll | PASS | same scale-clamp root cause; green after unclamp |
| 70_visual_regression | RED-BY-DESIGN | rebuilt: 1080p capture, tolerant verdict per origin screen, BLESS limited to cppx-only surfaces; goes green as parity lands |
| 71_visual_regression_lobby | RED-BY-DESIGN | rebuilt likewise; red until lobby cluster parity |

Full suite 2026-06-11 (post scale-unclamp): 21/23 PASS; only 70/71 red (the parity gate).

## Functional — origin-behavior coverage gaps (no scenario yet)

| Behavior | State |
|---|---|
| chat entry/log in-game (T → type → Enter, TEAM/ALL toggle) | UNVERIFIED |
| buy/tech overlay navigation + purchase | UNVERIFIED |
| player-list F1 hold | UNVERIFIED |
| quit-prompt Enter/Esc state machine | UNVERIFIED |
| F2/F4/F5 bindings | UNVERIFIED |
| staging → tech_select → launch full flow vs origin | UNVERIFIED |
| text-entry caps/length limits (alias, chat, password) vs origin | UNVERIFIED |
| scrolling behaviors (controls list, map list) vs origin | UNVERIFIED |

## Architecture guard backlog (17 findings, run 2026-06-11 — must be 0 for done)

| Finding | Where |
|---|---|
| paint-literal ×5 | components/actions/app_button_variant.h:120,185,203,252,253 |
| ~~paint-literal~~ FIXED 2026-06-11 (toggle rework) | ~~components/actions/boolean_setting_row.cppx:50~~ guard now 16 findings |
| paint-literal | screens/character_create.cppx:266 |
| paint-literal ×3 | screens/lobby_connect.cppx:112,120,146 |
| paint-literal | screens/lobby_screen.cppx:872 |
| paint-literal | screens/password_modal.cppx:55 |
| big-switch 9 cases | app_shell/app_root.cpp:61 |
| big-switch 7 cases | components/text/screen_title.cppx:26 |
| big-switch 7 cases | screens/update_screen.cppx:27 |
| god-view 260 lines | screens/character_create.cppx:226 CharacterCreateContent |
| god-view 407 lines | screens/lobby_screen.cppx:591 LobbyScreenView |
