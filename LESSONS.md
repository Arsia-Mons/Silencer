# LESSONS.md — one lesson per entry, why it mattered

- **The software renderer FLOORS float dst rects — sampling phase is uncontrollable from
  the draw rect.** (oval striping fix) Measured: dst.x=1027.5 rasterizes ink from 1027.
  So `+0.5`/src-offset tricks that work on real GPU float rects are dead here; with
  integer rects + center sampling only one of origin's four phases is reachable. The
  exact path is the same as the backdrop/element bakes: evaluate origin's int chain at
  absolute device pixels and draw the result 1:1. Verify the rasterization rule from
  ink bboxes BEFORE designing around a sampling model.

- **Draw-time variant resolution beats build-time texture choice for phase-dependent
  sprites.** Retained UI builds before layout, so a component can't pick its phase
  variant — but the EXECUTOR sees the final device rect. register_legacy_sprite +
  resolve_legacy_variant (lazy bake memoized on (X%18, Y%18)) needed no IR change, no
  per-screen textures, and automatically covers every future screen using the sprite.
  The authoring side only owes the grid: <=1-logical-px nudges so round(X/2.25) hits
  the golden cell (measure golden bboxes first — mainmenu alone uses all 4 y-phases,
  so canonical-phase layouts are impossible).

- **A texture's interior can be byte-exact and labels still red the tile — text has its
  own phase family.** After the oval fix, options labels split corr 1.0000 ("Controls",
  exact to the byte) vs 0.94-max ("Audio") on the SAME screen with identical authoring
  arithmetic: the single-phase glyph atlas can't render a string whose golden phase
  differs. Don't chase such tiles with position nudges — corr that plateaus below ~0.99
  at every integer shift means the PATTERN differs, not the placement.

- **Sub-device-pixel padding bias can fix half the labels without moving the rest.**
  Glyph x = floor(1.5 * float-center): labels split into .25/.125- and .75/.625-fraction
  classes; +0.375/+0.75 device of left/top padding pushes only the high-fraction class
  across the floor (+1 px) and leaves the already-exact class untouched. Floor-tune
  biases against measured fractions instead of nudging whole logical pixels.

- **Don't bake a phase — bake at absolute device coordinates, and snap the draw rect to
  what Yoga can express.** (element two-hop bake, options_controls 7.73→1.29) Evaluating
  origin's full int chain per absolute device pixel (out-of-box → transparent) is exact by
  construction and composites over the two-hop backdrop since both share src=int(dx/s).
  Draw side: Yoga rounds to whole LOGICAL px, so at scale 1.5 only even logical coords are
  device-integral — snap the texture rect outward to such coords and absolutely position
  the box (Yoga abs inset = parent border edge, padding excluded). Drive each correction
  by per-region ink-mask cross-correlation, not eyeballing.

- **Measure geometry before touching layout code.** lobby_connect's "button shift" was a
  misdiagnosis from one eyeball pass — ink-mask bbox + cross-correlation showed the row was
  already centered (origin's floating row escapes its parent padding). The options fix went
  the other way: pill-bbox profiling found the real 3px pitch error in minutes and origin
  source confirmed the constant (kButtonGap 19). numpy masks first, edits second.
- **The cppx transpiler rejects comments inside JSX attribute initializer braces** —
  put comments above the element, not inside `layout={{...}}`.

- **Hot tiles cluster by family — diagnose the family before fixing screens.** Iteration 0:
  13/13 screens failed the tile gate, but the worst tiles in 8 of them are the same Mars
  backdrop region. Measuring one tile's row profile revealed a single systemic cause
  (striping arithmetic), not 13 layout bugs. Always compare worst-tile coords across screens
  before opening per-screen work.
- **Region means can match while tiles read 30%.** The tolerant gate at 640×360 still sees
  scanline phase mismatch as huge diffs even when 78px-region means agree within ±2 RGB.
  A hot tile therefore doesn't always mean misplaced/wrong-colored content — dump per-row
  profiles (numpy) before trusting the % as a layout signal.
- **The pixdiff noise-ceiling calibration (~9.6%) did not include real backdrop striping**
  (reads 20–33%/tile). Don't waive these tiles (rule: >10% never waivable) and don't relax
  the gate; resolve the striping design question explicitly instead.
- **cap_menus.sh/cap_lobby.sh boot their own Silencer instances** — don't run them
  concurrently with the e2e suite; serialize capture → e2e.
- **`pkill -f Silencer` (the goal's build preamble) kills EVERY instance** — including
  e2e instances owned by parallel repair agents. While subagents run e2e, skip the pkill
  or scope it to the capture's own PID.
- **Origin's image fit modes are framebuffer-relative.** PackImage(6,0)=cover happens in
  origin's 640×480 element space, and the framebuffer then stretches non-uniformly to the
  window — so at the glass, every full-bleed backdrop is a STRETCH. The migration
  transcribed "cover" literally and got uniform 3× scaling + crop = wrong geometry AND
  wrong scanline arithmetic on 8 screens. When porting legacy draw flags, trace the full
  chain to the glass, not the flag name.
