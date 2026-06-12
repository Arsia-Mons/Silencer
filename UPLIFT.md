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
| options_audio | UNEXAMINED | |
| options_display | UNEXAMINED | |
| options_controls | UNEXAMINED | |
| lobby_connect | UNEXAMINED | |
| character_create | UNEXAMINED | |
| cc_alias | UNEXAMINED | |
| cc_select_agency | UNEXAMINED | |
| lobby_screen | UNEXAMINED | |
| create_game | UNEXAMINED | |
| game_staging | UNEXAMINED | |
| tech_select | UNEXAMINED | |
| mission_summary | UNEXAMINED | |
| message_modal / password_modal | UNEXAMINED | |
| ingame: hud_base / top_ticker / status_lines | UNEXAMINED | |
| ingame: chat (open + history) / messages | UNEXAMINED | |
| ingame: player_list / system_camera | UNEXAMINED | |
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
