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
| lobby_connect | **PASS — byte-identical** | pixdiff 0.0000%/0 hot, mae 0.00 (2026-06-11 sweep post 56725dc0 palette-true caret + per-page text bakes; re-verified post b8fc958b) | dialog sprite + button-patch registered legacy; origin pen grid (log 291/109+11k); capture pins lobby port 63532 AND waits for AUTHENTICATING (the log renders both). Scenario-71 harness now green (d739bc5d) |
| character_create | **PASS (full gate)** | 0.0000 byte-identical + gate wf_195b6b93 overall=PASS 5/5 critics | chrome_panel/row_plate registered; create plate on int-cast cell (166,94); List label pen cell 102 |
| cc_alias | **PASS (full gate)** | 0.0003%/0 hot + gate wf_195b6b93 overall=PASS (one low: caret 1 device row short — blink phase) | modal on cell (283,161); origin caret (1v yellow, Body lineH); title pen 349. 71-harness shows 1 hot tile (caret-blink frame nondeterminism) |
| cc_select_agency | **PASS (full gate)** | 0.0540%/0 hot (cd34af19) + gate wf_6a6a0a1f overall=PASS 5/5 critics — was 0.259/8 hot | brackets anchored absolutely at bx=PenGrid::L(7*len)+35 (flow placement flipped bakes per label-length parity); agency rows' box -1.5 with AppButton sparse padding keeping the label pen on its golden cell. Polish (critic lows/med, below gate): bracket glyph two-tone ramp (24,124,20/8,84,0/0,44,0 vs flattened 32,132,36+grey shadow), oval right-cap 2px clip @x903-904, right-panel clip 1px @x1080 |
| lobby_screen | **PASS — byte-identical** | pixdiff 0.0000%/0 hot, mae 0.00 (2026-06-11 sweep post 56725dc0; re-verified post 73bb8fe2 responsive panes + b8fc958b refactors) | origin stepped pane (now responsive: resolve_lobby_panes ports ResolveSteppedPaneLayout, byte-identical at design canvas) + agent card + chat internals + border snap + nine-slice/contain bakes |
| create_game | **PASS (full gate)** | 0.0082%/0 hot (4f7c188a) + gate wf_6a6a0a1f overall=PASS 5/5 critics | Game Options origin form (scrollbar, BodySm values), Select Map origin list + abs footer grid (map list + Create button now stretch within the tall cell). Polish (critic low): scrollbar thumb 1px rounding (fill to x1168, caps y211/y368) |
| game_staging | **PASS — byte-identical** | pixdiff 0.0000%/0 hot, mae 0.00 (4f7c188a staging button width; re-verified 2026-06-11 sweep post b8fc958b) | origin roster anchors (emblem/ready/name/level via kLobbyTallGrid), staging buttons 3/11/39, map-name Title variant, presence [game] suffix, default tech Laser+Rocket on connect |
| tech_select | **PASS — byte-identical** | pixdiff 0.0000%/0 hot, mae 0.00 (2026-06-11 sweep post 56725dc0 per-page palette tables; re-verified post b8fc958b) | origin tech grid (toggles native bank7 18/19 + dim 64 copies, labels "name (N)" 128/64, slots LegacyPalette(129,144)); agency-specific buyables filtered |
| mission_summary | **PASS — byte-identical** | pixdiff 0.0000%/0 hot, mae 0.00 (2026-06-11, first-nudge iteration); scenario 74 PASS; 13-screen sweep unchanged after | NEW GOLDEN 2026-06-11 (1920×1080, two origin runs byte-identical — ORIGIN_GOLDENS.md provenance incl. the two uncommitted origin capture patches). Real match-end drive both sides: GAS timeLimitSecs=5 bundle edit + dedicated server SIGSTOP on the lobby [stats] line (tools/cap/cap_mission_summary_{origin,cppx}.sh). ENGINE FIX (ours, committed): GameStateObject now replicates (requiresauthority was never set — origin never sends it) and replicas adopt the replicated winningTeamId instead of clobbering it; without this a time-limit match end can NEVER reach MISSIONSUMMARY on a lobby client (origin bug — origin drops to LOBBY via CONNECTION LOST; the secrets path reaches the summary in origin, so the screen itself is origin-reachable). Deliberate behavior divergence, documented here. Screen: verbatim origin floating grid ((sx-5,sy-19)×1.5 inside the cc chrome_panel frame; oval boxes nudged -0.5x/-0.75y onto their sprite cells), origin summaryLines port in BuildProgression, new Prompt-face kTextBody bake. Functional gap: stats scrollbox renders the top window only (origin wheel-scrolls it; scroll intent not yet wired) |

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

