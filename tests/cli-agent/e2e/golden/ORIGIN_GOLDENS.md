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

## NOT restored — do not trust as parity targets
- **ingame_hud, scoreboard, ingame_chat** — DEFERRED. The in-game HUD/scoreboard/chat
  overlay composites in a `Present()`/Clay layer that `--headless` skips, so the
  headless screenshot only captures the world buffer (world + deployed agent render,
  HUD does not). Needs a non-headless windowed capture (macOS `screencapture`) or a
  capture-build patch to composite the overlay into the captured buffer. These files
  are NOT present here yet.
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
