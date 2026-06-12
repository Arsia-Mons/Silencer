# UPLIFT.md — design-intent audit ledger

Parity (PARITY.md) reproduced origin/main exactly, defects included. This
ledger catalogs where the golden was a faulty target: origin implementation
accidents we recreated that should instead be elevated to the inferred
intent. Findings are PROPOSED until the user signs off per-item.

State: IN PROGRESS

## Coverage

| Unit | State | Findings |
|---|---|---|
| [systemic] whole-frame magnify: glyph striping | EXAMINED 2026-06-12 | U-1 |
| [systemic] whole-frame magnify: sprite phase striping | EXAMINED 2026-06-12 | U-2 |
| [systemic] background image double-scaling / banding | EXAMINED 2026-06-12 | U-3 |
| [systemic] float-rect flooring: 1px seams & jitter | EXAMINED 2026-06-12 | U-4, U-5 (no ORIGIN-side candidates — see unit note) |
| [systemic] palette quantization & dim formulas | EXAMINED 2026-06-12 | U-6 (+ unit note: dim/brighten LUTs clean at all used combos) |
| [systemic] spacing/alignment consistency across siblings | EXAMINED 2026-06-12 | none (see unit note) |
| mainmenu | EXAMINED 2026-06-12 | none (see unit note) |
| options | EXAMINED 2026-06-12 | none (see unit note) |
| options_audio | EXAMINED 2026-06-12 | none (see unit note) |
| options_display | EXAMINED 2026-06-12 | none (see unit note) |
| options_controls | EXAMINED 2026-06-12 | none (see unit note) |
| lobby_connect | EXAMINED 2026-06-12 | none (see unit note) |
| character_create | EXAMINED 2026-06-12 | none (see unit note) |
| cc_alias | EXAMINED 2026-06-12 | none (see unit note) |
| cc_select_agency | EXAMINED 2026-06-12 | none (see unit note) |
| lobby_screen | EXAMINED 2026-06-12 | none (see unit note) |
| create_game | EXAMINED 2026-06-12 | none (see unit note) |
| game_staging | EXAMINED 2026-06-12 | none (see unit note) |
| tech_select | EXAMINED 2026-06-12 | none (see unit note) |
| mission_summary | EXAMINED 2026-06-12 | none (see unit note) |
| message_modal / password_modal | EXAMINED 2026-06-12 | U-7 |
| ingame: hud_base / top_ticker / status_lines | EXAMINED 2026-06-12 | none (see unit note) |
| ingame: chat (open + history) / messages | EXAMINED 2026-06-12 | none (see unit note) |
| ingame: player_list / system_camera | EXAMINED 2026-06-12 | none (see unit note) |
| ingame: tech_overlay / buy_tech / secret_overlay | UNEXAMINED | |
| ingame: quit_prompt | UNEXAMINED | |

## Findings

### U-1: position-dependent glyph striping from origin's whole-frame magnify
- **Unit:** [systemic] whole-frame magnify: glyph striping
- **Class:** ARTIFACT
- **Severity:** med
- **Golden shows:** Same letter renders with different pixel patterns — and
  different overall size — depending on absolute screen position. Measured in
  `tests/cli-agent/e2e/golden/mainmenu.png`: 'o' in "Tutorial" (x1224,y376) is
  21×23 px with column-dup runs [3,2,2,7,2,2,3]; 'o' in "Options" (x1263,y678)
  is 20×22 px with runs [2,2,2,7,2,3,2]. The two ADJACENT 'b's in "Lobby"
  (x1463 vs x1488) carry shifted stripe phase [2,2,3,2,2,2,3,2] vs
  [2,2,2,3,2,2,2,3]. The three 'o's inside the single string "Connect To
  Lobby" each stripe differently (float pen drifts x-phase within a string).
  Evidence: `docs/plans/uplift-evidence/systemic-glyph-striping/`
  (o_tutorial_vs_options.png, t_tutorial_vs_options.png,
  bb_lobby_adjacent.png, lobby_word_4x.png).
- **Origin cause:** `origin/main:clients/silencer/src/render/clay_ui_compositor.cpp`
  Render() ~L1185-1231 — menus composite at virtual res (853×480 @1080p), then
  whole-frame nearest magnify by s=2.25 with sx=int(dx/s), sy=int(dy/s); each
  virtual px becomes 2 or 3 device px in a {3,2,2,2} cycle phased by absolute
  frame position. Nothing per-glyph is authored.
- **Inferred intent:** "Draw this bitmap glyph at UI scale s" — every instance
  of a glyph identical, evenly distributed pixels. (Seed candidate, confirmed
  by user in the audit brief.)
- **Elevated target:** Canonical-phase glyph rendering: each glyph baked once
  at fixed resample phase (src=int(lx/s), lx from 0), drawn at integer device
  pen positions with uniform advance (round(11·2.25)=25 device px). Keeps the
  2.25× nearest retro chunkiness; removes positional unevenness. Replaces
  `glyph_fonts.{h,cpp}` string_variant (memo (face,color,string,X%18,Y%18)) +
  the render_text_glyphs executor intercept; delete the per-position variant
  machinery and the PARITY.md pen nudges it obsoletes.
- **As implemented (2026-06-12):** canonical INTEGER DEVICE CELLS — every
  glyph draws into floor(pen + i·adv)-positioned rects of size
  round(gw·gscale) × round(atlas_h·gscale); the SW renderer's nearest scaling
  of identical src/dst dims yields byte-identical pixels for every instance of
  a letter (verified: all repeated letters in mainmenu "Connect To Lobby" are
  byte-equal; before, same letters differed in size AND stripes). Two spec
  refinements, both evidence-driven:
  (1) the pen keeps the FRACTIONAL design metric (11·gscale = 24.75 @1080p)
  and snaps per glyph (24/25 rhythm — origin's own integer-virtual-pen rhythm)
  instead of a uniform 25: the uniform advance grew every string ~1%,
  re-wrapped cc_select_agency's description and clipped its final words out of
  the fixed panel (text metric = layout intent; evidence in session log);
  (2) floor (not round) quantization matches the legacy SW dst-rect floor, so
  1:1-scale in-game text (640×480, integer native advance) renders EXACTLY as
  before — zero in-game golden churn (72/76 green vs pristine origin goldens,
  ingame_tech_overlay still 0.0000).
  string_variant machinery + CPU atlas copies + legacy_w/h plumbing deleted
  (glyph_fonts, draw_executor, pipeline_host, game_ui_pipeline). Authoring pen
  nudges kept: they are plain layout offsets (also sprite-shared); their
  consolidation stays a PARITY.md refactor item.
- **Parity blast radius:** Systemic — every golden containing menu text. As
  landed: 13 origin menu/lobby goldens + mission_summary + hover_mainmenu_oval
  + 3 cppx-only baselines (gallery, modals) superseded, diff-attributed to
  text-line bands only (chrome/sprites/backdrops byte-stable); in-game goldens
  untouched. ORIGIN_GOLDENS.md "Uplifted goldens" + PARITY.md UPLIFTED rows.
- **Effort:** M
- **Ticket:** SIL-190
- **Status:** IN REVIEW (SIL-190)

### U-2: position-dependent sprite striping from origin's whole-frame magnify
- **Unit:** [systemic] whole-frame magnify: sprite phase striping
- **Class:** ARTIFACT
- **Severity:** med
- **Golden shows:** The same chrome sprite renders with different pixel
  patterns — and different overall size — depending on absolute screen
  position. Measured in `tests/cli-agent/e2e/golden/mainmenu.png`: the four
  green button ovals are the same 435px-wide sprite assembly at ring tops
  y347/y498/y648/y799 (y mod 9 = 5/3/0/7 — four distinct phases of the
  {3,2,2,2} row-duplication cycle), yet the Options oval is 75 px tall while
  its three siblings are 74; left-cap row-run phases differ per instance
  (Tutorial [2,2,3,2,2,2,3…], Exit [2,3,2,2,2,3…], Options [3,2,2,2…]);
  Tutorial vs Exit left caps (40×74 @x1026,y347 vs @x936,y799) differ in 77
  of 615 ring-ink px (12.5%). In
  `tests/cli-agent/e2e/golden/options_display.png` the filled toggle
  half-disc (38×74 @x1245) differs between row 1 (y345) and row 2 (y464) in
  46 disc-ink px (2.0%) — visibly different edge stepping on two toggles of
  the same screen. Current render byte-matches the golden in all measured
  regions (we reproduce the artifact). Evidence:
  `docs/plans/uplift-evidence/systemic-sprite-striping/`
  (oval_left_caps_tut_exit_opt_4x.png, oval_cap_tut_vs_exit_diff_4x.png,
  toggle_disc_row1_row2_diff_4x.png).
- **Origin cause:** `origin/main:clients/silencer/src/render/clay_ui_compositor.cpp`
  Render() s>1 branch (~L1186-1233) — sprites composite 1:1 into the virtual
  scratch (853×480 @1080p), then the whole frame nearest-magnifies by s=2.25
  with sx=int(dx/s), sy=int(dy/s); each virtual px becomes 2 or 3 device px
  in a {3,2,2,2} cycle phased by absolute frame position (period 9 device px
  at s=2.25). Nothing per-sprite is authored.
- **Inferred intent:** "Draw this sprite at UI scale s" — every instance of a
  sprite identical pixels and identical size, like U-1's glyphs. Same root
  mechanism as U-1 (confirmed-by-user seed candidate), sprite arm.
- **Elevated target:** Canonical-phase sprite rendering, mirroring U-1 as
  landed: bake each registered legacy sprite ONCE at canonical phase
  (src=int(lx/s), lx from 0), draw at floor-quantized integer device cells
  sized to the canonical bake. Keeps the 2.25× nearest retro chunkiness;
  removes positional unevenness. Scope = everything
  `TextureRegistry::resolve_legacy` serves (Cell + NineSlice + Contain +
  Stretch fits, `texture_registry.{h,cpp}`); backdrops (bake_backdrop_rgba)
  are the separate double-scaling unit. Deletes the (X%18, Y%18) positional
  memo key (variants collapse to one per sprite — texture-cap pressure from
  per-phase accumulation disappears) and obsoletes the per-screen ≤1-logical-px
  authoring-nudge contract recorded in PARITY.md "[systemic] Per-phase
  legacy-sprite variants" (nudges existed only so vx=round(floor(dev_x)/s)
  recovers the golden cell). Apply U-1's landed lessons: element boxes keep
  their design-metric layout (no size growth) and floor (not round) dst
  quantization so 1:1-scale in-game draws are untouched.
