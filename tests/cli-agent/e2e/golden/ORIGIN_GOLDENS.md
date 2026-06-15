# Origin/main golden baselines — provenance

> ## Uplifted goldens (2026-06-12 — U-7 / SIL-51)
>
> `message_modal.png` and `password_modal.png` were superseded after restoring
> the origin fixed-dialog layout contract for the cppx-only modal baselines
> (`uplift: restore fixed-size message/password modal containers`, UPLIFT.md
> finding U-7, SIL-51). These screens have no standalone origin trigger, but
> origin source sizes the bank-40 modal element explicitly (`352x178` for
> message, `352x148` for password) and draws `PackImage(40,4)` /
> `PackImage(40,2)` into those boxes; the prior cppx baselines instead let the
> raw registered sprite dimensions size the whole modal, so the text overflowed
> outside the frame. The fix pins the dialog boxes to those origin constants,
> uses cover-fit dialog art, wraps the message text inside the padded content
> width, restores the password input to `180x14`, and uses the fixed Chrome
> OK plate. Diff attribution: BEFORE captures byte-matched the prior baselines;
> BEFORE→AFTER changed only the modal regions
> (`message_modal` bbox x663-1247/y406-672, `password_modal` bbox
> x696-1223/y429-650), with the mainmenu backdrop/logo outside those boxes
> stable. Evidence: `docs/plans/uplift-evidence/U-7/`.

