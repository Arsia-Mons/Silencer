# PARITY.md — origin/main parity ledger

States: `PASS` / `DIVERGED` / `UNVERIFIED`. Evidence = tool run from the session that set the state.
Gate: `pixdiff_tolerant.py` printed PASS (global <1%, zero tiles >5%) + `visual_parity_gate.js` overall=PASS.
Goldens: `tests/cli-agent/e2e/golden/` (origin/main @ af4c50c5, 1920×1080). Renders: `/tmp/cppx_renders/`.

## Visual — menu/lobby targets (goldens exist)

All measured 2026-06-11 (iteration 0) with recalibrated tile gate, fresh captures, build @ d3d3f9d4+dirty.

| Surface | State | Evidence (global% / hot tiles / worst tile) | Notes |
|---|---|---|---|
| mainmenu | **PASS — byte-identical** | pixdiff 0.0000%/0 hot, mae 0.00 (2026-06-11, string-variant bake); prior full gate wf_6e530100-b59 6/6 critics | per-phase sprite variants + per-phase string bake |
| options | **PASS (full gate)** | pixdiff 0.0000 byte-identical + gate wf_45c1807c-d48 overall=PASS | "Audio" glyph-phase tile cleared by the string-variant bake |
| options_audio | **PASS — byte-identical** | pixdiff 0.0000%/0 hot, mae 0.00 (2026-06-11); prior full gate wf_d51296b1-328 6/6 critics | |
| options_display | **PASS (full gate)** | pixdiff 0.0000 byte-identical (RGB MD5 match) + gate wf_1f9aedbb-fae overall=PASS 6/6 critics | string bake + fullscreenw row -1 (label pen vx 334) + indDx 1 (right toggle vx 553) |
| options_controls | **PASS (full gate)** | pixdiff 0.0000 byte-identical (raw-RGB md5 match) + gate wf_b79c44ad-0ad overall=PASS 6/6 critics | string bake + titlewrap inset-top 13 (title vy 14) + OR ml 2 (vx 513) |
| lobby_connect | DIVERGED | 1.47% / 42 / 25.9% @780,780 (pre-text-bake 1.46/41) | CORRECTED DIAGNOSIS: button row IS centered right (origin's floating row escapes pad-84 and centers across panel); real diff = button chrome detail (inner inset frame) + widths (L/C 275 vs 250 device px) |
| character_create | DIVERGED | 1.57% / 41 / 17.9% @468,234 (was 1.59/41) | portrait row + panel chrome |
| cc_alias | DIVERGED | 2.17% / 56 / 21.2% @624,468 (was 2.20/56) | alias dialog region |
| cc_select_agency | DIVERGED | 3.74% / 79 / 31.9% (was 7.09/125/40.7) | agency Description paragraph cleared by the string-variant bake (dense prose now phase-exact); residual = panel chrome + portraits |
| lobby_screen | DIVERGED | 3.67% / 94 / 31.9% @234,312 (was 3.68/94) | VERIFIED BY EYE: agent-card emblem scale/pos, WINS/LOSSES/XP row spacing, Agents button high + wrong chrome. ORIGIN SPEC (character_panel.cpp, virtual px → ×1.5 logical): content pad 6, emblem↔info gap 10; left rail emblemBoxW=clamp(inner*18%,40,64) square Contain emblem + LevelBadge(bodyLineH, centered); info col gap 5: name(heading lineH) → details row: stats col gap 10 [stat table gap 2: WINS,LOSSES rows; label col = w("LOSSES")+4] → XP line → actions row h21 (Chrome "Agents" minW 92 padX 12). The LOSSES↔XP gap = 10 virtual (15 logical) — that's the visible golden gap before XP |
| create_game | DIVERGED | 5.06% / 113 / 31.9% @234,312 (was 5.01/113) | map-list pitch fixed (21 logical = origin kMapListLineH 14 virtual; was 16.5+1) — right-panel hot column (38.4 @1248,*) cleared; worst tile is now the shared agent-card region (see lobby_screen spec) |
| game_staging | DIVERGED | 4.20% / 93 / 32.9% @1248,156 | shares lobby panels + right panel |
| tech_select | DIVERGED | 5.49% / 106 / 34.1% @234,312 (was 5.38/106/34.3) | tech grid region divergent; worst tile = shared agent-card "Agents" button (verified by eye: chrome + y-pos, not a text artifact). ±0.1 global movement here/create_game = un-grid-tuned text snapping to cells — resolves with the screens' own parity pass |

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

### [systemic] Glyph-atlas phase (text striping) — SOLVED 2026-06-11, all 5 menu screens byte-identical

Landed: GlyphFonts::string_variant (glyph_fonts.{h,cpp}) + the draw_executor
render_text_glyphs intercept — when a Text draw uses an exact-color face at 1:1
virtual scale (gscale == s), the WHOLE string is baked through origin's chain at
its absolute device cell: glyphs composited at integer virtual pen positions
(vx + i*advance, transparent-skipping, later-over-earlier), then per-device-pixel
magnify src = int((gx-off)/s). Drawn 1:1 at the cell; memo (face, color, string,
X%18, Y%18); textures owned by GlyphFonts (cap 256, recycle-all at cap), NOT the
64-cap TextureRegistry. build_color_face now keeps a CPU atlas copy + legacy dims.
Measured proof that decided the design: the golden satisfies int(gx/s) duplication
exactly over label regions, while the render's per-glyph float pen (advance 11 x
2.25 = 24.75) fits NO single column phase — x-phase drifts WITHIN a string, so
per-glyph atlas variants were unworkable; only a string-level bake covers both axes.
Pen recovery vx=round(floor(dev)/s) needs the authored pen within ~±1 device px of
the golden cell — three labels needed nudges (display fullscreenw mr 3 + indDx 1;
controls titlewrap inset-top 13, OR ml 2). Result: mainmenu, options, options_audio,
options_display, options_controls all pixdiff 0.0000 (byte-identical, mae 0.00).