- **As implemented (2026-06-12):** canonical bakes per fit
  (bake_canonical_{cell,stretch,nineslice,contain}_rgba, sprite_bake.cpp):
  hop-1 composite into the box-local virtual buffer (origin int arithmetic,
  bx=by=0), then shared NEAREST magnify from phase 0 (src=int(t/s), footprint
  ceil(v*s) so every virtual px keeps its full duplication band). Variants
  memoize on (base_id, s[, vw, vh]) — scale in the key fixes the latent
  stale-scale-after-resize bug the old key had (U-1's a2ea50a7 lesson); the
  (X%18, Y%18) positional key is gone. Draw position floor-quantized; at s=1
  the bake is the identity so in-game draws are byte-identical by
  construction (suites 72/76 PASS against pristine origin in-game goldens).
  Deleted with no shims: per-position memo key, bake_element_rgba,
  bake_element_nineslice_rgba, bake_element_contain_rgba,
  PipelineHost::bake_element_sprite + the options_controls device-footprint
  snap-outward contract (chrome_controls converted to the standard
  register_legacy Stretch path; Panel keeps its absolute logical rect).
  Authoring nudges kept as plain layout offsets (U-1 precedent). Verified
  with evidence (docs/plans/uplift-evidence/U-2/): mainmenu md-oval left caps
  Tutorial/Options/Exit — BEFORE sizes 74/75/74 + 36-41 differing ink px,
  AFTER all 75 with 0 ink-mask and 0 ink-value diffs; options_display toggle
  discs rows 1/2 — BEFORE 46 differing px, AFTER 0; diff overlays show
  changes confined to legacy chrome sprites (text + backdrops byte-stable).
  15 BEFORE captures byte-matched the goldens before the change.
- **Parity blast radius:** Systemic — every golden with legacy chrome sprites
  at scale >1: the 13 menu/lobby goldens + hover_mainmenu_oval +
  mission_summary + gallery (sprite bands: ovals, toggles, logo, nine-slice
  buttons, emblems, plates). In-game goldens unaffected (s=1 path has no
  magnify striping). Same supersession protocol as U-1.
- **Effort:** M
- **Ticket:** SIL-204
- **Status:** IN REVIEW (SIL-204)

### U-3: backdrop double-scaling banding from origin's two-hop menu compositing
- **Unit:** [systemic] background image double-scaling / banding
- **Class:** ARTIFACT
- **Severity:** med
- **Golden shows:** Full-bleed menu backdrops carry an irregular {2,2,5}
  duplication-run pattern on BOTH axes — each source px becomes 2, 2, then 5
  device px instead of a uniform 3, because the image is nearest-resampled
  twice. Measured in `tests/cli-agent/e2e/golden/mainmenu.png`: (a) the Mars
  planet's authored regular scanline rows render as 2/2/5-px-thick bands —
  row runs through the limb (x1570-1880, y260-560) = [5,2,2,5,2,2,…] avg 2.97;
  (b) the same authored 1-src-px star (rgb 32,8,8) renders 2×2 @(556,653),
  5×5 @(675,684), 2×5 @(556,531), 5×2 @(657,986) — 6.25× area variance by
  position alone; star blob width histogram {2,4,5,7,9,11} = exactly the
  two-hop run sums (uniform 3× ⇒ {3,6,9}); (c) stretch backdrops
  (lobby_screen, options_controls goldens) show the same {2,2,5} column
  banding horizontally + the standard {3,2,2,2} 2.25× stripe vertically;
  (d) the backdrop fills only 1919/1920 columns (scaled_w=int(853·2.25+0.5))
  — golden col x1919 fully black, x1918 has 540 backdrop px (mainmenu).
  Current render reproduces it byte-for-byte (backdrop strips diff 0 vs
  golden; pre-U-1/U-2 renders confirm backdrop bytes unchanged by those
  fixes). Evidence: `docs/plans/uplift-evidence/systemic-backdrop-banding/`
  (planet_banding_before_vs_mock_3x.png — golden vs simulated single-hop
  elevated target; same_star_four_sizes_10x_brightened.png).
- **Origin cause:** two compounding nearest resamples in
  `origin/main:clients/silencer/src/render/clay_ui_compositor.cpp`: hop 1
  DrawImage (~L505-552) cover/stretch-blits the 640×480 backdrop into the
  853×480 virtual scratch at ×1.333 ({1,1,2} runs); hop 2 Render() s>1
  branch (~L1206-1233) whole-frame magnifies ×2.25 ({3,2,2,2} runs). The
  patterns multiply: avg 3.0 but distributed {2,2,5}. At 1080p the
  end-to-end ratio is exactly 3.0 — a single resample would be perfectly
  uniform.
- **Inferred intent:** "Fill the screen with this backdrop image" (cover for
  menus, stretch for Options·Controls + lobby). The source art's regular
  scanline texture and uniform stars make even scaling the unambiguous
  intent. Third arm of the user-confirmed whole-frame-magnify seed candidate
  (U-1 glyphs, U-2 sprites).
- **Elevated target:** single-hop nearest resample sprite→device geometry in
  `bake_backdrop_rgba` (sprite_bake.cpp): compute cover/stretch fit at
  device res (cover @1080p = uniform 3×3 px blocks, centered crop; stretch
  keeps the unavoidable single-hop {2,2,2,3} vertical at 2.25 — same
  residual U-1/U-2 accept), sample src=int(dst/scale) once, fill all 1920
  columns. Delete the two-hop emulation arithmetic; call sites + 1:1
  full-bleed draw unchanged; s=1 already identity so in-game untouched by
  construction.
- **As implemented (2026-06-12):** bake_backdrop_rgba (sprite_bake.cpp) is now
  a single NEAREST resample sprite→device: cover = max(dw/sw, dh/sh) float
  scale, int(+0.5) draw size, int-centered crop, src=int((d-o)/scale); stretch
  = per-axis dw/sw, dh/sh. Two-hop virtual-canvas arithmetic deleted, and with
  it the legacy_w/h plumbing through PipelineHost::bake_backdrop_sprite +
  game_ui_pipeline (the fit needs only sprite + device dims; no shims).
  dw==sw && dh==sh is the identity, so 1:1 draws are untouched by
  construction. Verified with evidence (docs/plans/uplift-evidence/U-3/):
  15 BEFORE captures byte-matched the goldens; AFTER @1080p — planet limb
  row runs all exactly 3 (before [5,2,2,…]), star(32,8,8) blob histogram
  collapses to 3×3 (1013 blobs) + 3-multiples for adjacent clusters (before
  2×2/5×5/2×5/5×2 mix), col x1919 filled (540 backdrop px = x1918's count).
  Diff attribution: mainmenu changed px 99.83% strict src-equivalent to the
  same source texels via the before-render (1148 residual all at UI-element
  edges = occluded-texel candidates), lobby_screen 99.2% (residual = the
  panel-border blur strip @x1100-1199 blending the changed backdrop),
  hover_mainmenu_oval changed px byte-equal mainmenu's at every coordinate,
  magenta overlays on 7 screens + modals/gallery show every changed px on
  backdrop (chrome/text/layout byte-stable). lobby_connect byte-unchanged
  (no visible backdrop).
- **Parity blast radius:** As landed — 14 menu/lobby goldens superseded
  (mainmenu, options×4, character_create, cc_alias, cc_select_agency,
  lobby_screen, create_game, game_staging, tech_select, mission_summary,
  hover_mainmenu_oval; lobby_connect byte-unchanged) + modals/gallery
  re-blessed. Suites 70/71/74/75 PASS against the superseded goldens;
  in-game 72/76 PASS against pristine origin goldens (no backdrop at s=1).
  ORIGIN_GOLDENS.md "Uplifted goldens (U-3)" + PARITY.md UPLIFTED rows +
  systemic section updated.
- **Effort:** S
- **Ticket:** SIL-205
- **Status:** IN REVIEW (SIL-205)

### Unit note — [systemic] float-rect flooring (2026-06-12)

Examined the three flooring/rounding breadcrumbs PARITY.md recorded
(cc_select_agency oval right-cap clip + right-panel 1px clip, create_game
scrollbar thumb rounding) against the PRISTINE origin goldens
(`git show ba345131:tests/cli-agent/e2e/golden/...` — raw origin captures,
pre-supersession). Outcome inverts the audit brief's assumption: **origin's
pixels are correct in all three spots** — its int-virtual-grid pipeline
(unscaled sprite blits at int coords, integer thumb arithmetic) doesn't
produce these defects. The clips/gaps are OURS (fractional logical boxes +
floor quantization + nearest stretch decimation), passed sub-gate by the old
pixdiff metric and then enshrined as the target by the U-1/2/3 golden
supersessions → findings U-4, U-5 (DEFECT, port-side). The remaining
pristine-vs-current ±1px deltas in those regions (cc right-panel border
strokes 2↔3px, scrollbar rail columns, corner pieces) are stroke-phase
shifts attributable to U-2's canonical-bake divergence (documented
supersession), not seams — no ticket. Origin-side positional jitter that
DOES exist (mainmenu oval pitch 151/150/151, 24/25 pen rhythm) is the
accepted non-integer-scale residual U-1/U-2 already adjudicated. No
origin-side flooring candidate found; recording zero rather than padding.

### U-4: cc_select_agency agency ovals — right cap sheared, ring broken (port defect enshrined by supersession)
- **Unit:** [systemic] float-rect flooring: 1px seams & jitter
- **Class:** DEFECT (ours — origin renders it correctly)
- **Severity:** med
- **Golden shows:** All five agency-row ovals in the CURRENT
  `tests/cli-agent/e2e/golden/cc_select_agency.png` have a flat-sheared right
  cap with the ring BROKEN at the oval's vertical middle. Noxis oval
  (y211-271): right ink stops at x901 and rows y236-246 have no ring ink at
  all (dark fill bleeds out the gap); left cap complete to x373. Symmetry
  (top edge x394-883, center 638.5) puts the intended right extreme at x904.
  Assembly 529 device px wide vs correct 531. The PRISTINE origin golden
  (ba345131) renders it full + symmetric: 374→904 every row, ring closed.
  character_create's roster row (same 236×27 row_plate sprite) is full-width
  531 with closed ring in the CURRENT golden — the break is agency-step-only.
  HEAD render == golden re-verified this session (suite 71 PASS). Evidence:
  `docs/plans/uplift-evidence/systemic-float-rect-flooring/`
  (cc_agency_oval_rightcap_origin_vs_current_6x.png,
  cc_oval_leftcap_mirrored_vs_rightcap_6x.png).
- **Origin cause:** none — origin blits the full unscaled 236×27 sprite at
  int coords (`DispatchButtonSprite`,
  `origin/main:clients/silencer/src/render/clay_ui_compositor.cpp` ~L602-619;
  box = sprite width: kLeftColumnW=236, `character_create_layout.cpp:59,509`).
- **Port cause:** agency-rows wrapper `.margin = {-1.5f, 3.5f, -3.0f, 3.0f}`
  (`client/ui/screens/character_create.cppx:377`, a label-pen parity nudge)
  nets the grow-to-pane List rows ~2 logical px narrower than the roster
  step's; row_plate is `LegacyFit::Stretch` (`game_ui_pipeline.cpp:285`), so
  the stretch bake nearest-decimates 236 src cols into ~235 virtual and drops
  the cap's outermost ring column. Predates U-2 (present in the 2026-06-11
  pre-uplift render; flagged sub-gate by the parity critic: PARITY.md
  cc_select_agency "oval right-cap 2px clip @x903-904").
- **Inferred intent:** closed symmetric stadium oval, identical to the roster
  rows and origin's own render. Inverse case of U-1/2/3: the golden canonizes
  OUR regression against an origin render that was already right.
- **Elevated target:** agency rows' content box back to the full 236-virtual
  width (rework the wrapper margins into non-box-shrinking offsets, keeping
  the label pen on its golden cell); stretch bake at native size → no
  decimation, ring closes, 531 device px. Supersede cc_select_agency.png,
  diff confined to the five ovals' right-cap region.
- **As implemented (2026-06-12):** wrapper margin `{-1.5, 3.5, -3.0, 3.0}` →
  `{-1.5, 1.5, -3.0, 3.0}` (character_create.cppx) — keeps the −1.5
  one-virtual-col left shift, restores the box to the pane's full 354 logical
  = 236 virtual so resolve_legacy_sized computes vw=236 (was 235) and the
  Stretch bake is 1:1. Verified with evidence
  (docs/plans/uplift-evidence/U-4/): BEFORE capture byte-matched the golden;
  AFTER — every oval 531 px (373→903, the roster rows' exact cell), mid-row
  bright ring ink restored (was NO ink y238-244), AFTER right-cap strip
  byte-identical to character_create's roster oval (canonical bakes are
  position-independent per U-2; 13 residual px in the left strip = corner
  transparency over different backdrop texels — explained); BEFORE→AFTER diff
  = 3385 px in exactly five bands matching the five oval plates
  (y229-253/301-325/373-397/445-469/517-541, x ≤ 903), labels/text
  byte-stable. Note: cap lands at 373→903 not origin's 374→904 — same 531px
  width, 1px positional phase = the documented U-2 supersession residual
  (consistent with the roster step), not a seam. Suites 70/71/72/74/75/76 +
  17 all PASS; golden superseded + ORIGIN_GOLDENS.md U-4 section + PARITY.md
  row updated (polish note resolved).
- **Parity blast radius:** single screen — cc_select_agency.png (suite 71).
  In-game/hover untouched. Resolves the PARITY.md polish note.
- **Effort:** S
- **Ticket:** SIL-206
- **Status:** IN REVIEW (SIL-206)

### U-5: create_game scrollbar thumb 1px short of track (right + bottom)
- **Unit:** [systemic] float-rect flooring: 1px seams & jitter
- **Class:** DEFECT (ours — origin renders it correctly)
- **Severity:** low
- **Golden shows:** CURRENT `tests/cli-agent/e2e/golden/create_game.png`:
  Game Options scrollbar thumb fill spans x1157-1168 (160 rows/col), leaving
  a 1px black gutter at x1169 before the track right edge (x1170-1172) while
  sitting flush against the left edge (x1155-56); fill 1 row shorter than
  origin's. PRISTINE origin golden (ba345131): fill flush x1157-1169, 161
  rows, no gutter. HEAD render == golden (suite 71 PASS this session).
  Evidence: `docs/plans/uplift-evidence/systemic-float-rect-flooring/
  cg_scrollbar_thumb_origin_vs_current_5x.png`.
- **Origin cause:** none — origin's custom scrollbar fill is integer
  arithmetic on the virtual grid
  (`origin/main:clients/silencer/src/render/clay_ui_compositor.cpp`
  ~L955-1020), flush by construction.