> ## Uplifted goldens (2026-06-12 — U-5 / SIL-207)
>
> `create_game.png` was superseded once more after fixing the second PORT-side
> defect the parity gate had passed sub-tolerance and the U-1/2/3 supersessions
> enshrined (`uplift: create_game scrollbar thumb 1px short of track`,
> UPLIFT.md finding U-5, SIL-207). Like U-4 this CONVERGES back toward
> origin's correct rendering: the Game Options scrollbar thumb is a flex Box
> whose solid fill rasterized as a raw quad at fractional logical edges, while
> the track strokes around it snap to origin's virtual cell grid
> (`snap_legacy_hairline_border`) — the unsnapped fill landed half a virtual
> cell up-left of its cell (device rect x1156-1168 / y211-367: a 1px black
> gutter against the track's right stroke at x1169 and a 1px overpaint of the
> left stroke's inner column x1156 and the top stroke's inner row y211; origin
> renders x1157-1169 / y212-368, flush all round). Fix: the executor now snaps
> eligible solid Rect fills to the same virtual cell grid as the hairline
> strokes (`snap_legacy_solid_fill`, draw_executor.cpp — square corners, the
> two legacy stroke palette colors, quarter-integer virtual scale; the thumb is
> the only such fill today), so fills are flush with snapped strokes by
> construction. Diff attribution (this supersession's gate): BEFORE captures
> byte-matched all 8 lobby-cluster goldens; the BEFORE→AFTER diff is 338 px
> with bbox exactly the thumb region (x1156-1169, y211-368); AFTER minus
> pristine-origin (ba345131) in the scrollbar region equals BEFORE's residual
> set exactly (the documented U-3 backdrop texels) — the thumb itself is
> byte-equal origin; every other screen byte-stable (suites
> 70/71/72/74/75/76 + 12/17/31/53 all PASS; evidence:
> `docs/plans/uplift-evidence/U-5/`). Pre-U-5 baseline remains in git history
> (commit a560a5b4 and earlier).

> ## Uplifted goldens (2026-06-12 — U-4 / SIL-206)
>
> `cc_select_agency.png` was superseded a fourth time after fixing a
> PORT-side defect the earlier supersessions had enshrined (`uplift:
> cc_select_agency agency ovals — right cap sheared, ring broken`, UPLIFT.md
> finding U-4, SIL-206). Unlike U-1/2/3 this diverges from the old golden by
> CONVERGING back toward origin's correct rendering: the agency-rows wrapper's
> `-1.5/+3.5` margin nudge netted the grow-to-pane List rows 2 logical px
> narrower than the pane's 354 (= 236 virtual), so the Stretch row-plate bake
> nearest-decimated 236 source columns to 235 and dropped the right cap's
> outermost ring column — the stadium ring rendered OPEN at every oval's
> vertical middle (origin and the roster rows render it closed). The fix
> (`character_create.cppx`) keeps the −1.5 one-virtual-col left shift but
> makes the right margin symmetric (+1.5) so the box keeps full 236-virtual
> width and the bake is 1:1. Diff attribution (this supersession's gate):
> all 3385 changed px lie in exactly five row bands = the five agency oval
> plates (y229-253/301-325/373-397/445-469/517-541, x ≤ 903); the AFTER
> right-cap strip is byte-identical to the roster-row oval's in
> `character_create.png` (same canonical bake, position-independent per U-2);
> text, backdrop, and every other screen byte-stable (suites 70/71/72/74/75/76
> all PASS; evidence: `docs/plans/uplift-evidence/U-4/`). Pre-U-4 baseline
> remains in git history (commit d8936dda and earlier).

> ## Uplifted goldens (2026-06-12 — U-3 / SIL-205)
>
> 14 menu/lobby baselines (the 13 minus `lobby_connect.png`, which has no
> visible backdrop and stayed byte-identical, plus `mission_summary.png` and
> `hover_mainmenu_oval.png`) were superseded a third time by cppx renders
> after the single-hop backdrop resample (`uplift: backdrop double-scaling
> banding`, UPLIFT.md finding U-3, SIL-205) — the backdrop arm of the same
> whole-frame-magnify artifact family as U-1/U-2. Origin nearest-resampled
> full-bleed backdrops TWICE (sprite → 853×480 virtual canvas → ×2.25 frame
> magnify), compounding to irregular {2,2,5} duplication runs: the mainmenu
> planet's authored scanlines rendered as 2/2/5-px bands, the same 1-src-px
> star rendered 2×2 / 5×5 / 2×5 / 5×2 by position, and column x1919 was never
> filled. The fix resamples once at device geometry (cover @1080p = uniform
> 3×3 blocks, centered crop; stretch keeps the unavoidable single-hop
> {3,2,2,2} vertical at 2.25; all 1920 columns filled). Diff attribution
> (this supersession's gate): every changed pixel is backdrop-visible —
> mainmenu/lobby_screen changed px proven src-equivalent to the same source
> texels (99.8%/99.2% strict set-membership; remainder = UI-edge occlusion +
> the lobby panel-border blur over the changed backdrop), hover_mainmenu_oval
> changed px byte-equal mainmenu's at every coordinate, magenta overlays show
> chrome/text/layout untouched (evidence: `docs/plans/uplift-evidence/U-3/`).
> The cppx-only baselines (`gallery`, `message_modal`, `password_modal`) were
> re-blessed for the same reason (backdrop visible behind modals/gallery).
> **In-game goldens remain pristine origin captures** (no menu backdrop;
> suites 72/76 pass unchanged). Pre-U-3 baselines remain in git history
> (commit 101a83a8 and earlier).

> ## Uplifted goldens (2026-06-12 — U-2 / SIL-204)
>
> The same 15 baselines (13 menu/lobby + `mission_summary.png` +
> `hover_mainmenu_oval.png`) were superseded a second time the same day by
> cppx renders after the canonical-phase SPRITE bake (`uplift:
> position-dependent sprite striping`, UPLIFT.md finding U-2, SIL-204) — the
> sprite arm of the same whole-frame-magnify artifact U-1 fixed for glyphs.
> Origin's chain striped every chrome sprite (ovals, toggles, logo,
> nine-slice buttons, emblems, plates, dialog/panel frames) by its absolute
> position — the mainmenu Options oval was even a different SIZE (75 px) than
> its three siblings (74 px). The fix bakes each registered legacy sprite
> once at phase 0 and draws it at floor-quantized device cells: same sprite =
> byte-identical pixels + identical size everywhere. Per-screen diff
> attribution (this supersession's gate): every changed pixel sits on legacy
> chrome sprites; glyph text and backdrops are byte-stable (evidence:
> `docs/plans/uplift-evidence/U-2/`, incl. magenta diff overlays). The
> cppx-only baselines (`gallery`, `message_modal`, `password_modal`) were
> re-blessed for the same reason. **In-game goldens remain pristine origin
> captures** — the s=1 path is byte-identical by construction (suites 72/76
> pass unchanged). Pre-U-2 baselines remain in git history (commit 207239db
> and earlier).
>
> ## Uplifted goldens (2026-06-12 — U-1 / SIL-190)
>
> The 13 menu/lobby baselines below, plus `mission_summary.png` and
> `hover_mainmenu_oval.png`, are **no longer raw origin captures**: they were
> superseded by cppx renders after the canonical-glyph-cell fix
> (`uplift: position-dependent glyph striping`, UPLIFT.md finding U-1,
> SIL-190). Origin composited menus at virtual resolution and whole-frame
> nearest-magnified, striping every glyph by its absolute screen position; the
> fix renders each glyph in a canonical integer device cell (same letter =
> byte-identical pixels everywhere), deliberately diverging from origin on
> glyph-text pixels ONLY. Per-screen diff attribution (this supersession's
> gate): every changed pixel sits in a text-line band; chrome, sprites,
> backdrops and layout are untouched (evidence:
> `docs/plans/uplift-evidence/U-1/`). The pre-uplift origin captures remain in
> git history (commit a560a5b4 and earlier).
>
> The cppx-only baselines (`gallery`, `message_modal`, `password_modal`) were
> re-blessed the same day for the same reason. **In-game goldens are still
> pristine origin captures** — the fix's floor pen quantization reproduces the
> legacy 1:1 text rendering exactly (scenarios 72/76 pass against them
> unchanged; `ingame_tech_overlay` still gates 0.0000).
>
> These uplifted baselines stay bless-protected: future re-blesses need the
> same per-pixel diff attribution + an UPLIFT.md finding, never a green-making
> copy.

These PNGs are the **visual parity targets**: the real origin/main design that the
cppx UI must match. They are NOT cppx renders.

## Source & method
- **Source:** `origin/main` @ `af4c50c5` (Release **v00058**).
- **Captured:** 2026-06-07, fresh, at **1920×1080** (per-single-screen; user
  requirement) via the headless control port (`clients/cli/index.ts` `screenshot`),
  driven through the real screens (menus, Go lobby login + character-create,
  dedicated-server staging). Re-captured from the prior 960×720 baselines.
- **Fade disabled for determinism:** the `origin-capture` worktree
  (`.worktrees/origin-capture` @ af4c50c5) patches
  `GameRenderer::PaletteFadePhaseFromClock()` → `return 16` (full brightness, no
  mid-fade dim frames). Uncommitted; the capture binary is built there.
- Capture scripts (regenerated 2026-06-07, in `/tmp`): `cap_origin_menus.sh`
  (menus) + `cap_origin_mm_stable.sh` (mainmenu logo-stability poll — waits until the
  bank-208 SILENCER reveal HOLDS, i.e. logo green-px == previous read, because the
  reveal is wall-clock-driven, NOT frame-count-deterministic; a fixed wait catches a
  scrambled mid-reveal frame); and `cap_origin_lobby_final.sh` (lobby cluster; copies
  maps into the lobby `-maps-dir`, selects a map + Login/Create → STAGING → Choose
  Tech → tech_select).
- **Origin control labels differ from cppx:** origin `inspect` returns `widgets`
  (not cppx's `nodes`) keyed by `id` (`options.audio`, `lobby.game_create.create`,
  `lobby.game_join.choose_tech`, …); navigate by visible label ("Audio"/"Create"/
  "Choose Tech"). cppx cap scripts reading `r.nodes` find nothing against origin.

## Authentic origin/main baselines (13)
mainmenu, options, options_audio, options_display, options_controls,
lobby_connect, character_create, **lobby_screen** (captured as `lobby`),
cc_alias, cc_select_agency, create_game, game_staging, tech_select.

## In-game goldens (7) — captured 2026-06-11, 640×480
`ingame_hud_base, ingame_chat_open, ingame_chat_history, ingame_player_list,
ingame_buy_tech, ingame_messages, ingame_tech_overlay`

(The 8th, `ingame_quit_prompt`, was RETIRED: the baked HUD "Hit Enter To Quit"
prompt + the raw-scancode `world.quitstate` machine were deleted. The in-match
cancel affordance is now the UI-layer PauseScreen overlay, covered behaviorally
by `91_cancel_back_router.sh`.)

(The earlier "HUD doesn't capture headless" deferral note was WRONG: origin renders
world + HUD software-side into the 640×480 screenbuffer every frame; `--headless`
only skips Present. The control-port `screenshot` op captures HUD included.)

- **Source:** `origin/main` @ `af4c50c5` (v00058), built **PRISTINE** — the
  deterministic-fade patch was stashed for these captures and restored after.
  The patch corrupts the in-game palette: its FADEOUT branch writes brightness-0
  into the shared `temppalette` across all 256 entries, and the in-game ambience
  path (`game_loop.cpp:179` → `CopyWithBrightness(colors, ambience, 2, 114)`)
  only rewrites indices 2..114, re-presenting the stale black entries for every
  HUD/text color index → all in-game UI renders near-black. Pristine is safe
  here because in-game captures happen minutes past the wall-clock fade.
- **Resolution:** native **640×480** (in-game the screenbuffer is pinned to
  kLegacyRender 640×480 even headless). Do NOT upscale; gate both sides at 640×480.
- **Scene:** Tutorial single-player mission, map `AGENCY04.SIL` (night/rain bridge),
  default keymap (the tutorial message embeds key display names: "By tapping Left
  and Right."), spawn pinned at (2784,1350) (`RandomPlayerStartLocation` is
  empirically constant on this map; the capture script asserts it).
- **Capture script:** `tools/cap/cap_ingame_origin.sh` (this repo). Drive:
  MAINMENU → click Tutorial → SINGLEPLAYERGAME → `pause` → feedback-stepped
  (`step --frames N` + `world_state` message_progress convergence; multi-frame
  steps overrun by ±1-2 ticks so all anchors single-step the final approach) to
  the **anchor**: first ticks of tutorial message 2 ("Move your agent left and
  right..."), which loops forever without player movement. Anchors:
  - base tick = first `message_i == 0` after the anchor (center message hidden,
    wraps mod 256): `ingame_hud_base`, `ingame_player_list` (`ingame_ui_mode
    playerlist`), `ingame_buy_tech` (`ingame_ui_mode buy` — rows Laser/Rocket,
    footer "Available Credits: 500"), `ingame_chat_open` (`ingame_ui_mode chat`
    + `set_text` "parity check" on uid of `ingame.chat`; caret-ON enforced by
    blink-pair compare), `ingame_chat_history` (two `--chat-line` pushes
    "Recon: parity check one/two" + Esc closes input; display ticks frozen >0).
    All overlays toggled while PAUSED — renders advance, sim doesn't, so every
    overlay sees the identical world tick.
  - `ingame_messages` = base tick + 94 (`message_i == 94`, both lines fully
    revealed, brightness deterministic — message pulse keys off message_i, not
    the wall-clock-seeded renderer phase).
  - `ingame_tech_overlay` = next `message_i == 0` (`ingame_ui_mode tech` —
    creates/joins a team and teleports the player to the team base, so it is
    captured LAST; base interior has no sky → no rain → fully deterministic).
- **Nondeterministic regions (measured: 2 independent full runs, numpy diff):**
  - **Rain streaks** — sparse 1-2 px diagonal dashes anywhere in the world view
    (y0-~400), including THROUGH the transparent interiors of the chat/buy/
    player-list panels and behind message text (~1300-3000 px per frame, ~0.5%).
    Raindrops use C `rand()` whose call count is wall-clock-dependent
    (renderer.cpp:45,282) — unfixable without a capture patch, and pointless to
    fix since cppx rain can never align pixelwise anyway. MASK rain when gating.
  - **Minimap inset, rect x235-406 y419-479** (172×62 blit at 235,419) — live
    world view incl. rain, plus dot blinks keyed to the wall-clock-seeded
    renderer phase (`state_i % 2`, renderer.cpp:1195).
  - **Rain-impact puddle ripples** on the bridge deck — same rand() source as
    the streaks; they concentrate in the deck band **y296-340** (full width).
  - Everything else is **byte-identical across runs** (ingame_tech_overlay
    differs ONLY in the minimap inset: 42 px).
- **Gate masks (scenario 72 + tools/cap usage):** `pixdiff_tolerant.py --mask`
  rectangles `235,419,406,479` (minimap inset), `0,296,640,340` (ripple band),
  `624,0,640,419` (right-edge sliver: the 640px width leaves a half-width
  16px tile column where sparse rain reads at double density). The cppx
  capture additionally disables OUR rain layer (`rain` control op) so only the
  goldens' frozen streaks remain in the diff (~0.2% global, sub-tile).
- **NOT captured** (PARITY.md tracks): hud_trace_time (needs beaming =
  team.secretprogress >= 180, i.e. fully draining 4+ data terminals across the
  map), scoreboard-style F1 with multiple teams.

## Gameplay-driven in-game goldens (4) — captured 2026-06-11, 640×480

`ingame_top_ticker, ingame_status_lines, ingame_secret_overlay,
ingame_system_camera`

- **Source:** `origin/main` @ `af4c50c5` (v00058), PRISTINE build (both
  capture patches stashed). **Capture script:**
  `tools/cap/cap_ingame_origin_extra.sh` (+ `tools/cap/ingame_drive_lib.sh`):
  same paused/feedback-stepped tutorial session as the 8 above, plus TUI
  ACTION snapshots (net/inputserver.cpp keymask protocol) driving real
  gameplay — the earlier "needs real gameplay actions" deferral was wrong;
  the TUI input port replays the whole tutorial curriculum deterministically.
- **Drive:** anchor → F4 scancode edge → `ShowTopMessage("          *MUSIC
  PAUSED*")` (headless audio disabled ⇒ `MusicPaused()` always false, text
  stable) → **ingame_top_ticker** at topmessage_i==2, message_i==0. Tutorial
  cases 0-8 (run/jump/jetpack/crouch/roll/disguise×2/fire/weapon-switch);
  case 9 grants INV_BASEDOOR. Walk to the deck data terminal at x=1824
  (parsed from AGENCY04.SIL actor records, id 54) → keyuse → CanCreateBase
  fails on TERMINAL-in-AABB (player.cpp:3097) → "Can't build a base here"
  (color 208) → **ingame_status_lines** 92 ticks after the push (status
  time==8, mid-fade brightness 64; trigger pinned at message_i==60 so the
  capture lands at 152). Walk clear (x≈2100) → keyuse → base door built →
  **ingame_secret_overlay** at message_i==0 (secret panel bank 187 idx0 + 9
  dim hack lines, secretprogress 0). Enter base, buy Rocket ×2 at the
  inventory station (control-op row click = ActivateBuyTechSelection), exit,
  fire from open deck at message_i==230 → **ingame_system_camera** ~7 ticks
  into the rocket's flight (slot-0 "SYSTEM" frame bank95 idx2 + live inset).
- **Determinism:** two independent runs (ports 63554/63574) differ only in
  sparse rain dashes + the minimap inset (same classes as the 8 above); all
  UI/text regions byte-identical. NEW nondeterminism class found: **NPC
  behavior trees draw from the shared C rand() stream** (civilian.cpp:61,
  behaviortree.cpp:81) that wall-clock rain also consumes — wandering
  civilians need masks: `270,230,330,290` (status_lines, by the terminal) and
  `395,230,480,320` (secret_overlay, deck right of the door).

- **ingame_system_camera is NOT render-gated:** after the first base entry,
  origin's ambience repaint (game_loop.cpp:184 `CopyWithBrightness(colors,
  level, 2, 114)`) stamps the SHARED temppalette's stale high indices into
  the live palette — palette.cpp:227 only rewrites 2..114, and the last
  full-range temppalette write was the boot fade at phase 14 — so every
  UI/parallax index (>114) presents at 112/128 brightness from then on
  (measured: 8.8k diff px at exactly ratio 1.143 vs our render). Our
  pre-baked RGBA HUD can't track live palette mutation; PARITY.md tracks the
  waiver. The golden is still the origin truth for the surface's geometry.

## mission_summary.png — captured 2026-06-11, 1920×1080

- **Source:** `origin/main` @ `af4c50c5` (v00058) + TWO uncommitted capture
  patches in `.worktrees/origin-capture` (NEVER commit there):
  1. the deterministic-fade patch was **stashed** (binary built pristine —
     the summary settles >3s past the fade ramp, measured deterministic);
  2. `gamestateobject.cpp`: ctor sets `requiresauthority=true;
     snapshotinterval=0;` and `Tick()` early-returns on `World::REPLICA`
     after adopting the replicated `winningTeamId` into
     `world.winningteamid`. **Stock origin never replicates the
     GameStateObject at all** (requiresauthority defaults false, contradicting
     the header comment) AND replicas clobber the replicated field every
     TickObjects — so a time-limit match end can never reach
     CheckForEndOfGame on a lobby client and the client drops to LOBBY via
     CONNECTION LOST when the dedicated server exits. The secrets win path
     reaches MISSIONSUMMARY naturally (Team::Tick runs on replicas via
     ProcessSnapshotQueue→TickObjects); the patch only lets the time-limit
     end take the same screen path deterministically. The screen render
     itself is 100% stock origin code.
- **Capture script:** `tools/cap/cap_mission_summary_origin.sh`. Drive: Go
  lobby (fresh db) → alice/secret → Alice/Noxis → Create Game STAR72.SIL →
  Ready → dedicated server (same patched binary) ends the match at 5s via a
  temporary `timeLimitSecs=5` on gameModes id 0 in the BUILT BUNDLE's
  `Contents/assets/gas/gamemodes.json` (backed up + restored by the script)
  → 1-team draw (`winningteamid=0xFFFF`) → server SIGSTOPped on the lobby's
  `[stats]` log line (it would otherwise quit and put the client on the
  connection-lost path) → client reaches `message_i>=240` →
  GoToState(MISSIONSUMMARY) → resize 1920×1080 (in-game pinned the headless
  surface to 640×480) → settle 3s → screenshot.
- **Content:** fresh-account 1-player draw: "+ 0 XP", `*NEW UPGRADE
  AVAILABLE*` banner, six "Current <Stat> Level:" rows (0/0/0/3/0/0), six
  Upgrade oval buttons, zeroed stats scrollbox, Done. The upgrade-button
  state is included (fresh char: totalbonusupgrades−defaultbonuses <
  maxupgrades holds empirically).
- **Determinism:** two independent full runs byte-identical
  (md5 a2890322b49dcadbd877f74e8f081783), no masks needed.

## hover_mainmenu_oval.png — captured 2026-06-11, 1920×1080

- **Source:** `origin/main` @ `af4c50c5` (v00058), `.worktrees/origin-capture`
  build (pristine fade; the uncommitted GSO capture patch from the
  mission_summary capture does not affect menus).
- **Capture script:** `tools/cap/cap_hover_origin.sh`. MAINMENU at 1920×1080
  → `hover_at` the first oval's center (Tutorial, virtual 554,171) → settle
  1s (origin's hover ramp reaches its steady state, phase 4, in 5 × 42ms
  frames) → stable-pair poll (two consecutive byte-identical screenshots —
  only true inside the ~5s logo HOLD window with the ramp settled).
- **Content:** identical to mainmenu.png except the hovered Tutorial cell
  (diff confined to x1026-1460 y347-420): origin button.cpp phase-4 frame —
  sprite bank 6 idx 11 (base 7 + 4) at brightness 136, label at
  LegacyPalette(0,136).
- **Determinism:** two independent runs byte-identical
  (md5 ab22079f39213419d77914092198a604).
- **Chrome-button NEGATIVE (no stored golden):** the same script hovers the
  lobby_connect Login (Chrome) button — measured in two runs: the button's
  cell is byte-identical to the rest-state lobby_connect.png golden under
  hover (origin Chrome buttons change nothing: idx24 + brightness 128 for
  every phase). The whole-frame diffs were only the connect-log port text
  (random per run) and a run-variant caret row; the existing lobby_connect
  golden IS the chrome-hover golden.

## NOT restored — do not trust as parity targets
- **gallery.png** — cppx-only component showcase; no origin/main equivalent. Left as
  the prior cppx render.
- **message_modal.png, password_modal.png** — no standalone origin trigger
  (`show_password_modal` registers widgets but renders nothing from MAINMENU).
  cppx-only drift baselines, re-captured 2026-06-11 at the logo HOLD window
  (the retained-tree cap bump let the mainmenu logo keep animating under
  modals; scenario 70 now stability-polls the logo before each modal cap).

## Rules (see [[feedback_visual_parity_golden_gate]])
- These are the design SPEC. The visual-regression suite will show diffs until the
  cppx renders reach parity — that is intended, not a regression.
- **Never `BLESS=1` to a cppx render.** Blessing here overwrites ground truth. Only
  re-baseline against an externally-approved image.
- Before claiming a screen "done": pixdiff the cppx render vs the golden here and show
  the diff. Self-capture comparison does not count.
