---
name: e2e-visual-regression
description: Use when you need to compare the current branch's UI against a baseline (origin/main or any ref) across every reachable user journey — main menu, options tabs, lobby connect, in-game HUD and overlays. Captures screenshots on both sides, builds side-by-side composites, runs pixdiff. Catches regressions that unit tests, E2E scripts, and architecture-boundary tests miss — e.g. garbled text from string-arena lifetime bugs, button-shape divergences, layout drift.
---

# E2E Visual Regression Across UI Journeys

The other visual-regression skill (`.claude/skills/visual-regression-testing`) is scoped to one screen at a time and to engine-vs-hydration comparison. This one is the wide-coverage sibling: drive the *real* game through every UI surface via the CLI, screenshot each, do the same against a baseline branch, and surface diffs that none of the unit tests / E2E / architecture boundary tests catch.

**Why this exists.** During the Clay UI architecture refactor, unit tests stayed green (19/19), every E2E script passed (8/8), and an independent auditor verdict was "Faithful." But the actual rendered Options Controls screen showed garbled keybind labels ("ÍòÀ", "m;Ð") instead of "Move Up:", "Move Left:" — a string-arena lifetime bug invisible to every other gate. The scripts below would have caught it before the user did.

## When to invoke

Trigger phrases:
- "compare branch against origin/main visually"
- "go through every UI screen and screenshot"
- "diff the UI against [ref]"
- "visual regression test all user journeys"
- "make sure the UI still looks right end-to-end"

Don't invoke for:
- Single-screen iteration loops — use `visual-regression-testing` skill instead.
- Engine-vs-hydration comparison — same.
- Cosmetic spot-check of one screen — just use `cli screenshot` directly.

## What the skill does