NEXT: lobby/cc cluster — dominated by panel chrome + agent-card layout (see
lobby_screen ORIGIN SPEC row); text there now snaps to cells (cc_select_agency
7.09 -> 3.74 for free) but pens must be grid-tuned per screen as layout lands.

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
| 70_visual_regression | PASS | GREEN 2026-06-11: all 5 origin-golden screens byte-identical; cppx-only baselines (gallery/modals) re-blessed via the sanctioned path |
| 71_visual_regression_lobby | RED-BY-DESIGN | rebuilt likewise; red until lobby cluster parity (5 diverged 2026-06-11, numbers in the table above) |

Full suite 2026-06-11 (post scale-unclamp): 21/23 PASS; only 70/71 red (the parity gate).
Re-run 2026-06-11 post string-bake: 22 green, 70 red on gallery only, 71 red on the 5 lobby surfaces.

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
| god-view 409 lines | screens/lobby_screen.cppx:597 LobbyScreenView |

## Critic-panel architecture backlog (from gate runs wf_6e530100 + wf_d51296b1, 2026-06-11)

Visual verdicts PASSED; these CODE findings must close before the architecture bar is met:

| Sev | Finding | Fix shape |
|---|---|---|
| HIGH | per-screen nudge wrapper Boxes (options_screen w0-w3, audio musicw/actionsw/titlew, display fullscreenw/smoothw/actionpad, controls actionsnudge) — copy-pasted 1-2px magic margins | own grid-snap ONCE: resolve_legacy_variant draws at the snapped vx/vy it already computes, OR one shared dialog-column primitive; then delete all nudges |
| MED | legacy-variant cache key %18 only valid for quarter-integer s; stale dims stretch cached textures at e.g. s=1.5625 | store bake-time w/h in LegacyVariant, draw at texture dims; fold s into key or fall through to plain path off the quarter-integer set |
| MED | draw_executor legacy intercept sits after tint mods → in-branch un-mod rollback | hoist the intercept above the mod calls (clean early fork) |
| MED | Options-Controls frame rect has two unsynchronized owners (BakeChromeTextures inline block + panel.cppx fallback rect) | screen-owned descriptor/shared constants; extract BakeControlsFrame(rw,rh,uiScale) |
| MED | Save/Cancel row + BooleanSettingRow alignment duplicated at 3 call sites | extract shared DialogActions/SaveCancelRow primitive owning row + alignment |
| LOW | 4x duplicated two-hop virtual-canvas math; unexplained 4.0f/+9 tolerances + doc drift; 640/480 impossible-state fallback; 9 hand-paired bake+register sites; failed bakes not memoized (per-frame rebake churn); split eligibility predicate; wrapper indent drift | helpers + named constants + bake_legacy() pairing + id=0 sentinel |

Critic-panel additions (gate runs wf_1f9aedbb + wf_b79c44ad, 2026-06-11):

| Sev | Finding | Fix shape |
|---|---|---|
| HIGH | string-variant memo key omits render scale s; cache only cleared at shutdown/256-cap — stale wrong-sized text after fullscreen toggle/resize (glyph_fonts.cpp:325-348) | fold quantized s (or out_w/out_h) into the key, store bake-time w/h, flush on output-size change |
| MED | actionsnudge anonymous wrapper around ActionRow + mis-indented JSX (options_controls.cppx:334) | give ActionRow a layout override prop (engine-golden convention) or fold into actionwrap padding |
| LOW | omnibus nudge comment; legacy_w>0?:640 ternary duplication; per-draw GetCurrentRenderOutputSize; ind_dx single-call-site prop on shared BooleanSettingRow; Panel::ControlsFrame owning screen placement (extract BakedFrame if a 2nd appears); exact-color eligibility decided in two places | cleanups |