LOBBY CLUSTER LANDED 2026-06-11 (7/8 at the printed gate, character_create
byte-identical). New systemic mechanisms: executor hairline-border snap
(snap_legacy_hairline_border — idx216/220 hairlines snap per side to origin
virtual cells), nine-slice element bake (origin DispatchButtonNineSlice tiled
bands), contain + stretch legacy flavors, origin caret, kTextPresenceHeader/
kTextRosterLevel/kTextTechDim/kTextTechSlots variant faces, texture cap 256
(64 was exceeded by session-accumulated variants — silent raw-draw fallback).
REMAINING items all closed 2026-06-11: cc_select_agency cd34af19 (0.0540/0
hot); scenario 53 73bb8fe2 (resolve_lobby_panes ports ResolveSteppedPaneLayout
— panes reflow below the design canvas, byte-identical at it); scenario-71
d739bc5d (capture-state: AUTHENTICATING before lobby_connect cap, cc_alias
capped before typing — no caret-blink machinery exists or was needed).

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

### [systemic] Sub-tolerance palette drift — critic gate wf_195b6b93-ab5 (2026-06-11)

RESOLVED 2026-06-11 by 56725dc0 (palette-true caret via VisualStyle.Caret + per-
presentation-page palette tables for the variant text bakes) and 4f7c188a/cd34af19
(pen-grid geometry): lobby_connect/lobby_screen/tech_select/game_staging are now
byte-identical, so every family below is closed except the create_game residual
(0.0082, sub-1.3% tiles). Table kept for the record.

