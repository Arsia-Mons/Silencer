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

- **Simulate the bake in numpy before writing the C++.** (string-variant bake) Collapsing
  the golden label through int(gx/s) and fitting run boundaries on the render proved, in
  ~20 minutes, that (a) the golden obeys the whole-frame magnify exactly, (b) the render's
  per-glyph float pen (advance*2.25 = 24.75) fits NO single column phase — x-phase drifts
  WITHIN a string — and (c) round(dev/s) recovers the golden virtual pen. That killed the
  per-glyph-atlas-rows design on evidence and made the per-string bake a safe bet.

- **A phase-exact bake converts "fuzzy ±1 + wrong pattern" into "byte-exact or 2-3px off"**
  — binary outcomes you can finish. After the string bake, every label was either 0.0000
  or shifted one whole virtual cell (pen 1-2 device px outside the round-recovery window);
  the json layout dump (px = floor(x*1.5), slack vs cell) located each offender in minutes,
  and ≤1-logical nudges (wrapper mr, ml on the text, inset-top) closed all five menu
  screens to byte-identical. Watch compound moves: shifting a row -1 for its label flipped
  the row's right toggle out of ITS window (2.25-px windows; recompute every sibling).

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

- **The texture registry cap fails SILENTLY into raw draws.** Per-phase variants
  accumulate per (position, size) across a session; a 13-screen capture run
  exceeded the 64-texture cap and LATER screens lost their bakes — game_staging
  "regressed" with zero code changes to it. When a previously-PASS screen drifts,
  check resource caps before bisecting layout.
- **Pen-grid rule: authored logical pen = round(1.5 × origin virtual pen).** The
  string bake recovers vy from round(floor(dev)/2.25); .5-logical positions round
  unpredictably through Yoga, so land pens on integers via that rule and verify
  with the inspect JSON (floor(1.5L) must fall inside the cell's ±1.125 window).
- **Origin's int-cast floats decide cells: int(x), not round(x).** Clay centers
  produce .5 virtual coords; the compositor int-casts them (284.5 -> 284 for the
  alias modal, 166.5 -> 166 for plates) while OUR recovery rounds. Every centered
  origin element needs its golden cell measured from ink, not derived.
- **Shift-test (np.roll argmin over dx,dy) classifies a hot region in seconds**:
  dy=±2 with low residual = one virtual cell off (pen/grid); dx=dy=0 with high
  residual = pattern family (unbaked sprite/nine-slice/brightness variant).
- **Same-sprite, different draw mode = different flavor.** bank7 idx18/19 appear
  1:1 (roster), brightness-dimmed (tech grid), and the row plate appears 1:1
  (cc roster) AND stretched (agency rows): one base_id carries ONE flavor, so a
  stretch flavor that degrades to 1:1 for equal sizes covers both call sites.
- **origin Chrome buttons never swap art on focus** (SpriteIndexForFrame returns
  idx24 for all phases; only brightness ramps) and golden captures show focused
  controls at plain idle — focus_visible must not change the sprite.

- **Harness "flaky visual" diffs are usually capture-STATE, not rendering.** Scenario
  71 red on lobby_connect/cc_alias was diagnosed as port nondeterminism + "caret
  blink phase"; the real causes were capturing before the connect handshake populated
  the status log, and capturing AFTER set_text while the golden's field is empty
  (no blink exists — the caret draws unconditionally while focused). Before adding
  determinism machinery, diff the harness's drive sequence against the capture
  script that produces the passing standalone render (cap_lobby.sh) step by step.

- **A capture patch that's safe for one screen family can corrupt another through
  shared mutable state.** The deterministic-fade patch (menu-golden era) made
  ApplyPaletteFade(fadeOut=true) write brightness-0 into the SHARED 256-entry
  temppalette in one shot; the in-game ambience path only rewrites indices 2..114,
  so every in-game capture re-presented the stale black entries for all UI/text
  colors — HUD invisible, world fine. In-game goldens need the PRISTINE binary
  (fade is irrelevant there: captures happen minutes past it). Trace a capture
  patch's writes through every consumer of the buffer it touches.

- **`step --frames N` is only exact for small N; feedback-step to a world counter
  instead.** Multi-frame steps overran by ±1-2 ticks (wall-clock catch-up race in
  origin's sim loop). Converging on message_progress (`+1/tick, wraps mod 256`)
  with bulk steps that stop 30 short + exact single-step finish lands on a UNIQUE
  absolute sim tick — two independent tutorial runs were byte-identical outside
  rand()-driven rain. Never trust open-loop step counts for golden anchors.

- **Enumerate what ISN'T sim-deterministic before declaring a capture flaky:**
  origin's in-game frame has exactly three wall-clock leaks — rain (C rand(),
  call count depends on elapsed menu frames), minimap dot blink (renderer state_i
  parity), chat caret (SDL_GetTicks/50). Pause freezes the sim AND the renderer
  phase (state_i ticks with the sim), message pulses key off message_i — so
  everything else in the frame is exact. Mask the three leaks; gate the rest hard.

- **Porting origin's responsive arithmetic can preserve byte-identity if the
  logical mapping is pinned to the recorded constants.** resolve_lobby_panes runs
  origin's integer virtual-space math (RoundRatio/ScaleLegacyPx) and converts with
  exactly two rules — pane sizes floor(v*1.5), gaps/pads lround(v*1.5) — chosen
  because they reproduce every authored golden-cell constant at the design canvas
  (777/463/757/436/180/20/19). Verify the mapping against ALL authored values
  before wiring; a single uniform rounding rule does NOT exist (463.5 floors,
  19.5 rounds up).

- **The follow-camera's rest position is render-cadence-dependent — pin it, don't chase
  it.** Camera::Follow has a 100px y-hysteresis (h=100, yoffset=30): the endpoint after
  the spawn fall depends on how many render frames sampled the fall, so a heavier UI
  changed our camera endpoint vs origin's at the SAME sim tick (world content ~100px
  off; sim itself byte-matched). Fix: a `camera` control op + per-capture phase
  correlation of the world band against the golden (converges in 1-2 iterations).