1. **Capture** every reachable journey on the current branch into `/tmp/journeys-<current-sha>/`:
   - Main menu (640×480, 1280×720)
   - Options root, Options Controls, Options Display, Options Audio
   - Lobby connect screen (no live lobby needed — the connect-attempt UI itself is captured)
   - In-game HUD (via Tutorial), at 640×480 and 1280×720
   - In-game overlays via `ingame_ui_mode --mode {chat,buy,tech,playerlist}` (note: tutorial mode may not surface buy/tech overlays because those need world stations — flag this in the report, don't claim success)

2. **Capture the baseline** in a git worktree (`/tmp/silencer-<ref>/`). The worktree pattern is mandatory because:
   - Older refs may lack newer CLI ops (`resize`, `ingame_ui_mode`, `wait_ms`).
   - Older refs may use different label casing ("Controls" vs "CONTROLS") on menu buttons.
   - The legacy-UI fade-in is slower than the Clay-UI fade-in; older refs need ~2s settle vs ~1.5s for the Clay branch.

3. **Pixdiff** every pair via `tools/pixdiff/build/pixdiff`. The tool prints diff percentage (lower = closer); 0 = pixel-equal.

4. **Composite** side-by-side images with diff scores annotated, via ImageMagick (`magick`). Output to `/tmp/journeys-composite/`.

5. **Categorize** each pair:
   - ≤ 2% — visually identical, likely animation-phase noise only
   - 2-10% — minor layout drift; eyeball for real differences
   - 10-40% — fade-in timing mismatch OR genuine visual change; eyeball is mandatory
   - 40%+ — likely a fade-in capture mismatch OR a real regression; eyeball is mandatory

The diff number alone is not a verdict — animated logos and fade-in screens can hit 40% pixdiff with no functional difference. Always read the images.

## How to invoke

```bash
# Default: compare current branch vs origin/main
bash shared/skills/visual-regression-journeys/run.sh

# Custom baseline
BASELINE_REF=origin/release-2026-04 bash shared/skills/visual-regression-journeys/run.sh

# Skip baseline capture (re-use existing /tmp/journeys-main/)
SKIP_BASELINE=1 bash shared/skills/visual-regression-journeys/run.sh

# Skip current branch capture (re-use existing)
SKIP_CURRENT=1 bash shared/skills/visual-regression-journeys/run.sh
```

The driver does five things in order: build current → capture current → checkout baseline worktree + build → capture baseline → pixdiff + composites. Each step is its own script; they can be invoked independently for debugging.

## Pre-flight requirements

- `cmake --build build --target silencer -j 8` must succeed on the current branch first. The driver does NOT rebuild for you — it errors if `build/Silencer.app/Contents/MacOS/Silencer` is stale.
- `tools/pixdiff/build/pixdiff` must exist. Build with `cmake -B tools/pixdiff/build -S tools/pixdiff && cmake --build tools/pixdiff/build` if missing.
- `magick` (ImageMagick 7) on PATH. `brew install imagemagick` if missing.
- `ripgrep` (`rg`) for `tests/cli-agent/e2e/60_ui_architecture_boundaries.sh` to actually do anything when invoked. `brew install ripgrep`.

## What it catches (real examples)

- **Garbled text** from string-arena lifetime bugs. The Options Controls screen on `hv/clay-ui-migration` rendered "ÍòÀ" / "m;Ð" / "ým;Ð" where label strings should have been "Move Up:" / "Move Left:" / "Aim Up/Left:". Caught at composite 03_options_controls; the unit tests, the E2E suite, and an independent auditor missed it entirely.
- **Button-shape divergence** from a legacy→modern UI port. Origin/main rendered classic oval Silencer buttons; the Clay rewrite produced rectangular buttons stacked tightly. The architecture-goal said "preserve recognizable button feel" — pixdiff at 41% surfaces the question.
- **Layout drift at non-native viewports**. The `1280×720` reflow captures expose flex-misalignment that the canonical `640×480` flow hides.

## What it does NOT catch

- Behavior bugs that don't show on screen (state machine errors, network glitches, audio).
- Surfaces it can't reach: chat-overlay-with-typed-text, buy menu against a station, tech-tree mid-purchase. Tutorial mode doesn't surface most station-bound overlays. For those, drive a real lobby (not yet automated in this skill).
- Animation regressions — captures land on whatever frame is current; non-deterministic.

## Files

- `run.sh` — the orchestrator. Read it; it documents the flow as comments.
- `capture_current.sh` — captures the current branch into `$OUT_DIR`. Uses `tests/cli-agent/e2e/lib.sh` from the current worktree. Uses Clay-era labels ("OPTIONS", "CONTROLS").
- `capture_baseline.sh` — captures the baseline ref. Sets `SILENCER_BIN` to the worktree's build output. Uses legacy-era labels ("Controls", "Display", "Audio"). Restarts the binary per screen because legacy UI flow is fragile to CLI back-navigation.
- `build_composites.sh` — runs ImageMagick to produce side-by-side composites with diff scores. Uses `/System/Library/Fonts/Supplemental/Arial.ttf` to avoid ImageMagick's font fallback failure on macOS.

## Gotchas

- **Label casing differs between branches.** Legacy UI uses "Controls" / "Display" / "Audio"; Clay UI uses "OPTIONS" / "CONTROLS" / "DISPLAY" / "AUDIO". The two capture scripts are split intentionally.
- **Origin/main's legacy fade is slow.** Use `wait_ms --n 2000` post-state-transition, not `wait_frames --n 4`. The latter is ~167ms; legacy fade can take >1s.
- **`current_interface_id: 0` is a transient state on legacy UI.** After clicking a button to enter a state, the new Interface object instantiates one frame later. If you `inspect` or `click` too soon you get "no current interface". Wait ≥1.5s.
- **`set -e` is dangerous in the orchestrator.** Some captures legitimately fail (e.g., Tutorial → SINGLEPLAYERGAME timeout in a CI env without GPU). Each capture must be its own subshell so one failure doesn't kill the rest.
- **macOS Screen Recording perms** are not needed — these captures go through the in-process `cli screenshot` op which writes PNG from the indexed framebuffer.

## See also

- `.claude/skills/visual-regression-testing` — single-screen iteration loop (engine vs hydration). Use that one when iterating on a specific screen.
- `.claude/skills/using-silencer-cli` — the underlying CLI op reference.
- `tests/cli-agent/e2e/51_ingame_ui_overlays.sh` — pattern this skill copies for in-game state-reaching.
