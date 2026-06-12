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
| [systemic] background image double-scaling / banding | UNEXAMINED | |
| [systemic] float-rect flooring: 1px seams & jitter | UNEXAMINED | |
| [systemic] palette quantization & dim formulas | UNEXAMINED | |
| [systemic] spacing/alignment consistency across siblings | UNEXAMINED | |
| mainmenu | UNEXAMINED | |
| options | UNEXAMINED | |
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