- **Rain can't be sampled away — disable it at the source for golden gating.** Median
  capture (5..9 frames over up to 3s) still leaked slow parallax streaks and the
  marginal tiles flapped around the 5% line run-to-run. The `rain` control op (skips
  DrawRain/DrawRainPuddles) makes our render deterministic; the goldens' own frozen
  rain is absorbed by the documented masks + tile tolerance (worst tile dropped 5.8 →
  ~2.5%).

- **A conditionally-mounted Input means conditional hooks — wrap it in its own
  component.** The chat compose Input's internal use_state ran inside InGameScreenView
  only while chat was active; hook slots shifted across frames and the Input subtree
  intermittently failed to mount (probes fine, full captures missing the typed text).
  Fiber-wrapping the compose row (::ui::component) made hook order stable. Symptom
  signature: a subtree present in one run's `inspect` and absent in the next, with no
  errors logged.

- **"Invisible" focusable boxes still wear the theme — clear paint with
  chromeless(true), not just background/border/outline.** The ghost buy/tech row
  targets painted a sub-TOL stipple-parity artifact through the Box role's default
  paint that turned tech_overlay from byte-identical to 505 px off; clearing individual
  paint fields wasn't enough.

- **origin `scaled=true` on TeamEmblem means DOWNsample.** Renderer::DrawScaled(factor
  2) SKIPS every other pixel (half size), it does not magnify; the team-strip emblem is
  the bank-181 sprite at half res (the cc screens scale the same art UP via Contain).
  Read the blitter before trusting a flag name — same lesson as PackImage cover.

- **origin's translucent UI fills are palette-INDEX mixes.** Clay backgroundColor.r is
  a palette index and the compositor routes each pixel through the alpha LUT
  (AlphaSrcIndex + Palette::Alpha), quantized to the palette. Linear RGB blending of
  the F1 player-list dim read 30%/tile; reproducing the LUT in the headless composite
  (and mapping black to index 0 — the low reserved rows are real mixes) made it exact.
  Same family: DrawAlphaed ammo digits = Alpha(glyph, well_index) baked per glyph.

- **At s=1 (in-game 640x480) the two-hop problem vanishes but float rects still
  matter.** Sprites draw 1:1 with no phase variants needed; the only correctness rules
  are (a) author logical = ceil(1.5·d − .25) so device floors recover d, and (b) snap
  near-1:1 image dst rects to integers in the executor — a w+1/3 rect bled an extra
  stipple row/column on parity-sensitive art.

- **A "replicated" object can silently never replicate — check requiresauthority before
  trusting any snapshot-driven UI state.** GameStateObject's header claims replicas get
  it via snapshot, but Object defaults requiresauthority=false and the ctor never set
  it: SaveSnapshot skips it entirely, so winningTeamId/matchPhase NEVER reach clients
  (and GSO::Tick on replicas would clobber them anyway by recomputing from the local
  never-set world.winningteamid). Result: a time-limit match end is unreachable on
  origin lobby clients — they drop to LOBBY via CONNECTION LOST when the server exits.
  The secrets path works only because Team::Tick runs on replicas (ProcessSnapshotQueue
  → TickObjects).

