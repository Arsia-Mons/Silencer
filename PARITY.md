# PARITY.md — origin/main parity ledger

States: `PASS` / `DIVERGED` / `UNVERIFIED`. Evidence = tool run from the session that set the state.
Gate: `pixdiff_tolerant.py` printed PASS (global <1%, zero tiles >5%) + `visual_parity_gate.js` overall=PASS.
Goldens: `tests/cli-agent/e2e/golden/` (origin/main @ af4c50c5, 1920×1080). Renders: `/tmp/cppx_renders/`.

## Visual — menu/lobby targets (goldens exist)

All measured 2026-06-11 (iteration 0) with recalibrated tile gate, fresh captures, build @ d3d3f9d4+dirty.

| Surface | State | Evidence (global% / hot tiles / worst tile) | Notes |
|---|---|---|---|
| mainmenu | DIVERGED | 0.83% / 18 / 10.0% @1170,312 (was 2.32/49/32.5) | labels ±1px (cross-correlated); residual = sprite-interior striping phase (GPU center-sample vs origin floor) — candidate global executor fix |
| options | **PASS** | pixdiff PASS 0.26%/0 hot (4.2 max) + gate wf_45c1807c-d48 overall=PASS, 6/6 critics 0.95-0.97 (2026-06-11) | fixed-width ovals + gap 28.5 (origin kButtonGap 19) + label pad 7.5/4.5 |
| options_audio | DIVERGED | 0.61% / 21 / 12.0% @1170,312 (was 2.23/52/32.5) | toggle semantics fixed (origin: l=12 on/13 off, r=15 on/14 off — was mirrored single cell); cells now ±2px; residual = label-baseline band + striping phase |
| options_display | DIVERGED | 1.48% / 40 / 23.1% @702,624 (was 2.98/70/32.5) | Save/Cancel pills ±2px; render label band extends ~5px lower than golden in action row |
| options_controls | DIVERGED | 7.75% / 202 / 28.8% @1638,780 | UNCHANGED by backdrop fix — keybind grid itself diverges (row pitch/columns); panel covers most backdrop |
| lobby_connect | DIVERGED | 1.46% / 41 / 25.9% @780,780 | CORRECTED DIAGNOSIS: button row IS centered right (origin's floating row escapes pad-84 and centers across panel); real diff = button chrome detail (inner inset frame) + widths (L/C 275 vs 250 device px) |
| character_create | DIVERGED | 1.59% / 41 / 17.9% @468,234 (was 2.17/58) | portrait row + panel chrome |
| cc_alias | DIVERGED | 2.20% / 56 / 21.2% @624,468 (was 2.77/73) | alias dialog region |
| cc_select_agency | DIVERGED | 7.09% / 125 / 40.7% @1092,546 | agency Description paragraph: glyph metrics/wrap slightly off → dense text amplifies |
| lobby_screen | DIVERGED | 3.68% / 94 / 31.9% @234,312 (was 4.00/114) | VERIFIED BY EYE: agent-card emblem scale/pos, WINS/LOSSES/XP row spacing (golden has gap before XP), Agents button sits higher + wrong chrome |
| create_game | DIVERGED | 5.24% / 114 / 38.4% @1248,468 | Select Map list: tighter row pitch than golden + panel ~20px right |
| game_staging | DIVERGED | 4.20% / 93 / 32.9% @1248,156 | shares lobby panels + right panel |
| tech_select | DIVERGED | 5.38% / 106 / 34.3% @1326,312 | tech grid region divergent |

Measurements above re-run 2026-06-11 after the backdrop two-hop bake landed (build OK,
fresh captures). Next systemic family: chrome sprites (ovals/panels/buttons) are baked
native and scaled single-hop by the executor; origin scales them through the same
two-hop virtual-canvas chain as the backdrop — sizes land ±few px and striping differs.

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
| 17_character_create_focus | DIVERGED | NOT_FOUND focusable widget AliasConfirm |
| 21_main_menu_layout | DIVERGED | Tutorial button out of bounds in 640×480 (y=231 h=50 w=294 x=372) |
| 30_lobby_login | DIVERGED | NOT_FOUND focusable widget Continue |
| 31_lobby_create_staging | DIVERGED | NOT_FOUND focusable widget Continue |
| 40_lobby_basic | DIVERGED | NOT_FOUND focusable widget Continue |
| 50_resize_screenshot | DIVERGED | (in failing set; capture exact error on repair) |
| 52_menu_ui_scale_resize | DIVERGED | Options button outside 640×480 surface (x=372 y=432) |
| 53_lobby_create_options_scroll | DIVERGED | Options overlay missing button Done |
| 70_visual_regression | DIVERGED | 33 diffs >0.40% — expected red until visual parity lands |
| 71_visual_regression_lobby | DIVERGED | character_create 100% + AliasConfirm — expected red until parity |

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
