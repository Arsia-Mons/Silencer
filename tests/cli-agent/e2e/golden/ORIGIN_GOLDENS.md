# Origin/main golden baselines — provenance

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

## In-game goldens (8) — captured 2026-06-11, 640×480
`ingame_hud_base, ingame_chat_open, ingame_chat_history, ingame_player_list,
ingame_buy_tech, ingame_messages, ingame_tech_overlay, ingame_quit_prompt`

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
  - `ingame_quit_prompt` = separate `--headless --tui` session (control ops
    can't reach the quitstate machine — it listens to raw scancodes only):
    throwaway TCP frame sink + scancode bitmask (ESC=41) over `--tui-input-port`,
    press edge (quitstate 1) + release edge (quitstate 2, prompt latched), each
    consuming one sim tick — anchored at `message_i == 254` so the capture
    lands on `message_i == 0`.
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
- **NOT captured** (no deterministic headless trigger; PARITY.md tracks):
  top_ticker (only trigger is "Playing: <random track>" / F4-F9 paths),
  status_lines (all `ShowStatus` callers need real gameplay actions),
  hud_secret_overlay / hud_trace_time / hud_system_camera (need secrets/beaming/
  system-camera world states), scoreboard-style F1 with multiple teams.

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

## NOT restored — do not trust as parity targets
- **gallery.png** — cppx-only component showcase; no origin/main equivalent. Left as
  the prior cppx render.
- **message_modal.png, password_modal.png** — no standalone origin trigger
  (`show_password_modal` registers widgets but renders nothing from MAINMENU). Left as
  prior renders — treat as STALE/contaminated until recaptured.

## Rules (see [[feedback_visual_parity_golden_gate]])
- These are the design SPEC. The visual-regression suite will show diffs until the
  cppx renders reach parity — that is intended, not a regression.
- **Never `BLESS=1` to a cppx render.** Blessing here overwrites ground truth. Only
  re-baseline against an externally-approved image.
- Before claiming a screen "done": pixdiff the cppx render vs the golden here and show
  the diff. Self-capture comparison does not count.