- **Control the AUTHORITY's lifetime, not the replica's clock, when driving a
  client to a timed state.** The mission-summary drive needs the client's own
  message_i to reach 240 AFTER winningteamid arrives, but the dedicated server quits
  ~7s after its [stats] registration and its disconnect resets the client's message
  cycle (CONNECTION LOST, type 20) — connection-loss then always wins at 48 < 240.
  SIGSTOP-ing the server process on the lobby's [stats] log line freezes snapshots
  (harmless — the result already replicated) while the client's local messaging keeps
  ticking; the 10s peer timeout loses to the ≤10.6s worst-case cycle ~94% of runs.

- **GAS is file-local, not fetched: the built bundle's Contents/assets/gas/*.json is a
  zero-rebuild lever for both the client AND the lobby-spawned dedicated server** (same
  bundle, CDResDir chdir). Edit + restore in the capture script; gameModes id 0 (Data
  Retrieval) is what created lobby games run.

- **The +0.375/+1.125-device frame bias only matters where a cell's FIRST device
  row/col carries ink.** mission_summary authored at raw (sx-5,sy-19)×1.5 inside the
  cc chrome_panel frame came out byte-identical everywhere except the ovals: text
  recovery tolerates ±1 device px (glyph leading), but a sprite-cell texture origin
  one px below/right of its cell start loses a 1px sliver, and a .5-logical x that
  Yoga rounds UP lands the whole box one virtual cell off (+2 device). Two sub-logical
  nudges (-0.5x, -0.75y) on the oval boxes alone closed the screen to 0.0000.

- **use_hovered()/use_focused() are useless in a PRODUCT component that wraps a
  substrate control — the host node's fiber is the substrate's.** AppButton (its own
  fiber via JSX lowering) wraps Button (another fiber) which commits the host: the
  InteractionSnapshot keys by the host's fiber_id = Button's. Measured: hovered
  Button → use_hovered()==false inside AppButton. Interaction-driven ANIMATION in a
  product component needs edge callbacks instead — focus/blur existed; on_hover
  enter/leave was added (ClientUi dispatches on hovered-node change). Hover also does
  NOT move focus in the cppx model (scenario 16 — origin's hover-focuses model was
  deliberately replaced), so "ramp on focus" alone would never fire for the pointer.

- **The per-screen-layer frame-provider lambda silently zeroed use_clock deltas.**
  The Clock value was computed inside the provider chain, which runs once per visible
  screen LAYER (base + overlays): the second run in the same millisecond reset
  last-ticks and published delta≈0, so any delta-driven animation froze on screens
  with an overlay (the mainmenu logo only worked because mainmenu is single-layer).
  Per-frame values must be snapshotted once in RenderCppxClientUiFrame, not in the
  per-layer lambda.

- **Autofocus vs origin's no-focus rest state: gate ramps on the focus SOURCE.** Our
  screens autofocus for keyboard nav (origin never focuses anything until input);
  ramping on any-focus would light a button in every rest capture. Gating the ramp on
  hover-edge OR focus with source ∈ {Mouse,Touch,Keyboard,Gamepad} (Programmatic
  excluded) keeps every rest golden byte-stable while keyboard/pointer interaction
  ramps like origin.

- **The TUI ACTION channel makes "needs real gameplay" surfaces capturable.** The
  input server (net/inputserver.cpp) accepts full action-bit snapshots (move/jump/
  jetpack/use/fire/weapon bits), cached latest-wins — under pause+step every input
  lands on an exact sim tick, so the whole tutorial curriculum (run→jump→jetpack→
  crouch→roll→disguise→fire→weapon→base build→buy→rocket) replays deterministically.
  Prior "no deterministic headless trigger" waivers for status_lines/secret_overlay/
  system_camera/top_ticker were all wrong; only data-dependence (trace time's 4-
  terminal drain) remains expensive.

- **NPC behavior trees share the C rand() stream with wall-clock rain** (civilian
  .cpp:61, behaviortree.cpp:81): any civilian/guard near a capture region wanders
  nondeterministically across runs AND across binaries (our rain-off changes the
  draw count). Mask wandering NPCs like rain; don't chase their pose.

- **origin's center-message brightness is Uint8 arithmetic — it WRAPS, not clamps.**
  `brightness -= (mp - time + 8) * 8` at mp-time+8==32 subtracts 256%256=0, so the
  "faded out" message renders at FULL brightness at high mp. Porting it as clamped
  int left our message black (then red via unbaked-face fallback) where origin shows
  bright text. Port legacy Uint8 math with explicit wraps, never "sane" clamps.