| Family | Evidence | Root cause / fix shape |
|---|---|---|
| Input caret = palette idx 140, NOT yellow | golden lobby_connect caret rgb(116,156,104)@x858-859; golden cc_alias caret rgb(252,252,0); render always 252,252,0 | origin TextInputOpts.caretColor=140 (legacy default, text_input.h:49) resolved per-screen palette (cc palette idx140=yellow, lobby palette idx140=sage). OUR kCaretFill is hardcoded in draw_command_builder.cpp:29. Fix: caret color joins resolved VisualStyle (theme.caret exists unused); screens feed palette-resolved idx140 via chrome provider |
| lobby_connect caret pen +10 device px | golden x858 vs render x868 (= our Input's ~7.67-logical default content padding; origin contentInsetX=7v measured from input box, our well box already sits AT the pen) | zero the field's content inset or shift the well; verify typed-text pen too (goldens have empty fields — text start untested) |
| presence-header green ('In Lobby'/'Pregame') | golden sage rgb(92,148,92) fill + rgb(76,124,68)/(68,108,64) AA; render rgb(80,148,84)/(48,140,60) (~1000 px/screen on lobby_screen, create_game, game_staging, tech_select) | the brightness-160 presence variant face bakes the wrong effect color/ramp — re-derive from origin TextEffect for this state |
| version 'v.00058' amber | golden rgb(140,64,8); render rgb(152,72,20) on the four lobby-cluster screens (~558 px each; menu screens byte-identical so menu path is right) | lobby-cluster version bake uses a brighter amber ramp than origin's lobby-palette text effect |
| tech_select disabled-row dim | golden gray-greens rgb(8,56,8)/(16,64,16); render rgb(4,76,0)/(0,44,0)/(0,52,0) — blue channel zeroed | dim-64 formula diverges from origin's palette-brightness mapping; recompute via LegacyPalette brightness like origin, not a per-channel scale |
| lobby_connect left outer frame 1px thin | golden dark col rgb(8,84,0) x639-641 (3px), render x640-641 (2px); right/top/bottom byte-identical | dialog sprite bake/draw rect floors the left edge asymmetrically — snap panel box x |
| create_game geometry | spinner values top-aligned (golden +5px below label baseline: '0' y255-272 etc.); Game Options bottom hairline 2px@y408-409 vs golden 3px@y405-407; scrollbar thumb x1158 vs 1157 | pen-grid fixes in GameCreatePanel |
| game_staging buttons | Choose Tech/Charge Team/Ready right border ends x1174-1176, golden x1175-1178 (452 vs 454 px wide) | widen 2 device px (button cell or nine-slice rect) |

## Visual — no origin golden (functional checks only)

| Surface | State | Notes |
|---|---|---|
| gallery | n/a (visual) | cppx-only showcase; golden is a stale cppx render — do not gate |
| message_modal | n/a (visual) | no standalone origin trigger; golden stale — do not gate |
| password_modal | n/a (visual) | no standalone origin trigger; golden stale — do not gate |

## Visual — in-game (ALL 8 GOLDEN SURFACES PASS 2026-06-11, e2e scenario 72)

Enumerated from origin source 2026-06-11 (Explore agent over .worktrees/origin-capture).

**Goldens captured 2026-06-11** via `tools/cap/cap_ingame_origin.sh` →
`tests/cli-agent/e2e/golden/ingame_*.png` (8 PNGs, 640×480, pristine af4c50c5
binary — fade patch stashed for the capture, then restored; the patch's FADEOUT
branch blacks out the in-game UI palette). Full provenance + per-golden
nondeterminism masks (rain streaks; minimap inset x235-406 y419-479) in
ORIGIN_GOLDENS.md. Two independent runs byte-identical outside those masks.
Scene: Tutorial, AGENCY04.SIL, paused sim, feedback-stepped to message_i
anchors; overlays forced via origin's `ingame_ui_mode` control op; quit prompt
via a `--tui` session injecting the ESC scancode (control `key` op can't reach
the quitstate machine).

**Capture design (derived 2026-06-11) — the earlier "HUD doesn't capture headless" deferral
is WRONG for origin.** Origin's game_loop.cpp renders world + in-game HUD/overlays
SOFTWARE-side into the screenbuffer every frame (RenderClientUiFrame →
BuildInGameHudUi/BuildInGameOverlaysUi → clay_bridge::Render(game, &surface, cmds));
`--headless` only skips Present()/cursor (game_loop.cpp:36,239,352). The control-port
`screenshot` op captures GetScreenBuffer() (controldispatch.cpp:1027 HandlePostRender) —
HUD included. Caveat: in-game the screenbuffer is FIXED 640×480 (kLegacyRender;
game_loop.cpp:205 ResizeRenderSurfacePixels), so headless in-game shots are 640×480.

Golden pipeline (EXECUTED 2026-06-11 — actual pipeline deviated from the plan below):
1. ~~Build with the fade patch~~ → built PRISTINE (patch stashed, restored after): the
   patch's FADEOUT branch blacks out the in-game UI palette (see ORIGIN_GOLDENS.md).
2. ~~Real match via lobby~~ → Tutorial single-player (AGENCY04.SIL): origin's own
   `ingame_ui_mode` control op forces chat/buy/tech/playerlist deterministically, and
   pause + feedback-stepping on message_progress pins the exact sim tick. A real lobby
   match adds nothing visual that the tutorial HUD lacks except multi-team strips.
3. `screenshot` → 640×480 PNG per surface/state. ✓
4. ~~Upscale to 1920×1080~~ → NO upscale: gate both sides natively at 640×480 (our
   client also renders in-game UI at surface size headless, step 6 below).
5. Provenance + nondeterminism masks in ORIGIN_GOLDENS.md; saved as
   tests/cli-agent/e2e/golden/ingame_*.png. ✓ Repeatable: tools/cap/cap_ingame_origin.sh.
6. cppx-side path VERIFIED 2026-06-11 (read-only code analysis): headless (no window),
   RenderCppxClientUiFrame renders the UI at SURFACE size (game_ui_pipeline.cpp:651 —
   window-pixel size only when a window exists), and in-game the surface is forced to
   640×480 (game_renderer.cpp SyncRenderSurfaceToWindowPixels: map.loaded ⇒
   kLegacyRenderWidth/Height, same as origin). CaptureCompositedFrame's headless fallback
   (game.cpp:25) CPU-composites the premultiplied cppx layer over the palettized world
   when sizes match — so headless in-game screenshots INCLUDE the HUD at 640×480 on our
   side too. Gate plan: capture BOTH sides at 640×480 and pixdiff there (or upscale both
   with the same int(d/s) NEAREST maps). cppxScale floor 480/720 ⇒ origin-native 640×480
   proportions. In-game presentation palette = page 0 (BakeChromeTextures page_color).
   Recreation spec for all 13 HUD surfaces: INGAME_SPECS.md (worktree root).

**RECREATED 2026-06-11** (in_game_screen.cppx verbatim port of origin ui/hud/*;
use_hud + HudChrome bakes; full mechanism list in the 4cc92899+ commits). Gate =
`pixdiff_tolerant.py --mask` per ORIGIN_GOLDENS.md masks (minimap inset
235,419,406,479; rain-ripple deck band 0,296,640,340; right-edge rain sliver
624,0,640,419), our rain layer disabled via the `rain` control op (the
goldens' frozen rain is the residual global ~0.2%). Camera pinned to each
golden via the `camera` op + phase correlation (follow-cam has a 100px
y-hysteresis; rest position is render-cadence-dependent). Evidence: scenario
72 PASS in the full-suite run (suite all green, 25 scenarios) + standalone
sweep 2026-06-11: hud_base 0.1978/0 hot, chat_open 0.1916/0, chat_history
0.1921/0, player_list 0.1821/0 (worst tile 4.8% — deterministic residual,
under gate), buy_tech 0.1629/0, tech_overlay 0.0124/0 (byte-identical outside
golden rain), messages 0.1672/0, quit_prompt 0.3766/0.

| Surface | State | Origin anchor |
|---|---|---|
| hud_status_bar (minimap frame, fuel/health/shield/files gauges, poison, weapon glow+bracket, inventory) | **PASS** — ingame_hud_base 0.1978/0 hot | ui/hud/hud_status_sprites.cpp |
| hud_readouts (ammo counter via palette-Alpha-LUT bake, per-weapon ammo, credits, health/shield numerics) | **PASS** — ingame_hud_base | ui/hud/hud_readouts.cpp:14-88 |
| hud_team_strip (per-team peer sprites, in-base/secret pulses, secret slots, beaming) | **PASS** — ingame_hud_base (1 team) + ingame_tech_overlay (2-team); pulse states still uncovered by goldens (ramp variants implemented, EnsureHudRampVariant) | ui/hud/hud_teams.cpp |
| hud_secret_overlay (9-line hack progress, highlight pulses) | **PASS** — ingame_secret_overlay 0.0859/0 hot (2026-06-11, scenario 76); base built via the TUI tutorial drive (cap_ingame_origin_extra.sh); panel bg + 9 dim hack lines gated; highlight flashes implemented (EnsureHudRampVariant pulse 120-136) but not golden-covered (tutorial highlightsecrets only sets in case 21) | ui/hud/hud_secret_overlays.cpp:73-110 |
| hud_trace_time ("Government Trace Time: NNN") | NO GOLDEN — view + model IMPLEMENTED from spec (build_trace_time; terminal/player tracetime in snapshot). Trigger needs beaming = team.secretprogress >= 180 (gasloader secretProgressBeamThresh, team.cpp:74) accrued only while HACKING (player.cpp:2136, +secretInfo per terminal: GAS 45 big/15 small, terminal.cpp:229/239) ⇒ fully draining ≥4 of AGENCY04's terminals across multiple map levels — a multi-terminal navigation drive not yet scripted. ALSO subject to the same origin stale-temppalette dim as hud_system_camera if the drive enters the base | ui/hud/hud_readouts.cpp:90-103 |
| hud_system_camera (inset frames ×2) | **GOLDEN + IMPLEMENTED, gate WAIVED** — ingame_system_camera captured (rocket flight via TUI drive: base entry → buy Rocket → fire; 2 runs rain-only diff) and our render draws frame+inset (scenario 76 capture asserts presence; the inset world view matched the golden byte-exact pre-mask). Full-frame gate blocked by an ORIGIN palette-history bug: after the first base entry the ambience repaint (origin game_loop.cpp:184) stamps stale temppalette high indices (palette.cpp:227 writes only 2..114; last full-range write = boot fade phase 14) ⇒ all UI/parallax indices present at 112/128 from then on (measured 8.8k px at ratio 1.143 exactly). Our pre-baked RGBA HUD layer composites at page-0 colors and cannot track live palette mutation — reproducing this needs palette-space UI compositing (architecture follow-up) | ui/hud/hud_system_camera.cpp:13-40 |
| chat_overlay (history 5 lines, input + caret, ALL/TEAM toggle) | **PASS** — chat_open 0.1916/0 + chat_history 0.1921/0; world-backed compose (chat_set_text intent), caret = page-0 idx140 with origin's wall-clock blink (Input show_caret) | ui/hud/hud_chat_overlay.cpp:142-236 |
| buy_tech_overlay (5-row scroll window, UP/DOWN/repair price column, credits/viruses footer, wall-clock selection pulse) | **PASS** — buy_tech 0.1629/0 + tech_overlay 0.0124/0; focusable ghost rows (chromeless) keep Up/Down/Enter nav (scenario 51) | ui/hud/hud_buy_tech_overlay.cpp:58-190 |
| player_list_overlay (F1; per-team emblem + peer agency stats) | **PASS** — player_list 0.1821/0; dim fill composited through origin's palette alpha LUT (game.cpp headless composite) | ui/hud/hud_player_list_overlay.cpp:16-98 |
| ingame_messages (center reveal text, typed colors) | **PASS** — messages 0.1672/0 (type 0: per-char reveal + pulse brightness via hud_text_key(208,b) faces, b=64..160 baked). Types 1-4/10/11/20 colors ported but their exact-color faces NOT baked (no goldens) — they fall back to tinted coverage glyphs | ui/hud/InGameOverlays.cpp:58-145 |
| top_ticker (scrolling top message) | **PASS** — ingame_top_ticker 0.2004/0 hot (2026-06-11, scenario 76). The "F4 contaminates" deferral was wrong: headless audio is disabled so MusicPaused() is always false and F4 deterministically shows "          *MUSIC PAUSED*" with no other visual effect; topmessage_i pinned at 2 via paused stepping. View implemented (build_top_ticker, BodySm window start=max(0,progress/2-24)) | ui/hud/InGameOverlays.cpp:185-209 |
| status_lines (bottom-center stack, fading) | **PASS** — ingame_status_lines 0.0030/0 hot (2026-06-11, scenario 76, + civilian-rand mask 270,230,330,290). Trigger: keyuse with INV_BASEDOOR next to the deck data terminal (x=1824) → CanCreateBase TERMINAL-in-AABB fail → "Can't build a base here" (208); golden captured MID-FADE at time=8 (brightness 64) so the fade arithmetic is gated. View implemented (build_status_lines, shadow max(8,b-64), face-7 (208,b) bakes b=8..128 step 8) | ui/hud/InGameOverlays.cpp:147-183 |
| quit_prompt ("Hit Enter To Quit") | **PASS** — quit_prompt 0.3766/0 (Prompt face LegacyPalette(202)); captured via TUI ESC scancode edges like the golden | ui/hud/InGameOverlays.cpp:211-227 |

Hardcoded in-game bindings to verify functionally: T chat, F1 player list, F2 team colors,
F4 music toggle, F5 random music, Enter quit-confirm flow.

## Functional — e2e suite (fresh full run 2026-06-11 post in-game-surfaces + functional scenarios: ALL 33 GREEN)

| Scenario | State | Evidence |
|---|---|---|
| 00..22,30,31,40,50,51,52,60,70 | PASS | fresh full run 2026-06-11 (binary @ b8fc958b; re-verified post in-game work — 70's modal baselines re-captured at the logo HOLD + stability-polled, see ORIGIN_GOLDENS.md) |
| 53_lobby_create_options_scroll | PASS | 73bb8fe2 — resolve_lobby_panes ports origin ResolveSteppedPaneLayout; green at 1280x720 / 640x480 / 1000x1100 / 390x844 |
| 72_visual_regression_ingame | PASS | NEW 2026-06-11 — all 8 in-game goldens gated with the documented masks; full-suite run all green (25 scenarios) |
| 73_ui_click_sounds | PASS | NEW 2026-06-11 — ui_audio edge counter asserts hover-enter/dedupe/nav triggers |
| 71_visual_regression_lobby | PASS | d739bc5d — harness now captures lobby_connect at AUTHENTICATING (log populated) and cc_alias BEFORE typing (golden field is empty); the "caret blink" theory was wrong — the caret draws unconditionally while focused. Two consecutive green runs |
| 76_visual_regression_ingame_extra | PASS | NEW 2026-06-11 — top_ticker/status_lines/secret_overlay gated vs the new gameplay-driven goldens (system_camera captured non-gated, see its row); full-suite green |
| 77_ingame_chat_behavior | PASS | NEW 2026-06-11 — Enter sends/Esc cancels/channel toggle/100-char cap (origin-verified single-player no-history quirk) |
| 78_ingame_bindings_quit | PASS | NEW 2026-06-11 — F1 hold, F2 toggle, F4 ticker, quit machine 0→1→2→3→0 + RETURN quit, via TUI scancodes |
| 79_text_input_caps | PASS | NEW 2026-06-11 — lobby 16/28 + modal 20 caps via controlled-Input round-trip |
| 80_ingame_buytech_nav | PASS | NEW 2026-06-11 — selection clamp (no wrap) + Esc close |

Earlier same-day milestones: iteration-0 13/23 → post scale-unclamp 21/23 → post string-bake 22/23 → 24/24.

## Functional — origin-behavior coverage gaps (no scenario yet)

| Behavior | State |
|---|---|
| chat entry/log in-game (Enter sends, Esc cancels, TEAM/ALL toggle, 100-char cap) | **PASS (2026-06-11)** — scenario 77. IMPLEMENTED en route: compose Enter→send (Input on_activate; live value via fiber state since the snapshot prop is a frame stale), Esc→cancel (GameUiPipeline cancel routing — ClientUi's cancel pass only pops Overlays, the in-game HUD is the base screen; mirrors origin InGameUiController Cancel incl. buy/tech close), channel-toggle ghost target over the prefix (origin "ingame.chat.channel"). ORIGIN-VERIFIED QUIRK: in single-player the sent line never reaches history — SendChat fires MSG_CHAT at the authority (yourself) whose socket is never bound (tick_singleplayer.cpp comments out world.Listen); confirmed against the origin binary; the scenario asserts the origin-exact behavior. ENGINE FIX: pointer clicks no longer fire on_activate on Input nodes (focus only — origin inputs submit on RETURN alone; previously clicking the password-modal/lobby fields fired their submit) — focus.cpp confirmed_by_pointer + ClientUi gate |
| buy/tech overlay navigation + purchase | **PASS (2026-06-11)** — scenario 80 (Up/Down moves + clamps both ends per origin ClampBuyTechSelection — no wrap; Esc closes) + scenario 76's drive asserts a REAL purchase end-to-end (Down+Enter at the base inventory station → Player::BuyItem → rocketammo>0 gates the tutorial case-13 message). Insufficient credits = silent no-op in origin (BuyItem early-return, no status) — nothing to assert |
| player-list F1 hold | **PASS (2026-06-11)** — scenario 78: HOLD semantics (origin events: OnScancodeDown F1 → SetShowingPlayerList(true), OnScancodeUp → false) via TUI scancodes |
| quit-prompt Enter/Esc state machine | **PASS (2026-06-11)** — scenario 78: ESC down/up 0→1→2 (prompt), second ESC 2→3→0 (cancel), RETURN held while 1/2 → CheckForQuit → MAINMENU (tutorial unauthenticated). quit_state now exposed in world_state |
| F2/F4/F5 bindings | **PASS (2026-06-11)** — scenario 78: F2 toggles SetShowingTeamColors (world_state show_team_colors), F4 → "          *MUSIC PAUSED*" ticker (headless audio disabled ⇒ MusicPaused() always false ⇒ same text on repeat — origin-exact), F5 has no headless-observable effect (LoadRandomGameMusic + PlayMusic both no-op with audio disabled) |
| staging → tech_select → launch full flow vs origin | UNVERIFIED — scenario 31 covers create→staging, the mission_summary drive (tools/cap) covers a full launch via lobby; no single launch-flow scenario yet |
| text-entry caps/length limits (alias, chat, password) vs origin | **PASS (2026-06-11)** — scenario 79 (lobby username 16 / password 28, modal password 20 — origin lobby_connect_screen.h username[17]/password[29], password_modal.cpp maxLength 20) + scenario 77 (chat 100 = chatText[101]). IMPLEMENTED en route: the caps (set_username/set_password/set_alias/modal on_change substr truncation — our Inputs are fully controlled so the consumer cap is authoritative); alias 16 shares the implementation (not separately asserted — needs the lobby boot) |
| scrolling behaviors (controls list, map list) vs origin | PARTIAL — scenarios 12 (controls list scroll) + 53 (map-list/options reflow) cover keyboard/scroll reachability; wheel-vs-origin pixel comparison not done |
| hover/focus interaction VISUALS vs origin | **PASS (2026-06-11)** — hover_mainmenu_oval golden (origin, 2 runs byte-identical) gated at 0.0000/0 hot; chrome NEGATIVE verified both sides (origin: hovered Login cell byte-equals the rest golden ×2 runs; ours: 0 diff px in the cell before/after hover); scenario 75 green. IMPLEMENTED: origin's 5-phase ramp in AppButton (sprite base+phase at brightness 128+2p baked per phase + registered for the device-cell variant swap — oval Md/Sm/Lg + LegacyRow; label ramps through hud_text_key(0,128+2p) face bakes), one phase/42ms toward target, disabled pins 0. Target = hover (NEW engine on_hover enter/leave edge — use_hovered can't serve product components, the host's fiber is the substrate Button's) OR focus with a real input source (programmatic autofocus does NOT ramp — origin never autofocuses, keeps rest goldens byte-stable; hover does not move focus in our model, per scenario 16). DEVIATIONS (documented): `selected` does not ramp yet (origin selectedVisual; no golden exercises it — cc preview-selected rows must stay phase 0 at rest anyway); Ghost/Text 128→136 brightness ramp not implemented (no golden; in-game ghost rows must stay chromeless). FIXED EN ROUTE: use_clock delta_ms collapsed to 0 on multi-layer frames (computed per screen layer, not per frame) — no prior consumer animated on overlays, now snapshotted once per render frame |
| UI interaction SOUNDS | **FIXED 2026-06-11** — ClientUi publishes per-frame UiAudioEvents (hovered audible Button / activate / keyboard-nav onto one; audible = enabled Button only, per origin: toggles/inputs silent); GameUiPipeline (sole Audio owner) dedupes the hover edge and plays GAS soundUIClick via Audio::PlayUI; edge counter exposed headless via the `ui_audio` op. Evidence: scenario 73 (hover-enter +1, steady hover 0, second hover +1, nav +1) green in the full suite |

## Architecture guard backlog (15 findings, re-confirmed 2026-06-11 post b8fc958b — must be 0 for done)

(56725dc0 had pushed LobbyConnectView over the 200-line god-view threshold —
16 findings; the connect_status_log extraction in b8fc958b restored 15.
character_create's CharacterCreateContent is now 300 lines and lobby_screen's
LobbyScreenView 494; line numbers below drift with edits — re-run for exact.)

`python3 clients/silencer/tools/react_architecture_guard.py --root clients/silencer` (NOT
--root .../src/client/ui — that path exits 0 silently; line numbers below from the fresh run).

| Finding | Where |
|---|---|
| paint-literal ×4 | components/actions/app_button_variant.h:142,221,293,294 |
| paint-literal | screens/character_create.cppx:268 |
| paint-literal ×3 | screens/lobby_connect.cppx:98,106,139 |
| paint-literal | screens/lobby_screen.cppx:1073 |
| paint-literal | screens/password_modal.cppx:55 |
| big-switch 9 cases | app_shell/app_root.cpp:61 |
| big-switch 7 cases | components/text/screen_title.cppx:26 |
| big-switch 7 cases | screens/update_screen.cppx:27 |
| god-view 292 lines | screens/character_create.cppx:228 CharacterCreateContent |
| god-view 494 lines | screens/lobby_screen.cppx:777 LobbyScreenView |

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

Critic-panel additions (gate run wf_195b6b93-ab5 over the lobby-cluster diff adb39e78..HEAD, 2026-06-11 — 25 findings):

| Sev | Finding | Fix shape |
|---|---|---|
| ~~HIGH~~ DONE b8fc958b | pen-grid anchoring idiom duplicated raw at every site | PenGrid (pen_grid.h) owns L() + per-panel origin (kLobbyTallGrid); all 8 -797/-100 sites + both lambdas + lobby_connect inline + character_create bracket pen converted |
| ~~MED~~ DONE b8fc958b | dead code: image_patch_sub; ignored chrome focus param; bracket_w/h | deleted (incl. the chrome_btn_focus bake/registration/field) |
| ~~MED~~ DONE b8fc958b | legacy-fit flavors = 3 mutually-exclusive bools + 3 register fns + smeared dispatch | LegacyFit enum (Cell/NineSlice/Contain/Stretch), one register_legacy + one resolve_legacy |
| MED | composition regressions: TechListCell/StagingRosterCell became 60-90-line for-loop orchestration (named row components deleted); lobby_connect field label/well blocks pasted ×2 (field_label helper deleted); 6 inline text_patch paints where a BodyText variant is the shape | re-extract row components + text variants |
| MED | use_chrome duplicated truth: agency_emblem_ws/hs[5] arrays added while scalar agency_emblem_w/h kept; parallel-array growth (emblem ×5 fields, ready ×7) | struct-of-arrays → array of small structs; delete scalars |
| MED | caret rect hardcoded in SHARED engine runtime (draw_command_builder.cpp:486: width 1.5f "1 legacy virtual px", height 16.5f) — Silencer-specific constants in the engine | derive from resolved style (joins the caret-color fix above) |
| LOW | kv_row bool `sm` flag-param; 2×2 nested-ternary ready-texture select; decorative scrollbar built inline; "neww" margin-nudge wrapper pasted in both branches; eseam/cseam boolean-blind frame_patch_sides calls; per-row whole-struct closure captures; function-local static bake scratch buffers; bake registration fn accreting per-asset paragraphs; draw_emblem stored-derived flag; bloat comment lobby_ui_model.cpp:230; bake_element_nineslice/stretch copy ~25 lines of two-hop scaffolding | cleanups, table-driven bakes |

Critic-panel additions (gate run wf_6a6a0a1f over f9b337de..HEAD, 2026-06-11 — visual: BOTH screens PASS; code: 15 findings, deduped):

| Sev | Finding | Fix shape |
|---|---|---|
| MED | chromeless-input StyleStatePatch triplicated (character_create alias_field_style, lobby_connect field_style, password_modal) — one parity fix required the same edit at 3 sites | extract tokens::chromeless_field_style(caret_color) or a ChromelessInput component |
| MED | lobby pane arithmetic re-resolved at 6 points/frame; lobby_right_cell_layout() calls use_app() INSIDE a free layout helper (hook-in-helper) while sibling lobby_chat_layout(chat_w) takes a param | resolve panes once in LobbyScreen, pass values down (match lobby_chat_layout's shape) |
| MED | stale stacked kv_row comments (old `sm` comment kept when `cycle` block added, lobby_screen.cppx:345-351) | delete the old block |
| MED | advantage_bracket() dead after inline abs_at replacement (character_create.cppx:110-126) | delete |
| LOW | dead plumbing: caret_game (use_chrome.h:140) and canvas_h (use_app/app_provider/pipeline) written never read; INGAME_SPECS.md duplicate companion line; kv_row `cycle` flag forks typeface+anchoring at once; lobby_connect panel_style/chrome_frame split conditionals on one predicate | cleanups |
