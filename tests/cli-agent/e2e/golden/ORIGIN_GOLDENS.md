# Origin/main golden baselines — provenance

These PNGs are the **visual parity targets**: the real origin/main design that the
cppx UI must match. They are NOT cppx renders.

## Source & method
- **Source:** `origin/main` @ `af4c50c5` (Release **v00058**).
- **Captured:** 2026-06-06, fresh, at **960×720** via the headless control port
  (`clients/cli/index.ts` `screenshot`), driven through the real screens (menus,
  Go lobby server login + character-create, dedicated-server staging).
- **Fade disabled for determinism:** the capture build patched
  `GameRenderer::PaletteFadePhaseFromClock()` → `return 16` so every screen renders
  at full brightness instantly (no mid-fade dim frames). This patch lives ONLY in a
  throwaway `origin-capture` worktree, never committed to origin/main.
- Capture scripts: `/tmp/cap_menu.sh`, `/tmp/cap_mm.sh` (logo-decode match),
  `/tmp/caplobby.sh`, `/tmp/capstaging.sh` (maps copied into the lobby `-maps-dir`).

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