- **Port cause:** our inline decorative scrollbar (GameCreatePanel) computes
  the thumb rect from fractional logical coords; floor of left + floor of
  width drops the last device column/row on the right/bottom only. Flagged
  sub-gate by the parity critic (PARITY.md create_game "scrollbar thumb 1px
  rounding"); enshrined by the golden supersessions.
- **Inferred intent:** thumb fill flush to the track interior on all sides.
- **Elevated target:** compute the thumb's right/bottom as floored track
  edges (floor(right) − floor(left)) so the device extent fills the interior
  (to x1169, 161 rows). Supersede create_game.png, diff confined to the
  thumb-fill rect.
- **As implemented (2026-06-12):** fresh measurement sharpened the diagnosis:
  the whole 13×157 fill rect sat HALF A VIRTUAL CELL up-left (device
  x1156-1168 / y211-367 vs origin x1157-1169 / y212-368) — a 1px gutter
  against the track's right stroke at x1169 AND a 1px overpaint of the left
  stroke's inner column (x1156) and top stroke's inner row (y211), not a
  width/height shortfall. Root mechanism: the thumb is a flex Box whose solid
  fill rasterized as a raw geometry quad at fractional logical edges, while
  the surrounding track strokes snap to origin's virtual cell grid
  (snap_legacy_hairline_border) — the one legacy primitive left unsnapped
  (borders, sprites, glyphs already snap). Fix is the consistent completion,
  in the executor not the screen: `snap_legacy_solid_fill`
  (draw_executor.cpp) snaps eligible solid Rect fills (square corners, the
  two legacy stroke palette colors — shared `legacy_stroke` helper — at
  quarter-integer virtual scale, shared `legacy_virtual_scale` gate) onto the
  same cell grid, so fills sit flush against snapped strokes by construction
  (robust to sub-cell layout drift; no authoring nudge). The thumb is the
  only legacy-green solid fill in the tree today (grep: one
  `background(tokens::kChromeStroke)` user). Verified with evidence
  (docs/plans/uplift-evidence/U-5/): 8 BEFORE lobby-cluster captures
  byte-matched the goldens; AFTER thumb = fill x1157-1169 rows y212-368 with
  both track strokes fully restored; BEFORE→AFTER diff 338 px, bbox exactly
  the thumb region; AFTER-vs-pristine-origin residual set in the scrollbar
  region is EXACTLY BEFORE's (the documented U-3 backdrop texels) — the
  thumb itself byte-equal origin; all other captured screens byte-unchanged.
  Suites 70/71/72/74/75/76 + 12/17/31/53 PASS (in-game untouched; sv-gate +
  no legacy-green fills at s=1). create_game.png superseded;
  ORIGIN_GOLDENS.md U-5 section + PARITY.md row updated (polish note
  resolved).
- **Parity blast radius:** single screen — create_game.png (suite 71).
  Resolves the PARITY.md polish note.
- **Effort:** S
- **Ticket:** SIL-207
- **Status:** IN REVIEW (SIL-207)

### Unit note — [systemic] palette quantization & dim formulas (2026-06-12)

Replicated origin's full palette pipeline in analysis tooling
(`PALETTE.BIN` 772-byte-stride pages + `<<2` expansion, verified against
known golden colors page1[140]=yellow / page2[140]=sage; `Palette::Brightness`
linear-scale/blend-to-white + `ClosestMatch` LUTs; `Color` luma-offset;
page-0 ramp arithmetic) and probed every dim/brighten combo the UI actually
uses. Everything except U-6 came back CLEAN — recording so per-screen
iterations don't re-litigate:

- Menu pages (1=menus, 2=lobby cluster) always take the
  ClosestMatch-quantized path (`palette.cpp` Calculate: the ramp-arithmetic
  dim is page-0-only). Computed full LUTs for the used intensities — 64
  (tech dim), 96 (Muted), 136 (hover max = 128+phase·2, button.cpp
  FrameForPhase), 160 (presence) — zero entries with err>40 or hue-shift>40°
  on any index the menus actually render (the only outliers are unused
  yellow/red entries: page1@96 idx138-143, page2@160 idx155/161).
- Hover brightening measured in the goldens directly:
  hover_mainmenu_oval vs mainmenu diff = 31,471 px, 23 distinct color
  substitutions, ALL on-ramp green→brighter-green (plus gray→gray). No
  off-ramp speckle, no collisions.
- Page-0 in-game dim ramp arithmetic: all seven ramps in the dimmable range
  (idx 2-113) are luminance-monotone; the one ramp not anchored at black
  (idx 18-33, starts lum 75) is exactly the `colorramp==16` exception origin
  already special-cases to ClosestMatch. Handled, not an accident.
- PARITY-confirmed palette semantics (caret = per-page idx140, presence
  sage, version amber, tech dim formula) — INTENT, matched to origin's
  palette logic during the parity grind; nothing new found against them.
- ClosestMatch quirks (start=2, page-0 upperonly≥114) and the alpha-LUT
  0.5→1 step are in-game-only mechanisms with no golden-visible defect to
  point to; no candidate recorded.

### U-6: 6-bit palette expansion by plain `<<2` caps every channel at 252 — white is never white
- **Unit:** [systemic] palette quantization & dim formulas
- **Class:** ARTIFACT
- **Severity:** low
- **Golden shows:** No palette-derived pixel in any golden exceeds channel
  value 252. Full-extrema sweep of all 30 goldens this session: 12 saturate
  at exactly 252, none above (gallery.png's lone 255-green is a cppx-only
  test color). Concrete instances: cc_alias caret = rgb(252,252,0), 48 px
  @x718-719, y491-514 ("full yellow"); ingame_hud_base brightest text =
  rgb(252,252,252) ("full white" at 98.8%); every in-game golden's whites
  likewise. Evidence:
  `docs/plans/uplift-evidence/systemic-palette-quantization/`
  (cc_alias_caret_252_vs_proper_expansion_12x.png,
  ingame_hud_white_252_vs_proper_expansion_8x.png,
  mainmenu_expansion_delta_map_amplified85x.png).
- **Origin cause:** `origin/main:clients/silencer/src/render/palette.cpp`
  Load() ~L44-52 — PALETTE.BIN stores VGA 6-bit DAC values (0..63);
  expansion is a plain `r << 2` (63→252). Our port replicates it verbatim
  (`clients/silencer/src/render/palette.cpp:50-52`); all cppx UI color
  sourcing flows through the same table (game_ui_pipeline.cpp page_color +
  sprite/glyph bakes), so the artifact is in every golden.
- **Inferred intent:** 6-bit 63 = maximum DAC intensity = full brightness on
  original hardware. The palette author authored pure white/yellow; 252 is
  the standard sloppy 6→8-bit expansion losing the top 3 intensity levels.
- **Elevated target:** canonical bit-replicating expansion
  `(v << 2) | (v >> 4)` in Palette::Load (one line ×3 channels); per-pixel
  effect on existing colors is exactly `c | (c >> 6)` (+0..3/channel,
  saturated → true 255). Delete cached PALETTECALC*.BIN so the ClosestMatch
  LUTs recompute from corrected colors.
- **Parity blast radius:** systemic, the widest yet — EVERY golden
  (all 30, including the so-far-pristine in-game set) shifts by ≤3/channel.
  Diff trivially attributable (pure recolor, geometry byte-stable), but
  honest note: max delta 3/255, mainmenu only 5.4% of px change at all (by
  1-2) — at/below perception threshold; value is correctness of saturated
  colors. Reasonable for the user to reject on golden-churn grounds.
- **Effort:** S
- **Ticket:** SIL-208
- **Status:** TICKETED — judged NOT implementable now (2026-06-12 iteration):
  benefit is sub-perceptual (max delta 3/255) while the blast radius is the
  widest possible — it would supersede ALL 30 goldens including the pristine
  in-game set that U-1..U-5 deliberately kept byte-identical to raw origin
  captures (that set is the strongest remaining verification anchor; in-game
  suites still gate against it). Churn > benefit; the auditing iteration
  itself predicted likely user rejection. Left for the user to decide.

### U-7: message/password modals use raw sprite size instead of origin fixed dialog boxes
- **Unit:** message_modal / password_modal
- **Class:** DEFECT
- **Severity:** med
- **Golden shows:** The current cppx-only baselines render both modal panels
  at the raw registered bank-40 sprite box instead of the fixed origin modal
  element. Fresh captures from this audit byte-match the checked-in baselines
  (`cmp=0`, `pixdiff=0.0000`; full gate 70 PASS). In `message_modal.png`,
  inspect reports the panel at logical `(480,316) 320x88` → device
  `(720,474) 480x132`, while the sample message text spans device
  `(663,486) 594x33`, visibly spilling past both sides of the frame. In
  `password_modal.png`, inspect reports the panel at logical
  `(498,306) 284x108` → device `(747,459) 426x162`, while the prompt spans
  device `(699,466) 522x33`, again outside the panel. Evidence:
  `docs/plans/uplift-evidence/message_modal_password_modal/`
  (`message_modal_current_yellow_vs_origin_cyan_2x.png`,
  `password_modal_current_yellow_vs_origin_cyan_2x.png`,
  `modal_size_overflow_before_overlay_2x.png`; yellow=current raw-sprite box,
  cyan=origin fixed dialog rect).
- **Origin cause:** none — origin sizes the modal element explicitly,
  independent of raw sprite dimensions:
  `origin/main:clients/silencer/src/client/ui/modals/message_modal.cpp`
  lines 26-29 and 82-93 use `kDialogW=352`, `kDialogH=178`,
  `kDialogPadX=34`, `kDialogPadY=44`, `.image = PackImage(40,4)`;
  `origin/main:clients/silencer/src/client/ui/modals/password_modal.cpp`
  lines 27-30 and 101-123 use `kDialogW=352`, `kDialogH=148`,
  `kInputW=180`, `kInputH=14`, `.image = PackImage(40,2)`. Origin text
  defaults to word wrapping (`clients/silencer/src/ui/primitives/text.h`
  lines 89-94; `text.cpp` lines 195-205), and origin `DispatchImage` samples
  sprites into the Clay bounding box (`render/clay_ui_compositor.cpp` lines
  495-549).
- **Port cause:** the retained cppx screens override those origin constants
  with raw registered sprite dimensions whenever `use_chrome()` has the
  texture: `message_modal.cppx:42-45` uses `chrome.dialog_msg_w/h`, and
  `password_modal.cppx:69-72` uses `chrome.dialog_pw_w/h`. `ScreenSubtitle`
  also paints through the retained text component with `TextVisual.wrap`
  defaulting to `None`, so the smaller raw boxes expose the overflow in the
  blessed cppx-only baselines.
- **Inferred intent:** centered modal dialog with the fixed origin modal
  footprint and internal word-wrapped prompt/message content, not raw sprite
  dimensions accidentally controlling the whole dialog layout.
- **Elevated target:** restore the origin modal layout contract: fixed
  logical `352x178` message and `352x148` password dialog containers; render
  the bank-40 dialog art using origin `PackImage` cover semantics into those
  boxes; constrain/wrap the message/prompt text to the padded inner width;
  keep the password input `180x14`, OK Chrome button, modal focus trap, and
  existing control IDs. Supersede `message_modal.png` and
  `password_modal.png` after before/after attribution.
- **As implemented (2026-06-12):** message/password modals no longer size
  themselves from `chrome.dialog_*_w/h`; they use the origin fixed containers
  (`352x178` message, `352x148` password), cover-fit the bank-40 dialog art
  (`tokens::image_patch_cover` = origin `PackImage` semantics), use fixed
  Chrome OK plates, and restore the password field to `180x14`. Message text
  renders as a wrapped Large-face text box at the padded inner width
  (`284` logical), so the visual-regression sample now stays inside the
  dialog. Password prompt keeps word-wrap enabled but uses the fixed dialog
  width with centered text: origin metrics keep this specific prompt on one
  line, while forcing it through the retained font's `284`-logical inner
  width wrapped it into the input well (rejected during after-crop review).
  Verified with evidence (`docs/plans/uplift-evidence/U-7/`): BEFORE captures
  byte-matched the prior baselines; AFTER inspect reports message text
  `498,312 284x45`, message OK `523,376 234x31`, password prompt
  `464,316 352x23`, input `550,355 180x14`, password OK
  `523,385 234x31`; BEFORE→AFTER diff bboxes are confined to the modal
  regions (`message_modal` x663-1247/y406-672, `password_modal`
  x696-1223/y429-650). `message_modal.png` and `password_modal.png` were
  superseded; ORIGIN_GOLDENS.md + PARITY.md document U-7. Verification:
  build wrapper PASS; scenarios 13, 22, 70, 71, 72, 74, 75, and 76 PASS
  (a mistaken attempt to run nonexistent `76_visual_regression_ingame_goldens.sh`
  failed with "No such file or directory"; the actual remaining script
  `76_visual_regression_ingame_extra.sh` passed).
- **Parity blast radius:** two cppx-only regression baselines:
  `message_modal.png` and `password_modal.png`; possibly `update_screen` if
  it shares `dialog_msg_w/h` and the same fixed-origin dimensions. No
  origin-restored menu/lobby/in-game goldens should change unless the shared
  dialog helper is over-broadened.
- **Effort:** M
- **Ticket:** SIL-51
- **Status:** IN REVIEW (SIL-51)

### Unit note — mainmenu (2026-06-12)

First per-screen audit. Examined the current golden
(`tests/cli-agent/e2e/golden/mainmenu.png`, post U-1/2/3 supersession)
against the pristine origin capture (`git show ba345131:...`) and origin
source (`origin/main:clients/silencer/src/client/ui/screens/main_menu/
main_menu_screen.cpp` + `components/silencer_logo.cpp`). Suite 70 re-run
this session: PASS (fresh render byte-matches the golden; the stale
`/tmp/cppx_renders/mainmenu.png` from Jun 11 equals the PRISTINE golden —
670,909 px diff vs current is exactly the documented U-1/2/3 supersession,
don't be fooled by it). **Zero candidates.** What was checked and cleared:

- **Logo sprite (bank 208, held frame 60):** no occlusion box — the dark
  vertical band behind "R"/right ring continues far above and below the
  logo stage (authored backdrop art, planet-limb shadow; evidence
  `docs/plans/uplift-evidence/mainmenu/logo_dark_band_authored_backdrop_2x.png`);
  backdrop-red px present in every column of the logo band (no transparency
  defect). Tick marks between S/1 and 3/R, ring terminals, and the
  horizontal wire are all present in the pristine origin golden — authored
  animation art (logo_left/right_pristine_vs_current_4x.png). Ink bbox
  pristine 772×66 @(257,513) vs current 771×65 @(258,514) — 1px stripe-phase
  shift, the documented U-2 canonical-bake residual; no size growth, no
  clipping (stage = frames-29..60 union bounds, frame ink well inside).
- **Oval assemblies (U-4 lens):** Tutorial oval ring closed on every row
  (no no-left/no-right holes, no empty columns), top/bottom edges flat at a
  single device row across all 355 middle columns — no cap/middle seam;
  the only edge jumps (7 device px at x1029/1035/1452/1458) are the cap
  curvature steps of the authored art at 2.25×. Post-U-2 the four ovals are
  byte-identical assemblies, so one oval proves all four.
- **Layout arithmetic vs intent:** stagger spacers 40/80/40/0 virtual land
  the left caps at x1026/1112(Lobby)/1026/936 — exactly 90 device px per
  40-virtual step; version footer ink top-left at (22,1042) device =
  origin's authored (kVersionFooterX=10, 480−17=463) cell exactly.
- **Already-adjudicated, not re-litigated:** oval pitch 151/150/151 + 24/25
  glyph-pen rhythm (U-1/U-2 non-integer-scale residual), label centering
  bearing asymmetry (INTENT, siblings unit note), hover ramp (palette unit
  note), backdrop banding/stars (U-3, fixed), slashed capital 'O' in
  "Options" (font's authored glyph). PARITY.md's mainmenu nudges
  (per-phase variants + string bake) were deleted by U-1/U-2 — none remain.

### Unit note — [systemic] spacing/alignment consistency across siblings (2026-06-12)

Swept every sibling set with ≥3 members across the goldens (vertical pitch,
horizontal column alignment, and label-centering lenses), cross-checking
suspicious pitches against origin/main layout source and, where the U-1/2/3
supersessions could mask things, against the pristine origin goldens
(`git show ba345131:...`). Outcome: **zero candidates** — every sibling set
origin computes identically IS uniform; recording the evidence so per-screen
iterations don't re-measure:

- **options** ovals: device tops 327/444/561/678 — pitch 117 exact
  (= 52 virtual), all four 75 px tall (post-U-2). Uniform.
- **options_display**: both rows pitch 118; toggle stacks aligned at the
  same x (1245) on both rows. Uniform.
- **options_controls** keybind rows: value-oval tops 310/454/598/742 —
  pitch 144,144,144. The preset→rows gap differs (126) but is the authored
  section boundary (kSectionGap=8 + viewport start; source kRowH=43,
  kRowGap=10, rows GROW — `controls_keybind_list.cpp:28-30`). Uniform.
- **create_game**: map-list rows alternate 31/32 device = uniform virtual
  pitch 14; Game Options rows pristine pitches {38,38,39,38} = uniform
  virtual 17 (current 39/38/39/37 is the documented U-1 floor-quantization
  ±1 residual — same virtual layout, verified label tops byte-aligned at
  x727 all five rows); character stat rows alternate 31/32 = uniform 14.
- **tech_select**: rows fixed `kRowH=13`, no gap, checkbox cell 13×13
  (`tech_tree_grid.cpp:32-38,240`); measured pitches avg 29.25 (=13×2.25),
  apparent 28/30 wobble is threshold noise from dim (unavailable-tech) rows
  over the busy backdrop, not layout.
- **mission_summary**: stat lines `kLineH=11` uniform; upgrade column
  `kLevelStartY + index*kLevelRowGap` (46) — pure index arithmetic
  (`mission_summary_screen.cpp:55,66,202,226`). Screen is byte-identical
  parity (verbatim origin grid port). Uniform by construction.
- **game_staging** button stack: wrapper padTops 3/11/39
  (`game_join_panel.cpp:37-39`) — the larger Ready gap is authored
  hierarchy (INTENT); roster rows are `team*55 + slot*13` index arithmetic.
- **mainmenu**: stagger step 40 authored (`main_menu_screen.cpp` 
  kActionStaggerStep); oval pitch 151/150/151 and the 24/25 glyph-pen
  rhythm are the adjudicated U-1/U-2 non-integer-scale residuals.
- **Label centering in ovals** (mainmenu + options, 8 buttons): ink-bbox
  asymmetry dH spreads 0..+10 device but tracks the LAST glyph's right
  bearing in the monospace 11-advance cell ('l'/'y' full-cell → ~0;
  's'/'t' → +8); vertical dV is −5 without descenders, −1 with ('p','y')
  — i.e. the full line box IS centered. INTENT, not mis-centering. Same
  bearing effect explains the 2px 'M'-vs-digit variance in create_game's
  value column (1007 vs 1005 = sub-virtual-pixel).
- **In-game overlays** (s=1, byte-identical parity): no set has ≥3 visible
  siblings in the goldens (buy_tech 2 rows, player_list 1 row, messages 2
  lines); buy-tech row pitch 29 vs origin's grid — nothing measurable
  beyond parity already proves.

The PARITY.md "nudge" breadcrumbs in this area (display fullscreenw mr 3 +
indDx 1, controls titlewrap inset-top 13) are sub-device-pixel pen-recovery
glue from the U-1 string-bake era — port-side authoring offsets, not
evidence of origin spacing accidents; their consolidation remains a
PARITY.md refactor item, not an uplift.

### Unit note — options (2026-06-12)

Audited the root Options overlay after regenerating the menu-cluster renders
with `OUT=/tmp/cppx_renders bash tools/cap/cap_menus.sh`; the fresh
`/tmp/cppx_renders/options.png` byte-matches the current golden
(`cmp_exit=0`, pixdiff 0.0000, tolerant gate 0.0000/PASS). **Zero candidates.**
What was checked and cleared:

- **Authored shape matches origin intent:** origin `options_screen.cpp`
  resets presentation to the menu palette/camera (lines 34-37), draws only the
  full-screen bank-6 starfield (lines 77-83), and centers one 196-virtual-px
  column (lines 84-90) containing four Md oval buttons in order
  Controls/Display/Audio/Go Back (lines 91-106). Origin `button.cpp`
  resolves Md oval buttons to bank 6 idx7, fixed 196x33, Title text, yOffset 8
  (lines 110-120). The current cppx screen is the same plain centered oval
  nav menu with no title/panel/dirty controls (`options_screen.cppx:53-60`,
  buttons at :61-95) and uses the shared Overlay screen layout gap of 28.5
  logical = 19 virtual (`screen_layout.cppx:72-85`).
- **Golden pixels express that shape cleanly:** current overlay button rects
  from the live inspect tree are all `294x50` logical at x=492, y=218/296/374/452,
  i.e. device rects x=738, y=327/444/561/678, 441x75. The top pitch is exactly
  117 device px for all three gaps (= 52 virtual × 2.25). Evidence:
  `docs/plans/uplift-evidence/options/button_stack_current_3x.png`.
- **No hidden title/chrome accident:** the region above the stack is only the
  U-3-updated starfield; no stray title, slate dialog border, panel frame, or
  mainmenu content leaks through the overlay. Evidence:
  `docs/plans/uplift-evidence/options/top_left_no_title_current_3x.png`.
- **Button chrome is not clipped:** green-ink bboxes for the four overlay
  ovals are aligned at x738-1172 with y bands 327-401, 444-518, 561-635,
  678-752. The cap curvature and bottom highlight are continuous in the
  current crop; the origin-vs-current crop shows only already-adjudicated
  U-1/U-2/U-3 differences (canonical text/sprite phase and single-hop
  backdrop), not a new options-specific defect. Evidence:
  `docs/plans/uplift-evidence/options/controls_oval_current_6x.png` and
  `docs/plans/uplift-evidence/options/origin_vs_current_button_stack_2x.png`.
- **Already-adjudicated, not re-opened:** the current ovals are 75px high
  where the pristine origin capture's green-mask bbox is 74px, the expected
  U-2 canonical-sprite supersession residual at 2.25x; backdrop deltas vs
  pristine origin are the U-3 single-hop supersession; label crispness/stripe
  changes are U-1. The options `grid_nudge` wrapper remains a PARITY.md
  cleanup breadcrumb for old phase recovery, but the rendered result is
  centered on origin's virtual x=328 cell and does not create a visible
  design-intent miss.

### Unit note — options_audio (2026-06-12)

Audited after regenerating the menu-cluster renders with
`OUT=/tmp/cppx_renders_uplift_options_audio bash tools/cap/cap_menus.sh`;
the fresh `/tmp/cppx_renders_uplift_options_audio/options_audio.png`
byte-matches the current golden (`cmp_exit=0`, pixdiff `0.0000`,
tolerant gate `0.0000 ... PASS`). **Zero candidates.** What was checked
and cleared:

- **Authored shape matches origin intent:** origin `options_audio_screen.cpp`
  resets to the menu palette/camera (lines 50-51), draws only the full-screen
  bank-6 starfield on the root (lines 87-94), then uses an invisible
  layout-only `OptionsAudioPanel` fixed at 420 virtual px with 24/32 padding
  and 22px child gaps (lines 95-104). Its contents are exactly one title
  (lines 105-106), one Boolean row (lines 107-113), and Save/Cancel Md ovals
  (lines 114-128). The shared origin Boolean row is likewise fixed geometry:
  33px row height, 24px button-to-indicator gap, 10px indicator gap, and two
  20x33 indicator sprites (components/boolean_setting_row.cpp:19-23, 51-69);
  `button.cpp` resolves the Music Lg oval to bank 6 idx23 at 220x33 and the
  Save/Cancel Md ovals to bank 6 idx7 at 196x33 (lines 113-143).
- **Golden pixels express that shape cleanly:** the live inspect tree places
  the title at logical `(533,168) 215x29`, the Music row at `(419,230) 441x49`
  with the 330px Lg button at x419 and the two indicator cells at x785/x830,
  and the action row at `(336,312) 606x50` with 294x50 Save/Cancel ovals at
  x336/x648. Pixel measurement on the fresh 1920x1080 capture found the title
  green bbox `(801,252)-(1120,289)`, the Music/toggle row bbox
  `(628,345)-(1283,419)`, the action row bbox `(504,468)-(1406,542)`, and no
  green UI pixels in the empty upper band. Evidence:
  `docs/plans/uplift-evidence/options_audio/current_form_2x.png`,
  `music_toggle_row_4x.png`, and `save_cancel_row_3x.png`.
- **No screen-specific accident found:** there is intentionally no visible
  panel frame, the oval/toggle chrome is continuous, and the one-row form's
  spacing follows the same fixed origin constants used by options_display.
  The pristine-origin vs current comparison shows only the already
  adjudicated U-1/U-2/U-3 supersession families (canonical glyphs, canonical
  chrome sprites, and single-hop backdrop), not a new Audio-specific defect.
  Evidence:
  `docs/plans/uplift-evidence/options_audio/pristine_vs_current_form_150pct.png`.

### Unit note — options_display (2026-06-12)

Audited after regenerating the menu-cluster renders with
`OUT=/tmp/cppx_renders_uplift_options_display bash tools/cap/cap_menus.sh`;
the fresh `/tmp/cppx_renders_uplift_options_display/options_display.png`
byte-matches the current golden (`cmp_exit=0`, pixdiff `0.0000`,
tolerant gate `0.0000 ... PASS`). Full visual gate
`bash tests/cli-agent/e2e/70_visual_regression.sh` also passed. **Zero
candidates.** What was checked and cleared:

- **Authored shape matches origin intent:** origin `OptionsDisplayScreen`
  resets to palette/camera for the overlay, draws only the full-screen bank-6
  starfield on the root (`options_display_screen.cpp:42-50, 89-96`), then
  lays out a fixed 420-virtual panel with 24/32 padding and 22px child gaps
  but no visible frame (`:97-106`). Its visible contents are one title
  (`:107-108`), two Boolean rows (`:116-129`), and Save/Cancel Md ovals
  (`:131-145`). Origin `BooleanSettingRow` is fixed geometry: 33px row
  height, 24px button-to-indicator gap, 10px indicator gap, and two 20x33
  indicator sprites (`components/boolean_setting_row.cpp:19-23, 58-68`);
  origin `button.cpp:110-143` resolves the row label to the 220x33 Lg oval
  and Save/Cancel to 196x33 Md ovals.
- **Golden pixels express that shape cleanly:** the live inspect tree places
  the title at logical `(516,168) 248x29`, the Fullscreen row at
  `(418,230) 442x50`, the Smooth Scaling row at `(419,309) 441x50`, and the
  action row at `(336,392) 606x50`. Pixel measurement on the fresh capture
  found the title green bbox `(776,252)-(1142,289)`, row bboxes
  `(627,345)-(1283,419)` and `(628,463)-(1283,537)`, matching 118px vertical
  pitch, and the action row bbox `(504,588)-(1406,662)`. Both toggle stacks
  are aligned at x1177 with identical bbox sizes. Evidence:
  `docs/plans/uplift-evidence/options_display/current_form_2x.png`,
  `toggle_rows_current_4x.png`, `save_cancel_row_current_4x.png`, and
  `current_vs_golden_diff_black_2x.png`.
- **No screen-specific accident found:** there is intentionally no visible
  panel frame, both toggle pairs are continuous and aligned, and the two-row
  spacing follows the same fixed origin constants as Audio/Controls. The
  pristine-origin vs current comparison shows only the already adjudicated
  U-1/U-2/U-3 supersession families (canonical glyphs, canonical chrome
  sprites, and single-hop backdrop), not a new Display-specific defect.
  Evidence:
  `docs/plans/uplift-evidence/options_display/pristine_origin_vs_current_form_150pct.png`.
  The remaining PARITY.md breadcrumb (`fullscreenw` row nudge + `indDx`) is
  port-side phase-recovery glue already called out by the systemic
  spacing/alignment audit as a cleanup item, not an uplift candidate.

### Unit note — options_controls (2026-06-12)

Audited after regenerating the menu-cluster renders with
`OUT=/tmp/cppx_renders_uplift_options_controls bash tools/cap/cap_menus.sh`;
the fresh `/tmp/cppx_renders_uplift_options_controls/options_controls.png`
byte-matches the current golden (`cmp_exit=0`, md5
`9e39388a184403cd75e820cad1ecb639`) and the tolerant pixdiff is
`0.0000 (mae=0.00 maxtile=0.0% hot_tiles=0 PASS)`. **Zero candidates.**
What was checked and cleared:

- **Authored shape matches origin intent:** origin `OptionsControlsScreen`
  resets to the menu palette/camera (`options_controls_screen.cpp:76-79`),
  then paints a stretched bank-6 backdrop and stretched bank-7 idx7 controls
  frame (`:285-306`). Its frame/action arithmetic is explicit:
  frame margins 5/7/6/20, min panel 560x420, title y=14, panel pad top 70,
  action top y=405, action row h=33 (`:25-38`, `:225-242`). Origin
  `BuildKeybindListBody` fixes the content grid at 486 virtual px with a
  43px preset row, 43px keybind rows, 10px row gaps, 180/112/45/112 columns
  and 12px gaps, 8px section gaps, 16px action spacer, then Save/Cancel Md
  ovals (`controls_keybind_list.cpp:27-38`, `:146-250`). The cppx screen is
  the same x1.5 grid and frame composition
  (`options_controls.cppx:32-50`, `:292-349`).
- **Golden pixels express that shape cleanly:** the live inspect tree places
  the title at logical `(476,22) 324x29` (device approx `(714,33) 486x44`),
  content at `(273,116) 729x542`, the scroll viewport at `(273,191) 729x369`,
  preset button at `(561,123) 330x50`, keybind primary/secondary lanes at
  x=561/x=833 logical, and Save/Cancel at `(335,609)`/`(647,609)`. Pixel
  measurement on the current golden found title green bbox
  `(680,33)-(1234,87)`, preset oval `(841,184)-(1329,258)`, primary oval
  bboxes `(841,310)-(1090,384)`, `(841,454)-(1090,528)`,
  `(841,598)-(1090,672)`, `(841,742)-(1090,816)`, the secondary lane
  `(1249,310)-(1498,816)`, scrollbar `(1636,193)-(1724,869)`, and action
  buttons `(470,913)-(1424,987)`. Evidence:
  `docs/plans/uplift-evidence/options_controls/current_form_150pct.png`,
  `keybind_rows_current_2x.png`, `scrollbar_track_current_4x.png`, and
  `current_vs_golden_diff_black.png`.
- **No hidden clipped-row or frame accident:** retained off-viewport row nodes
  exist in the inspect dump, but direct pixel sampling found zero green UI px
  in the pre-action gap regions where row 4 would bleed
  (`x380-690/y860-912`, `x1120-1225/y860-912`,
  `x1240-1530/y860-912`); the only green below the viewport is the
  Save/Cancel chrome beginning at y=913. The controls frame, scrollbar rail,
  and four visible keybind rows are continuous with no broken cap/ring seam or
  1px gutter. Evidence:
  `docs/plans/uplift-evidence/options_controls/viewport_bottom_actions_current_3x.png`.
- **Already-adjudicated, not re-opened:** pristine origin
  (`ba345131:tests/cli-agent/e2e/golden/options_controls.png`) shows the same
  four visible movement rows and control structure; current-vs-pristine
  differences are the documented U-1/U-2/U-3 supersessions (canonical text,
  canonical legacy sprites, single-hop stretched backdrop), not a
  Controls-specific defect. Evidence:
  `docs/plans/uplift-evidence/options_controls/pristine_origin_vs_current_form.png`.
  The PARITY.md breadcrumbs (`titlewrap inset-top 13`, `OR ml 2`) remain the
  port-side phase-recovery cleanup called out by the systemic
  spacing/alignment audit, not an uplift candidate.

### Unit note — lobby_connect (2026-06-12)

Audited after regenerating the lobby cluster with
`OUT=/tmp/cppx_renders_uplift_lobby_connect bash tools/cap/cap_lobby.sh`;
the fresh `/tmp/cppx_renders_uplift_lobby_connect/lobby_connect.png`
byte-matches the current golden (`cmp_exit=0`, md5
`397d8e32e7f512cae8d5f20ae93001c7`) and the tolerant pixdiff is
`0.0000 (mae=0.00 maxtile=0.0% hot_tiles=0 PASS)`. **Zero candidates.**
What was checked and cleared:

- **Authored shape matches origin intent:** origin `LobbyConnectScreen`
  defines one fixed 284x277 virtual dialog with a 250x170 log well, two
  21px form rows, a 116x24 stipple patch over the baked button wells, and
  label-fit Chrome buttons (`lobby_connect_screen.cpp:51-70`). It centers
  the baked `PackImage(7, 2)` dialog sprite (`:344-358`), draws the log via
  `ScrollTextBox` at `kLogX/kLogY` with 11px line height (`:365-382`), lays
  out Username/Password labels + text inputs over the baked wells
  (`:395-475`), then patches the old button wells and draws Login/Create +
  Cancel over them (`:497-527`). The cppx screen mirrors the same contract:
  one baked `dialog_connect` sprite with chromeless overlays
  (`clients/silencer/src/client/ui/screens/lobby_connect.cppx:70-147`) and
  absolute log/field/button positions (`:159-257`).
- **Golden pixels express that shape cleanly:** the live inspect tree places
  the panel at logical `(427,152) 426x416` with the sprite anchored one
  logical px left (device x639), log text at logical x436 and y164/180/197/
  213/230, Username/Password labels at `(448,448)` and `(448,489)`, input
  wells at `(571,445) 264x31` and `(571,486) 264x30`, and Login/Create +
  Cancel at `(501,525) 174x32` and `(675,525) 102x32`. Direct pixel
  measurement found the current non-black dialog bbox
  `(639,228)-(1273,851)`; the focused Username caret is the documented lobby
  sage rgb `(116,156,104)` at `(858,678)-(859,701)`. Evidence:
  `docs/plans/uplift-evidence/lobby_connect/current_dialog_2x.png`,
  `current_log_form_3x.png`, `current_chrome_buttons_6x.png`, and
  `username_caret_sage_8x.png`.
- **No screen-specific accident found:** the frame, log well, form wells,
  caret, stipple patch, and Chrome buttons are continuous with no broken cap,
  gutter, clipped stroke, or unintended hover state. The current-vs-pristine
  origin comparison (`git show ba345131:tests/cli-agent/e2e/golden/
  lobby_connect.png`) differs only inside the dialog region
  `(639,230)-(1273,851)`, matching the already adjudicated U-1 canonical text
  and U-2 canonical legacy-sprite supersessions; PARITY.md explicitly notes
  U-3 left this screen byte-unchanged because no backdrop is visible. Evidence:
  `docs/plans/uplift-evidence/lobby_connect/pristine_vs_current_dialog_diff_2x.png`.
  The ORIGIN_GOLDENS chrome-hover negative check also confirms this Chrome
  button family intentionally has no hover visual change, so the rest-state
  buttons are not hiding a missing focused/hovered frame.

### Unit note — character_create (2026-06-12)

Audited the roster step's current golden
(`tests/cli-agent/e2e/golden/character_create.png`) against the pristine
origin capture (`git show ba345131:tests/cli-agent/e2e/golden/
character_create.png`) and the origin Clay-era source
(`origin/main:clients/silencer/src/client/ui/screens/character_create/
character_create_layout.cpp`). Full lobby visual gate
`bash tests/cli-agent/e2e/71_visual_regression_lobby.sh` passed this session,
which re-captures `character_create` in the real login flow. **Zero
candidates.** What was checked and cleared:

- **Authored shape matches origin intent:** origin fixes the stage/panel at
  640x480 and 628x441 virtual px with frame/content padding, a 236px left
  column, 196px right column, 33px title band, 14px title-to-rows gap, 27px
  row height, 5px row gap, and 272px agent-row area
  (`character_create_layout.cpp:45-68`). The root draws the bank-6 starfield
  and baked bank-7 idx5 panel (`:572-599`). The select-agent step is exactly
  a title, a fixed row list, and an empty detail pane unless an existing
  agent is selected (`:621-725`); each row is the `LegacyRow` button sprite
  at `minWidth = kLeftColumnW` (`:493-510`).
- **Golden pixels express that shape cleanly:** direct component measurement
  on the current golden found the closed roster row as one green component at
  `(373,211)-(903,271)` = 531x61 device px; the pristine origin row is the
  same size at `(374,212)-(904,272)`, the known U-2 one-pixel phase residual.
  The title/pane chrome is one continuous left-pane component at
  `(334,82)-(996,972)`, the empty right pane is continuous at
  `(1024,84)-(1563,992)`, and the baked scroll rail is continuous at
  `(914,199)-(956,888)` with its inner fill `(919,235)-(951,852)`.
  Evidence:
  `docs/plans/uplift-evidence/character_create/title_row_scroll_current_3x.png`,
  `roster_row_closed_6x.png`, and `center_divider_join_4x.png`.
- **No screen-specific accident found:** the visible scrollbar/rail is baked
  into the origin panel art rather than conditional row-list logic, the single
  row's oval cap/ring is closed, the panel joins have no 1px seam or broken
  stroke, and the empty right pane is deliberate for a roster with no existing
  agent. Current-vs-pristine differences in the left-pane crop are the already
  adjudicated U-1/U-2/U-3 families (canonical glyphs, canonical legacy
  sprites, single-hop backdrop), not a new roster-step defect. Evidence:
  `docs/plans/uplift-evidence/character_create/
  pristine_vs_current_left_pane_250pct.png`.

### Unit note — cc_alias (2026-06-12)

Audited the alias modal's current golden
(`tests/cli-agent/e2e/golden/cc_alias.png`) after regenerating the lobby
cluster with
`OUT=/tmp/cppx_renders_uplift_cc_alias bash tools/cap/cap_lobby.sh`; the
fresh `/tmp/cppx_renders_uplift_cc_alias/cc_alias.png` byte-matches the
current golden (`cmp_exit=0`, md5 `9b9b9300f755bcbfb08ac5fbc1894311`).
Compared against the pristine origin capture
(`git show ba345131:tests/cli-agent/e2e/golden/cc_alias.png`) and the origin
Clay-era source. **Zero candidates.** What was checked and cleared:

- **Authored shape matches origin intent:** origin uses the same select-agent
  frame behind the modal (`BuildEnterAlias` calls `BuildSelectAgent` with
  `interactive=false`, `origin/main:clients/silencer/src/client/ui/screens/
  character_create/character_create_layout.cpp:728-735`), then floats a fixed
  284x108 virtual `PackImage(40, 2)` alias dialog at `kAliasModalTop=161`
  (`:92-100`, `:737-754`). The title is centered in a 33px title band
  (`:755-773`), and the text input is a fixed 236x27 virtual frame at
  offset `(24,49)` (`:774-790`). The cppx screen mirrors that contract with
  the underlying frame plus a 426x162 logical dialog sprite, a dialog title,
  and a chromeless focused input over the baked well
  (`clients/silencer/src/client/ui/screens/character_create.cppx:279-360`).
- **Golden pixels express that shape cleanly:** the live inspect tree places
  the alias modal at logical `(425,242) 426x162`, title text at
  `(523,252) 231x29`, and the focused Alias input at `(471,316) 354x40`.
  Direct pixel measurement on the current golden found the input well as one
  closed green component `(691,474)-(1221,533)` = 531x60 device px, and the
  empty-field caret as exact menu-palette yellow rgb `(252,252,0)` at
  `(718,491)-(719,514)` (48 px). Evidence:
  `docs/plans/uplift-evidence/cc_alias/current_alias_modal_2x.png`,
  `input_caret_current_6x.png`, and
  `current_frame_and_modal_150pct.png`.
- **No screen-specific accident found:** the dialog frame, side caps, input
  oval, caret, underlying roster row, and baked scroll rail are continuous
  with no clipped cap, gutter, broken ring, or stray focus/hover state.
  Current-vs-pristine differences span the screen because `cc_alias` was
  intentionally superseded by U-1/U-2/U-3; inside the modal crop the visual
  differences are the already adjudicated canonical glyphs, canonical sprite
  phase, and single-hop backdrop, not a new alias-specific defect. Evidence:
  `docs/plans/uplift-evidence/cc_alias/
  pristine_vs_current_alias_modal_150pct.png`. PARITY.md's cc_alias row
  already records the U-1/U-2/U-3 supersession and the historical one-frame
  caret low-tile disclosure; the current real-flow capture is byte-identical
  to the golden.

### Unit note — cc_select_agency (2026-06-12)

Audited the agency picker's current golden
(`tests/cli-agent/e2e/golden/cc_select_agency.png`) after regenerating the
lobby cluster with
`OUT=/tmp/cppx_renders_uplift_cc_select_agency bash tools/cap/cap_lobby.sh`;
the fresh `/tmp/cppx_renders_uplift_cc_select_agency/cc_select_agency.png`
byte-matches the current golden (`cmp_exit=0`, sha256
`5c22d6e92c4fceee5bd8064299fcf3c87c67cab00f5487d1809b65dcdeb4c6ad`).
Compared against the pristine origin capture
(`git show ba345131:tests/cli-agent/e2e/golden/cc_select_agency.png`) and
the origin Clay-era source. Focused lobby visual gate
`bash tests/cli-agent/e2e/71_visual_regression_lobby.sh` passed. **Zero
candidates.** What was checked and cleared:

- **Authored shape matches origin intent:** origin fixes the shared wizard
  grid at a 640x480 stage, 628x441 panel, 236px left column, 196px right
  column, 78px column gap, 33px title band, 14px title-to-rows gap, 27px
  rows, and 5px row gap
  (`origin/main:clients/silencer/src/client/ui/screens/character_create/
  character_create_layout.cpp:45-68`). `BuildSelectAgency` lays out exactly
  one `SELECT AGENCY` left column with five `LegacyRow` buttons and one right
  info column with `Advantages`, parsed advantage rows, `Description`, and a
  wrapped paragraph (`:820-887`). The cppx screen mirrors that contract with
  the same two-pane frame and no extra subtitle/back button/modal chrome
  (`clients/silencer/src/client/ui/screens/character_create.cppx:362-419`).
- **Golden pixels express that shape cleanly:** the live inspect tree places
  the frame at logical `(167,29) 942x662`, the agency rows wrapper at
  `(249,141) 354x232`, five focusable rows at y `141/189/237/285/333` with
  48 logical px pitch, and the right pane at `(721,56) 294x584`. Direct pixel
  component measurement on the current golden found the five row-plate
  components as identical 531x61 device boxes at x `373-903` and y
  `211-271`, `283-343`, `355-415`, `427-487`, `499-559` — the U-4 repaired
  full-width closed ovals. The right-pane green chrome bbox is continuous at
  `(1024,84)-(1563,992)`, matching pristine origin's bbox; the description
  wraps to the same final visible line, ending with `and enhanced durability.`
  Evidence:
  `docs/plans/uplift-evidence/cc_select_agency/current_form_150pct.png`,
  `agency_rows_current_4x.png`, `right_pane_pristine_vs_current_2x.png`,
  `panel_divider_current_6x.png`, and `panel_right_edge_current_6x.png`.
- **No remaining `right-panel clip @x1080` candidate:** the PARITY.md
  breadcrumb was rechecked directly. The focused x1080 crop contains text and
  backdrop only, not a panel edge; green UI-pixel count in
  `x=1068..1091, y=90..359` is zero for both pristine origin and current.
  White text in that crop shifts by one device px (`origin bbox x1080-1091`,
  current x1081-1091), which is the already adjudicated U-1 canonical-glyph
  phase divergence, not a clipped stroke or pane boundary. Evidence:
  `docs/plans/uplift-evidence/cc_select_agency/
  right_panel_x1080_origin_vs_current_12x.png`.
- **Already-adjudicated, not re-opened:** current-vs-pristine differences are
  the documented U-1/U-2/U-3/U-4 supersessions (canonical glyphs, canonical
  legacy sprites, single-hop backdrop, and restored agency-row caps). The
  bracket glyphs still show the PARITY.md two-tone-ramp polish residual, but
  those bracket sprites are present in origin and current, sit beside the same
  parsed `+3`/`+5` metadata, and do not show a design-intent miss. No Linear
  ticket opened because there is no new ARTIFACT/DEFECT/ERA-LIMIT candidate.

### Unit note — lobby_screen (2026-06-12)

Audited the logged-in lobby's current golden
(`tests/cli-agent/e2e/golden/lobby_screen.png`) after regenerating the lobby
cluster with
`OUT=/tmp/cppx_renders_uplift_lobby_screen bash tools/cap/cap_lobby.sh`;
the fresh `/tmp/cppx_renders_uplift_lobby_screen/lobby_screen.png`
byte-matches the current golden (`cmp_exit=0`, sha256
`5087a1b09b3124ae1fb8a42358e52709255ef338e7e37d823f0746d693312375`) and
the tolerant pixdiff is `0.0000 (mae=0.00 maxtile=0.0% hot_tiles=0 PASS)`.
Compared against the pristine origin capture
(`git show ba345131:tests/cli-agent/e2e/golden/lobby_screen.png`) and the
origin Clay-era source. **Zero candidates.** What was checked and cleared:

- **Authored shape matches origin intent:** origin `LobbyScreen::BuildUi`
  resets to lobby palette/page 2, stretches the bank-7 lobby backdrop, then
  computes `rootPadX`, `rootPadTop`, `regionGap`, title height, and body rect
  from the live surface (`origin/main:clients/silencer/src/client/ui/screens/
  lobby/lobby_screen.cpp:57-76,113-176`). The stepped cockpit panes are then
  resolved with fixed legacy ratios: character panel, open-right upper pane,
  elbow seam, chat pane, chat/tall seam, and open-left right-tall pane
  (`lobby_main_area.cpp:153-192,260-386`). The title bar, agent card, empty
  game browser, and chat/presence layout are likewise explicit source
  structure (`lobby_chrome.cpp:91-183`, `character_panel.cpp:308-520`,
  `game_select_panel_layout.cpp:241-287`, `chat_panel_layout.cpp:63-195`).
  The current cppx screen mirrors those constants through
  `resolve_lobby_panes` and the screen-local `LobbyPaneGrid`/panel components
  (`clients/silencer/src/client/ui/components/layout/lobby_panes.h:28-75`,
  `clients/silencer/src/client/ui/components/layout/screen_layout.cppx:23-53`,
  `clients/silencer/src/client/ui/screens/lobby_screen.cppx:70-113,
  756-1032,1067-1143,1150-1344`).
- **Golden pixels express that shape cleanly:** at 1920x1080 the resolved
  design geometry is `pad_x=20`, `pad_top=38`, `gap=20`, `left_w=777`,
  `tall_w=463`, `chat_w=757`, `agent_w=436`, `upper_h=180` logical. Direct
  pixel measurement on the current golden found the title bar UI bbox at
  `(30,57)-(1889,121)`, agent card `(30,151)-(683,420)`, upper-right/create
  pane `(714,151)-(1195,421)`, right-tall Active Games cell
  `(1195,151)-(1889,1023)`, chat panel `(30,450)-(1165,1023)`, chat log well
  `(45,499)-(1149,965)`, Active Games well `(1216,207)-(1869,771)`, and
  agent stat well `(397,211)-(670,407)`. Evidence:
  `docs/plans/uplift-evidence/lobby_screen/agent_card_current_2x.png`,
  `active_games_empty_cell_current_2x.png`, and
  `chat_presence_split_current_2x.png`.
- **No new screen-specific accident found:** the apparent overlapping
  cockpit lines are the authored open-edge pane joins and seam strips from
  origin's `OpenRightChrome`, `OpenLeftChrome`, `RightEdgeChrome`, and
  `LobbyElbowGapSeam`/`LobbyChatTallSeam` layout, not a clipped or doubled
  border. The zoomed seam crop shows continuous strokes at the elbow/right
  tall join with no 1px black gutter or broken endpoint:
  `docs/plans/uplift-evidence/lobby_screen/
  pane_elbow_and_right_tall_seam_current_4x.png`. The large empty
  Active Games well is also intentional: origin builds an empty scroll-list
  frame when `state.rows` is empty and has no placeholder text
  (`game_select_panel.cpp:34-60,192-207`;
  `game_select_panel_layout.cpp:154-193`).
- **Already-adjudicated, not re-opened:** current-vs-pristine differences are
  the documented U-1/U-2/U-3 supersessions (canonical text, canonical legacy
  sprites/buttons/emblem, and single-hop lobby backdrop). A side-by-side crop
  of the pristine and current left pane grid is saved at
  `docs/plans/uplift-evidence/lobby_screen/
  pristine_vs_current_left_pane_grid.png`; those differences do not expose a
  new lobby-screen-specific ARTIFACT/DEFECT/ERA-LIMIT candidate. No Linear
  ticket opened.

### Unit note — create_game (2026-06-12)

Audited the Create Game lobby panel after regenerating the lobby cluster with
`OUT=/tmp/cppx_renders_uplift_create_game bash tools/cap/cap_lobby.sh`; the
fresh `/tmp/cppx_renders_uplift_create_game/create_game.png` byte-matches the
current golden (`cmp_exit=0`, sha256
`672621b5bed1d9552e7128b01343c89fc7dc0a8694853ebd31f5df2b473cb966`).
Compared against the pristine origin capture
(`git show ba345131:tests/cli-agent/e2e/golden/create_game.png`) and the
origin Clay-era source. **Zero candidates.** What was checked and cleared:

- **Authored shape matches origin intent:** origin routes create mode into the
  same stepped lobby grid as the logged-in lobby: `ResolveSteppedPaneLayout`
  computes upper/right-tall cells, `BuildRightUpperContents` mounts
  `BuildGameCreateUpperTree`, and `BuildRightTallContents` mounts
  `BuildGameCreateTallTree`
  (`origin/main:clients/silencer/src/client/ui/screens/lobby/lobby_main_area.cpp:153-240`).
  The upper tree is a fixed Game Options form: content pad 6, heading, inset
  form border, five visible 14px rows from a six-row option list, value
  column, and an 8px scrollbar with a 1px track pad
  (`origin/main:clients/silencer/src/client/ui/screens/lobby/game_create_panel_options.cpp:99-150,280-440`).
  The tall tree is a Select Map heading, inset map list, Game name/Password
  footer, and full-width Create button
  (`origin/main:clients/silencer/src/client/ui/screens/lobby/game_create_panel_map_form.cpp:243-270,285-307,318-370,376-400`).
  The current cppx screen mirrors those two cells in `GameCreatePanel`,
  `CreateRightCell`, and `map_list_rows`
  (`clients/silencer/src/client/ui/screens/lobby_screen.cppx:316-443,455-522,1035-1065`).
- **Golden pixels express that shape cleanly:** the live inspect tree places
  the Game Options title at device `(728,165)` with rows at y
  `212/250/288/327/364/404`; numeric values sit on the intended top-biased
  cells (`0` at y255, `99` at y292, `24` at y332, `6` at y369). The map list
  rows are 21 logical px / 32 device px slots, from `ALLY10c.sil`
  `(1210,208)` through `sewers10.sil` `(1210,586)`, and the footer anchors at
  `Game name:` `(1209,796)`, `New Game` `(1209,846)`,
  `Password (optional):` `(1209,880)`, and Create button
  `(1209,963) 668x48`. Pixel masks found continuous green UI bboxes for the
  options cell `(714,151)-(1209,444)`, right cell `(1180,151)-(1889,1039)`,
  map-list well `(1209,190)-(1876,619)`, and Create button
  `(1193,963)-(1889,1023)`. Evidence:
  `docs/plans/uplift-evidence/create_game/current_game_options_4x.png`,
  `current_right_cell_2x.png`, and `current_map_list_footer_2x.png`.
- **No remaining scrollbar or clipping candidate:** the U-5 scrollbar repair
  is present in the current golden; the thumb is flush inside the snapped
  track (solid fill columns x1157-1169 have 161 green rows each), with no
  right-side gutter or top-left overpaint. Evidence:
  `docs/plans/uplift-evidence/create_game/current_scrollbar_thumb_8x.png`.
  The Select Map list frame, footer text, and wide Create chrome are
  continuous; no row is clipped, and no off-viewport row bleeds into the
  footer/action area.
- **Already-adjudicated, not re-opened:** current-vs-pristine origin differs
  in 176,710 pixels across the screen, expected after the documented
  U-1/U-2/U-3/U-5 supersessions (canonical text, canonical legacy sprites,
  single-hop lobby backdrop, restored scrollbar thumb). Side-by-side and
  changed-pixel evidence is saved at
  `docs/plans/uplift-evidence/create_game/
  pristine_vs_current_game_options_2x.png`,
  `pristine_vs_current_right_cell_150pct.png`, and
  `pristine_vs_current_changed_pixels_magenta_2x.png`. No Linear ticket
  opened because there is no new ARTIFACT/DEFECT/ERA-LIMIT candidate.

### Unit note — game_staging (2026-06-12)

Audited the pregame staging screen after regenerating the lobby cluster with
`OUT=/tmp/cppx_renders_uplift_game_staging bash tools/cap/cap_lobby.sh`;
the fresh `/tmp/cppx_renders_uplift_game_staging/game_staging.png`
byte-matches the current golden (`cmp_exit=0`, sha256
`f86758a5be74544a4bd1afc6303043f35f5856a9515947736eaa465240298718`) and
the tolerant pixdiff is `0.0000 (mae=0.00 maxtile=0.0% hot_tiles=0 PASS)`.
Compared against the pristine origin capture
(`git show ba345131:tests/cli-agent/e2e/golden/game_staging.png`, sha256
`81c2c37bf4a30958e6cc93fc3742460aaf6071369ccfbcd23ebe0daab2dc6c3a`) and
the origin Clay-era source. **Zero candidates.** What was checked and
cleared:

- **Authored shape matches origin intent:** origin `LobbyScreen` switches the
  joined-game state into `gameJoinActive` and shows the joined map name in the
  title bar (`origin/main:clients/silencer/src/client/ui/screens/lobby/
  lobby_screen.cpp` lines 94-111, 184-187). The staging upper cell is exactly
  three full-width Chrome buttons with authored top pads 3/11/39 virtual px
  (`game_join_panel.cpp:33-39,155-191`). The right-tall staging roster uses
  fixed anchors for agency emblem, ready checkbox, player name, and level
  text, with rowY = `team*55 + slot*13`
  (`game_join_panel.cpp:194-319`). The chat panel groups presence rows as
  `In Lobby` / `Pregame` / `Playing` and derives the two-column chat/presence
  layout from the panel metrics (`chat_panel.cpp:169-205,220-318`). The cppx
  screen mirrors those contracts in `StagingPanel`, `StagingRosterCell`,
  `LobbyTitleBar`, and `LobbyChatPanel`
  (`clients/silencer/src/client/ui/screens/lobby_screen.cppx:525-575,
  675-743,748-820,968-1032,1222-1277`).
- **Golden pixels express that shape cleanly:** the live inspect tree places
  the staging buttons at logical `(480,105)`, `(480,153)`, and `(480,243)`,
  all `311x32`; the right roster cell at `(797,100) 463x582`; the player row
  emblem at `(830,113) 48x41`, ready box at `(882,110) 24x20`, name at
  `(909,113)`, and level at `(959,113)`; and the chat title/presence at
  `#New Game-1` `(30,308)`, `Pregame` `(537,338)`, and
  `Alice [New Game]` `(537,354)`. Direct pixel measurement found continuous
  green UI bboxes for the title bar `(30,57)-(1889,121)`, agent card
  `(30,151)-(683,420)`, staging-button region `(714,151)-(1210,430)`, chat
  panel `(30,450)-(1178,1035)`, and roster cell `(1180,151)-(1889,1023)`.
  Evidence:
  `docs/plans/uplift-evidence/game_staging/current_layout_50pct.png`,
  `staging_buttons_current_4x.png`, `roster_row_current_6x.png`, and
  `chat_pregame_current_3x.png`.
- **No screen-specific accident found:** the three staging buttons have
  continuous Chrome rings, the large empty right-tall area below the single
  local player is the expected one-player roster state, the title-bar map
  overlay is present and aligned, and the chat/presence panel shows the
  pregame channel state without clipped text, row bleed, or missing chrome.
  Current-vs-pristine origin differs by 176,348 pixels, but the changed-pixel
  overlay shows the same already-adjudicated U-1/U-2/U-3 families as
  lobby_screen/create_game: canonical text, canonical legacy sprites, and
  single-hop lobby backdrop. Evidence:
  `docs/plans/uplift-evidence/game_staging/
  pristine_origin_vs_current_upper_roster_50pct.png`,
  `pristine_vs_current_changed_pixels_magenta_50pct.png`, and
  `current_vs_golden_diff_black_50pct.png`. No Linear ticket opened because
  there is no new ARTIFACT/DEFECT/ERA-LIMIT candidate.

### Unit note — tech_select (2026-06-12)

Audited the pregame Choose Tech panel after regenerating the lobby cluster
with
`OUT=/tmp/cppx_uplift_tech_select_20260612093756 bash tools/cap/cap_lobby.sh`;
the fresh `/tmp/cppx_uplift_tech_select_20260612093756/tech_select.png`
byte-matches the current golden (`cmp_exit=0`, sha256
`8beb5c93fc442cdb6c62917e90ad8a82b179435f05698f904aad3d2fbf07f240`) and
the tolerant pixdiff is `0.0000 (mae=0.00 maxtile=0.0% hot_tiles=0 PASS)`.
Compared against the pristine origin capture
(`git show ba345131:tests/cli-agent/e2e/golden/tech_select.png`, sha256
`fdd5b56a1216600b714c8ced5fb4de3982e7b10449885a20054e9939b818129f`) and
the origin Clay-era source. **Zero candidates.** What was checked and
cleared:

- **Authored shape matches origin intent:** origin mounts the tech screen by
  swapping the joined-game upper cell from Choose/Change/Ready to a single
  full-width `Back To Teams` button, and the right-tall cell to
  `BuildGameTechTallTree`
  (`origin/main:clients/silencer/src/client/ui/screens/lobby/
  lobby_main_area.cpp:215-249`). The tech tall tree places
  `Tech slots left: N` at virtual pad `(57,36)` with
  `LegacyPalette(129,144,true)`, then builds the grid
  (`game_tech_panel.cpp:252-264`). The grid constants are fixed 13px rows,
  local column 3, 13x13 bank-7 toggle sprites, 1px column gap, and labels two
  px to the right/two px down from the toggle row
  (`tech_tree_grid.cpp:32-46,70-75,101-118,131-146,191-245`). The current
  cppx screen mirrors those anchors directly in `TechUpperPanel` and
  `TechListCell`
  (`clients/silencer/src/client/ui/screens/lobby_screen.cppx:584-602,
  605-672`), with the same model-side slots-left/filter/interactable rules
  (`clients/silencer/src/game/ui/lobby_ui_model.cpp:238-268`).
- **Golden pixels express that shape cleanly:** live inspect found 15
  `TechRow` controls, all at logical x `878`, with row y positions
  `192/212/231/251/270/290/309/329/348/368/387/407/426/446/465`; at the
  1920x1080 device scale those floor to
  `288/318/346/376/405/435/463/493/522/552/580/610/639/669/697`, the
  expected 13-virtual-px rhythm at 2.25x (`30/28/30/29` repeating). The slots
  text gray-pixel bbox is `(1325,232)-(1566,250)`, matching virtual
  `(588,103)` after scale/ink-bearing offset. The visible rows are the
  origin-filtered Noxis list: `Laser`, `Rocket`, `Flamer Ammo`,
  `Health Pack`, `E.M.P. Bomb`, `Shaped Bomb`, `Plasma Bomb`,
  `Neutron Bomb`, `Plasma Detonator`, `Fixed Cannon`, `Flare`, `Camera`,
  `Base Door`, `Base Defense`, `Insider Info`. The first two selected rows
  and the one-slot enabled rows remain bright; multi-slot rows are dim because
  the readout reports one slot left. Evidence:
  `docs/plans/uplift-evidence/tech_select/tech_grid_anchor_overlay_2x.png`.
- **No screen-specific accident found:** checkbox rings are closed, selected
  fills are not clipped, dim rows follow the origin `techslotsleft` logic,
  labels stay inside the right-tall panel, and the `Back To Teams` chrome is
  continuous. Current-vs-pristine origin differs by 177,648 pixels, but the
  differences are the documented U-1/U-2/U-3 supersessions (canonical text,
  canonical legacy sprites/toggles/buttons, and single-hop lobby backdrop).
  Side-by-side context is saved at
  `docs/plans/uplift-evidence/tech_select/
  tech_grid_pristine_vs_current_2x.png`. No Linear ticket opened because
  there is no new ARTIFACT/DEFECT/ERA-LIMIT candidate.

### Unit note — mission_summary (2026-06-12)

Audited the post-match Mission Summary after running the dedicated gate
`bash tests/cli-agent/e2e/74_visual_regression_mission_summary.sh`; the
fresh `/tmp/cppx_renders/mission_summary_e2e.png` byte-matches the current
golden (`fresh_vs_golden_changed_px=0`) and the tolerant pixdiff is
`0.0000 (mae=0.00 maxtile=0.0% hot_tiles=0 PASS)`. The same gate also
re-asserted the scroll behavior: rest first line `Kills:`, after -3 notches
first visible text `Secrets`, clamped end includes the final Flamer
`Player kills:` line, and scrolling back clamps at the top. Compared against
the pristine origin capture
(`git show ba345131:tests/cli-agent/e2e/golden/mission_summary.png`) and the
origin Clay-era source. **Zero candidates.** What was checked and cleared:

- **Authored shape matches origin intent:** origin defines one fixed
  `640x480` stage with a `628x441` bank-7 idx-5 panel, frame margins
  `5/7/19/20`, a stats column at `(89,92)` sized `180x308` but clamped by a
  `300px` scroll window, six upgrade rows at `kLevelStartY + index*46`, and
  seven `196x33` medium ovals for upgrades + Done
  (`origin/main:clients/silencer/src/client/ui/screens/mission_summary/
  mission_summary_screen.cpp:41-72,304-397`). Wheel input in origin is a
  single accumulated `Scroll` action clamped to `[0, lines - 27]`
  (`mission_summary_screen.cpp:251-258,407-417`). The cppx screen mirrors
  the same floating grid, scroll window, row constants, and upgrade labels in
  `clients/silencer/src/client/ui/screens/mission_summary.cppx:32-68,
  195-239,246-268`.
- **Golden pixels express that shape cleanly:** live inspect places the
  stats wheel target at `(293,139) 270x462`, title at `(312,66)`, XP at
  `(788,68)`, upgrade labels at x `744`, right-aligned values at x `984`,
  six upgrade controls at x `717` with y `162/231/300/369/438/507`, and
  Done at `(717,582)`, all `294x50` logical controls. Direct pixel masks on
  the fresh render found continuous green components for the outer panel
  `(250,43)-(1658,1035)`, left stats pane `(334,82)-(996,972)`, right upgrade
  pane `(1024,84)-(1563,992)`, scroll rail `(914,199)-(956,888)`, its inner
  fill `(919,235)-(951,852)`, and seven oval buttons
  `(1075,243)-(1509,947)` at the expected row bands. Evidence:
  `docs/plans/uplift-evidence/mission_summary/current_panel_150pct.png`,
  `summary_column_current_3x.png`, `upgrade_rows_current_2x.png`, and
  `scrollbar_and_done_current_4x.png`.
- **No screen-specific accident found:** the panel joins, scroll rail,
  summary text column, six upgrade rows, and Done button are continuous with
  no broken cap, gutter, clipped stroke, row bleed, or unintended overlap.
  The apparent static scrollbar is baked into the origin panel art while the
  text window itself scrolls; origin passes `showScrollbar=false` for the
  `ScrollTextBox`, and the real wheel probe above verifies the text movement.
  Text-content quirks were also checked: origin literally passes
  `Suicides` without a colon and uses bare section headers such as
  `Secrets`, `Files`, and `Grenades thrown` in the same summary-line list
  (`mission_summary_screen.cpp:443-479`), so there is no pixel-level
  evidence of a rendering miss to elevate.
- **Already-adjudicated, not re-opened:** current-vs-pristine origin has a
  tolerant diff of `3.2245` with hot tiles concentrated around the panel and
  buttons, expected after the documented U-1/U-2/U-3 supersessions
  (canonical text, canonical legacy sprites/buttons/panel, and single-hop
  backdrop). Changed-pixel context is saved at
  `docs/plans/uplift-evidence/mission_summary/
  pristine_vs_current_uplift_delta_overlay.png`; the zero fresh-vs-golden
  comparison is saved at
  `docs/plans/uplift-evidence/mission_summary/
  fresh_vs_golden_diff_black.png`. The known PARITY.md wheel-scope
  deviation remains deliberately documented: origin scrolls from anywhere on
  the screen, while cppx routes wheel through the hovered `SummaryScroll`
  target. No Linear ticket opened because there is no new
  ARTIFACT/DEFECT/ERA-LIMIT candidate.

### Unit note — message_modal / password_modal (2026-06-12)

Audited the two cppx-only modal baselines after running the full menu/modal
gate `bash tests/cli-agent/e2e/70_visual_regression.sh` (PASS) and then
recapturing both modals with the same `show_message_modal` /
`show_password_modal` control-port sequence plus the logo-HOLD stability
poll. Fresh captures in `/tmp/cppx_uplift_modals_20260612/` byte-match the
current goldens (`cmp=0`, `pixdiff=0.0000` for both), and inspect dumps prove
the expected modal focus behavior (message OK focused; password input focused;
mainmenu remains mounted behind the overlay).

Outcome: one candidate, U-7. These baselines have no standalone origin trigger
(`ORIGIN_GOLDENS.md` already marks them cppx-only), but the old origin modal
classes are still source evidence and they contradict the current pixels:
origin fixes the message/password modal element boxes at `352x178` and
`352x148`, while current cppx sizes them from the raw registered bank-40
sprite dimensions (`320x88` and `284x108` logical in the live tree). The
result is visible text overflow in both current baselines. Evidence crops:
`docs/plans/uplift-evidence/message_modal_password_modal/
message_modal_current_dialog_2x.png`,
`password_modal_current_dialog_2x.png`,
`message_modal_text_button_4x.png`,
`password_modal_prompt_field_button_4x.png`,
`message_modal_current_yellow_vs_origin_cyan_2x.png`,
`password_modal_current_yellow_vs_origin_cyan_2x.png`, and
`modal_size_overflow_before_overlay_2x.png` (also attached to SIL-51).

No separate candidate recorded for the animated mainmenu backdrop behind the
modals: scenario 70 intentionally captures during the logo HOLD window, and
the background differences are already covered by the prior U-1/U-2/U-3
supersessions. The Chrome OK buttons remain the origin no-hover chrome family
documented in ORIGIN_GOLDENS; this audit found no additional button-state or
focus-trap issue beyond the modal container/text layout defect.

### Unit note — ingame: hud_base / top_ticker / status_lines (2026-06-12)

Audited the first in-game unit after running the in-game gates and then
recapturing persistent evidence with the same deterministic drives:
`bash tests/cli-agent/e2e/72_visual_regression_ingame.sh` PASS and
`bash tests/cli-agent/e2e/76_visual_regression_ingame_extra.sh` PASS;
`tools/cap/cap_ingame_cppx.sh /tmp/cppx_uplift_ingame_base_20260612` and
`tools/cap/cap_ingame_cppx_extra.sh /tmp/cppx_uplift_ingame_extra_20260612`
produced the crops below. **Zero candidates.** What was checked and cleared:

- **Authored shape matches origin intent:** origin's in-game HUD remains a
  fixed 640x480 device-coordinate overlay, not the menu whole-frame magnify
  path. `origin/main:clients/silencer/src/client/ui/hud/InGameHud.cpp:20-67`
  orders system camera, status sprites, readouts, team strip, secret/trace,
  buy/tech, and chat. `hud_status_sprites.cpp:40-180` places minimap,
  gauges, weapon face/selector, inventory icons, and letters at exact sprite
  offsets. `hud_readouts.cpp:14-88` places ammo/health/shield/credits and
  inventory counts at fixed pens. `InGameOverlays.cpp:147-209` centers the
  bottom status stack with `TextAdvance(BodySm)=7`, draws shadow/main lines
  at `(x+1, y+1)` and `(x, y)`, and draws the top ticker at `(200,10)` with
  a 35-char window. The cppx port mirrors these rules through `L(d)` integer
  device recovery and the same coordinates in
  `clients/silencer/src/client/ui/screens/in_game_screen.cppx:24-120`,
  `:127-260`, and `:702-748`; snapshot state for top/status lines is copied
  from world messaging in
  `clients/silencer/src/game/ui/world_session_model.cpp:316-329`.
- **Golden pixels express that shape cleanly:** fresh `ingame_hud_base`
  current-vs-golden measurements were byte-identical in the checked HUD
  readout regions: lower-left ammo/weapon/status crop `(0,400)-(180,480)`,
  lower-right files/credits crop `(450,400)-(640,480)`, and the top-right
  inventory crop `(520,0)-(640,80)` all had `changed_px=0`. Evidence:
  `docs/plans/uplift-evidence/ingame_hud_base_top_ticker_status_lines/
  hud_lower_left_golden_vs_current_4x.png`,
  `hud_lower_right_golden_vs_current_4x.png`,
  `hud_inventory_golden_vs_current_4x.png`, and `measurements.json`.
- **Top ticker has no ticker-specific artifact:** the golden/current crop
  shows `*MUSIC PAUSED*` at the origin window, with text ink at the expected
  leading-space offset. The fresh crop `(190,0)-(470,35)` differs only in
  sparse rain pixels (`top_ticker_diff_black_5x.png` marks changed pixels as
  magenta); no changed pixels touch the green ticker glyphs. Evidence:
  `top_ticker_golden_vs_current_5x.png` and
  `top_ticker_diff_black_5x.png`.
- **Status stack has no fade/centering defect:** `Can't build a base here`
  appears centered over the terminal at the origin fade frame, and the
  golden/current status crop `(190,340)-(470,390)` is byte-identical
  (`changed_px=0`). The line's x/y, shadow offset, color fade, and
  bottom-stack position match origin's `DrawStatus` arithmetic. Evidence:
  `status_line_golden_vs_current_6x.png` and
  `status_line_diff_black_6x.png`.
- **Already-adjudicated, not re-opened:** full-frame fresh captures still
  show the documented in-game capture nondeterminism from ORIGIN_GOLDENS
  (rain/minimap/civilian masks), and the scenario gates passed under those
  masks. The in-game set remains intentionally pristine-origin-gated after
  U-1/U-5, so palette U-6 and menu-scale uplift decisions are not re-litigated
  here. No Linear ticket opened because there is no new ARTIFACT/DEFECT/
  ERA-LIMIT candidate in this unit.

### Unit note — ingame: chat (open + history) / messages (2026-06-12)

Audited the in-game chat compose/history surfaces plus the center tutorial
message after fresh capture and gates:
`bash tests/cli-agent/e2e/72_visual_regression_ingame.sh` PASS and
`bash tests/cli-agent/e2e/77_ingame_chat_behavior.sh` PASS. Persistent capture
for evidence:
`PORT=63920 bash tools/cap/cap_ingame_cppx.sh /tmp/cppx_uplift_ingame_chat_20260612`.
**Zero candidates.** What was checked and cleared:

- **Authored shape matches origin intent:** origin chat chrome is explicitly
  fixed at `kChatX=400`, `kChatY=280`, `kChatW=231`, `kChatChromeH=70`,
  `kTextX=10`, `kTextStartY=10`, and `kLineStepY=10`
  (`origin/main:clients/silencer/src/client/ui/hud/hud_chat_overlay.cpp:25-36`).
  `BuildChatOverlay` only renders when chat is active or `showChatTicks > 0`,
  keeps at most five displayed rows by dropping the oldest row when compose is
  active, truncates history lines to 36 chars, draws `(ALL):` / `(TEAM):` plus
  a 28-visible-char input, and registers the channel toggle over the prefix
  (`hud_chat_overlay.cpp:142-236`). Chat wrapping/history population and the
  100-char send cap are origin world behavior
  (`world_messaging.cpp:34-52`, `:119-127`). The cppx port mirrors those
  constants and branches in `build_chat_overlay`
  (`clients/silencer/src/client/ui/screens/in_game_screen.cppx:414-482`), with
  scrollback copied oldest->newest from world messaging
  (`clients/silencer/src/game/ui/world_session_model.cpp:363-370`).
- **Golden pixels express that shape cleanly:** fresh
  `ingame_chat_open` and `ingame_chat_history` crops visually match the
  goldens. The remaining crop diffs are sparse rain pixels, not panel/text
  structure: open chat crop `(380,270)-(640,370)` has 209 changed px, 174
  inside documented masks and 35 outside at `(475,270)-(539,294)`; history
  crop has 189 changed px, 171 inside masks and 18 outside at
  `(471,270)-(579,295)`. The current panel/text green bboxes are stable at
  `(390,278)-(634,349)` for both chat surfaces. Evidence:
  `docs/plans/uplift-evidence/ingame_chat_open_history_messages/`
  (`ingame_chat_open_golden_vs_current_diff_4x.png`,
  `ingame_chat_history_golden_vs_current_diff_4x.png`, `measurements.json`).
- **Center message reveal matches origin intent:** origin `DrawMessage` reveals
  one glyph per `message_i`, uses type-0 color 208 at `liney=60`, title
  advance 11, 20px line height, per-character centering, shadow at `(x+1,y+1)`,
  and the documented pulse/fade brightness arithmetic
  (`origin/main:clients/silencer/src/client/ui/hud/InGameOverlays.cpp:58-129`).
  The cppx port is the same arithmetic in `build_center_message`
  (`clients/silencer/src/client/ui/screens/in_game_screen.cppx:755-825`).
  Fresh `ingame_messages` high-blue glyph mask is byte-stable against the
  golden: bbox `(156,60)-(483,96)`, 2244 px current vs 2244 px golden, xor 0.
  The visual crop's 134 differing px are rain/background, not glyph pixels.
  Evidence:
  `docs/plans/uplift-evidence/ingame_chat_open_history_messages/`
  (`ingame_messages_golden_vs_current_diff_3x.png`, `measurements.json`).
- **Behavioral contract remains origin-matched:** scenario 77 re-confirmed Esc
  closes compose without history, Enter sends and closes without appending a
  local single-player history line, channel toggle keeps input open and flips
  ALL/TEAM, and compose text caps at 100 chars. Those are already documented
  in PARITY.md as origin behavior, not design defects. No Linear ticket opened
  because there is no new ARTIFACT/DEFECT/ERA-LIMIT candidate in this unit.

### Unit note — ingame: player_list / system_camera (2026-06-12)

Audited after fresh persistent captures:
`PATH=/tmp/cppx-uplift-py/bin:$PATH PORT=<fresh> bash
tools/cap/cap_ingame_cppx.sh /tmp/cppx_uplift_ingame_player_system/base`
and `... cap_ingame_cppx_extra.sh
/tmp/cppx_uplift_ingame_player_system/extra`. The base script captured
`ingame_player_list`; the extra script captured `ingame_system_camera` after
its inset-presence probe. `ingame_player_list` passed the documented tolerant
gate against the current golden (`0.1821 (mae=0.12 maxtile=4.8% hot_tiles=0
PASS)` with the standard in-game masks). `bash
tests/cli-agent/e2e/78_ingame_bindings_quit.sh` also passed, re-confirming
the raw-scancode F1 hold semantics. **Zero candidates.** What was checked and
cleared:

- **Player-list authored shape matches origin intent:** origin F1 is
  hold-driven (`origin/main:clients/silencer/src/game/input/game_input.cpp:
  270-307`), and the current input path matches it
  (`clients/silencer/src/game/input/game_input.cpp:220-258`). Origin
  `BuildPlayerListOverlay` uses one fixed root with 50px side/top padding, a
  centered panel of height `10 + teams*58`, 10px panel padding, 40px emblem
  slot, 58px team rows, vertically-centered 12px peer rows, and the exact
  stats string (`origin/main:clients/silencer/src/client/ui/hud/
  hud_player_list_overlay.cpp:16-98`). The cppx port is the same integer
  640x480 device-coordinate layout in
  `clients/silencer/src/client/ui/screens/in_game_screen.cppx:561-625`.
  Origin populates row data from each team's peers/user profile stats and
  2x bank-181 emblem size
  (`origin/main:clients/silencer/src/client/ui/views/HudView.cpp:79-144`);
  current snapshot/provider code copies the same peer names, agency stats,
  dead/secret state, pulse-resolved team sprites, and emblem texture data
  (`clients/silencer/src/game/ui/world_session_model.cpp:256-314`,
  `clients/silencer/src/client/ui/providers/world_session_provider.cpp:
  140-194`).
- **Player-list golden pixels express that shape cleanly:** current
  player-list-vs-hud-base overlay bbox is exactly `(50,50)-(589,117)` =
  `540x68`, matching the one-team panel contract. Within the panel, the
  current and golden green UI bbox is identical at `(100,78)-(573,87)`, 572
  px. The fresh crop shows the emblem, "Player" label, and stats text aligned
  and unclipped; residual full-frame diff is the already documented in-game
  rain/minimap nondeterminism outside the overlay contract. Evidence:
  `docs/plans/uplift-evidence/ingame_player_list_system_camera/
  player_list_golden_vs_current_panel_4x.png` and
  `player_list_diff_panel_amplified_4x.png`.
- **System-camera authored shape matches origin intent:** origin draws each
  active system-camera world inset as a 135x44 surface, follows the configured
  object plus world offsets, calls `DrawWorldScaled(..., factor=2)`, applies
  `EffectRampColor(..., 190)`, and blits slot 0/1 at `(5,349)` / `(500,348)`
  before the minimap (`origin/main:clients/silencer/src/game/ui/
  game_ui_pipeline.cpp:122-155`). Current pipeline code is the same
  (`clients/silencer/src/game/ui/game_ui_pipeline.cpp:113-146`). Origin's
  frame builder positions the bank-95 frame at `x=-spriteoffsetx[95][idx]`
  and y anchored to bank-92's offset plus logical y
  (`origin/main:clients/silencer/src/client/ui/hud/
  hud_system_camera.cpp:13-40`); current cppx frame code mirrors that with
  `kLogicalY={381,318}` and `syscam_oy` from bank 92
  (`clients/silencer/src/client/ui/screens/in_game_screen.cppx:631-642`,
  `clients/silencer/src/client/ui/hooks/use_chrome.h:178-181`).
- **System-camera geometry is clean; the mismatch is the known waiver:** the
  fresh full-frame current-vs-golden system-camera comparison still fails
  (`1.8232 ... FAIL`) for the PARITY.md stale-temppalette origin bug after
  base entry; that deliberate divergence is already documented and not
  re-opened. Geometry measurements in the audited crop are identical:
  current and golden green frame/inset bbox `(1,327)-(140,444)`, 8935 px, and
  dark-green fill bbox `(1,327)-(140,444)`, 8332 px. The crop diff bbox
  `(0,319)-(159,444)` with max channel delta 32 is a palette/brightness
  difference over the same shape, not a new placement, clipping, or scaling
  defect. Evidence:
  `docs/plans/uplift-evidence/ingame_player_list_system_camera/
  system_camera_golden_vs_current_left_inset_4x.png` and
  `system_camera_diff_left_inset_amplified_4x.png`.
- **Already-adjudicated, not re-opened:** `ingame_system_camera` remains
  intentionally not render-gated for the documented palette-history waiver,
  and ORIGIN_GOLDENS already calls out that the multi-team F1 scoreboard
  variant is not captured. This unit found no new ARTIFACT/DEFECT/ERA-LIMIT
  candidate to ticket.