- **A frozen UI layer mid-capture = retained-tree commit failure, not staleness.**
  The center-message reveal emits 2 nodes per revealed glyph onto the flat hud Box;
  a 75-char message blew UI_RETAINED_MAX_CHILDREN(128)/NODES(512) and every commit
  failed from that frame on — screenshots showed the last-good frame (message stuck
  mid-reveal) while world_state advanced. Grep the run log for "failed to commit" /
  "too many children" BEFORE debugging snapshot plumbing; caps raised to 512/1024
  (drawlist budget 4MB).

- **origin's in-game palette has HISTORY: the ambience repaint stamps stale
  temppalette.** CopyWithBrightness(colors, level, 2, 114) writes only 2..114 of the
  SHARED temppalette and SetColors applies ALL 256 — the >114 (UI/parallax) entries
  re-present whatever the last full-range write left (the boot fade at phase 14 =
  112/128). First base entry changes the ambience level, fires the repaint, and dims
  every UI index by 12.5% for the REST OF THE SESSION (measured: 8.8k px at exactly
  ratio 128/112 in the syscam golden). Same bug family as the fade-patch lesson. Any
  post-base-entry origin golden carries it; our page-color-baked RGBA HUD can't
  follow live palette mutation — that gate is waived (PARITY.md), and a trace-time
  golden would need a no-base-entry drive to dodge it.

- **queue_deferred_mutation only survives if queued DURING the frame** —
  begin_frame clears the deferred queue, so a mutation queued from pre-frame
  code (control dispatch, the pipeline's input pre-pass) silently vanishes.
  Event handlers (on_change/on_activate) run inside the frame and may queue;
  pre-frame game-side code must mutate the world directly (safe — the
  no-mutation rule guards mid-build only).

- **Headless gates can't see window-only render paths — audit every
  `if(window)` fork against the capture path.** The in-game UI rendered at
  window-pixel res (uniform scale) while origin stretches its one 640×480
  screenbuffer non-uniformly; every gate passed because headless has no window
  and silently took the correct surface-size branch. The divergence only
  exists at a real window, where a 612-virtual HUD anchor sat at device 1377
  instead of 1836. When the capture path and the user path fork on
  window-presence, the gate is gating the wrong branch.

- **This host can't open SDL windows without a console login.** No WindowServer
  session for uid 501 (/dev/console owned by root): direct launch fails with
  "video driver did not add any displays", SDL_VIDEODRIVER=offscreen gets a
  window but no SDL_GPU backend, and `open`/System Events can't reach a GUI
  domain. Windowed verification scripts (cap_ingame_windowed_verify.sh) need a
  logged-in console session — don't burn cycles on launch workarounds.

- **align_camera stuck at a CONSTANT shift = the correction is outside the
  follow window, not a convergence problem.** Camera::Follow's x-window is
  ±7 (w=15, xoffset=0): the per-render-frame Follow snaps any pinned x back
  inside it, so a needed correction beyond ±7 repeats the same measured shift
  forever (observed 10,0 under full-suite load). More iterations can't help —
  tear down and re-drive (the rest position re-rolls), bounded retries.

- **Pointer click = on_activate on ANY focusable in the engine — gate inputs.**
  Every submit-on-activate text field (password modal, lobby connect, chat
  compose) fired its submit when set_text's synthetic focus-click landed.
  origin inputs submit on RETURN only; fixed at the substrate
  (FocusRuntime.confirmed_by_pointer + ClientUi skips invoke_activate for
  Input-role pointer confirms). Symptom signature: a modal that closes itself
  the moment a script set_texts into it.

- **Prove a code path is input-reachable in ORIGIN before building a drive to
  exercise it.** The buy/tech window-follow (scrolled = selected−4) looked like
  a missing feature, but origin registers interactables for the VISIBLE rows
  only and has no in-game Scroll handler — no input can move the selection past
  the window in origin either. The complete close-out was a verbatim source
  port + asserts over the reachable subset (window pinned at 0, selection
  clamps), not an expensive 20-row gameplay drive that would assert the same
  zeros.

- **Wheel parity needs no scrolled-state golden when step arithmetic + row
  content are asserted through the control socket.** A scrolled capture would
  re-test the same row pixels the byte-identical rest golden already gates;
  what scrolling changes is WHICH rows are in the viewport — asserted exactly
  via in-view row indices + first-row label per notch and at both clamp ends
  (scenario 82), and via first/last visible line for mission_summary
  (scenario 74's post-capture probes).

- **`world_state.peers` ≠ peerlist length.** In a lobby match vs a dedicated
  server the client reports peers=1 while `peerlist` carries both the local
  peer and the dedicated AUTHORITY peer (id 0, accountid 0) — assert peer
  topology from `peerlist` + `authoritypeer`, not the scalar.
