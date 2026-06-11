# GOAL: Full UI parity with pre-refactor origin/main — every surface, menus AND in-game, VISUAL and FUNCTIONAL — built from first principles with 2026 shadcn/React/hooks architecture.

You are an autonomous agent running on a loop in this worktree
(`/Users/hv/repos/Silencer/.worktrees/cppx-migration-cc`, branch `hv/cppx-migration-cc`).

**Why this exists:** Silencer's UI was migrated from legacy Clay to a retained cppx
engine (hooks + Yoga flex + premultiplied-RGBA IR). The migration drifted: some screens
look wrong, some behave wrong, some in-game surfaces were deferred. The owner wants the
player-facing experience to be indistinguishable from pre-refactor `origin/main`
(v00058) while the *code* is a clean, idiomatic 2026 component architecture. You are
the closing loop: find every remaining divergence, fix it, and prove the fix.

## Definition of done (objective stop gate — do not stop before, do not stop on a question)

1. Every visual target surface passes the adversarial gate workflow
   (`tools/cap/visual_parity_gate.js` → overall=PASS) AND `pixdiff_tolerant.py`
   prints PASS (global < 1%, zero tiles over 5%) vs its origin golden. Sole
   exception: a tile in the 5–10% band may be waived as renderer grain only if
   the critic gate passed while explicitly shown that tile's crop, and the
   waiver + coords are recorded in PARITY.md. Tiles over 10% are never waivable.
2. The full functional suite is green: `bash tests/cli-agent/run.sh` — zero failures,
   including the scenarios you add for behaviors not yet covered. The architecture
   guard is also green: `python3 clients/silencer/tools/react_architecture_guard.py
   --root clients/silencer` (starts with 17 pre-existing findings — they're backlog).
3. `PARITY.md` (your ledger, see below) has zero rows in state `DIVERGED` or `UNVERIFIED`.
4. A final fresh-context verification pass (see "The verification pass") returns no
   high-severity findings on the shipped build.

When all four hold, commit, DM the owner a summary with screenshots (skill:
`discord-dm`), and stop. Until then, every iteration picks the highest-value open row
in `PARITY.md` and closes it.

## Ground truth — what "match" means

- **Visual golden:** `tests/cli-agent/e2e/golden/*.png` — authentic `origin/main`
  @ `af4c50c5` (v00058) captures at 1920×1080. Read
  `tests/cli-agent/e2e/golden/ORIGIN_GOLDENS.md` for provenance and capture method.
  The golden image overrides every other description of the look, including this file.
- **Functional golden:** the `origin/main` *source and binary*. The
  `.worktrees/origin-capture` worktree sits at `af4c50c5` with a deterministic-fade
  patch and builds the real origin client — read its code to derive expected behavior,
  and run it when reading isn't conclusive. Note: origin's control port `inspect`
  returns `widgets` keyed by `id`; cppx returns `nodes` — drive origin by visible label.
- **Engine golden (architecture only):** `/Users/hv/repos/ui` is the reference cppx
  engine. It defines idiomatic engine usage, NOT the look.
- **Missing goldens:** if a surface in scope has no golden (most in-game surfaces),
  capture one FIRST from the origin-capture worktree at 1920×1080, document it in
  ORIGIN_GOLDENS.md, then work against it. Never invent a golden from a cppx render.

## Scope — all of it

**Menus/lobby (visual targets, goldens exist):** mainmenu, options, options_audio,
options_display, options_controls, lobby_connect, character_create, cc_alias,
cc_select_agency, lobby_screen, create_game, game_staging, tech_select.
(gallery / message_modal / password_modal have no origin equivalent — functional
checks only, skip visual gate.)

**In-game (previously deferred — now in scope):** HUD (health/ammo/inventory/timer),
scoreboard, chat entry + log, team/tech overlays, death/respawn messaging, pause/escape
flow, and any other player-visible in-game surface you find in origin. Enumerate them
from origin source; do not assume this list is complete.

**Functional parity, every surface:** focus order and keyboard navigation (arrows/
tab/enter/escape), focus-follows-hover, mouse hover/click hit-targets, text entry
(alias, chat, passwords — including caps/length limits), scrolling, modal open/dismiss,
button enable/disable states, screen-to-screen transitions, lobby flows
(connect → auth → character create → lobby → create/join → staging → tech select →
launch), and in-game bindings that drive UI. The behavior spec is origin's code —
derive it, don't guess it.

## PARITY.md — your ledger (create on iteration 0, maintain forever)

On your first iteration, build the backlog: enumerate every surface × {visual,
functional-behavior} from origin source + the golden set, one row each, with state
`UNVERIFIED` / `DIVERGED` / `PASS` and a one-line evidence pointer (gate run, pixdiff
number, e2e scenario name). Re-derive current visual status by actually running
captures + `tools/cap/gate_all.js` — do NOT trust any stale status notes, including
old ones in git history. Keep a `LESSONS.md` beside it: one lesson per entry with why
it mattered (corrections AND confirmed approaches); update or delete entries rather
than duplicating; don't record what the repo already documents.

## Architecture bar (the "shadcn 2026" part — this is about CODE, never the look)

- Composable primitives + design tokens (`clients/silencer/src/client/ui/components/`,
  tokens in `tokens.h`), screens in `clients/silencer/src/client/ui/screens/*.cppx`,
  state via hooks. Read a screen top-to-bottom like a 2026 React component.
- Idiomatic Yoga flex — express origin's layout as flex relationships, never
  transcribe baked legacy pixel coordinates.
- Sprite-first: the original sprite art is the skin (fix the bake if it's wrong);
  nine-slice by default, whole-sprite fixed-aspect for chromes that need it. No vector
  redraws of sprite chrome.
- Don't add features, abstractions, configurability, or error handling beyond what the
  task requires. No backwards-compat shims. Terse comments only — the owner rejects
  comment bloat. If 200 lines could be 50, rewrite it.

How this bar is VERIFIED (two layers, both required):
- Mechanical guard: `python3 clients/silencer/tools/react_architecture_guard.py
  --root clients/silencer` (also a ctest, `react_architecture_guard`). Deterministic
  smells: raw Color{} paint outside tokens/theme, switches over 6 cases, props
  structs over 10 fields, signatures over 8 params, view functions over 200 lines,
  same-line conditional hooks. It starts RED — 17 pre-existing findings (12 paint
  literals, 3 big switches incl. the 9-case AppRoot phase dispatch, 2 god-views
  incl. the 407-line LobbyScreenView) are backlog rows for PARITY.md. Done requires
  it green. Never loosen its thresholds to get green.
- Critic panel: the gate workflows' CodeHygiene phase runs 4 fresh-context lenses
  over the diff (hygiene, composition, state/data-flow, control-flow), each citing
  file:line, with /Users/hv/repos/ui as the idiom reference. The panel owns the
  judgement calls the guard can't grep: prop drilling, state ownership, screens
  that orchestrate instead of compose, MVC-shaped dispatchers.

## THE #1 FAILURE MODE (a prior run shipped this — do not repeat)

Flattening origin's dense multi-color green-phosphor console into a tasteful uniform-
green modern dashboard. Verified palette (already in `tokens.h`): "Silencer" wordmark
RED (152,28,28) · version AMBER (140,64,8) · agent names BLUE (40,96,200) · body/labels
GREEN (24,124,20) · agency emblem = colored sprite (bank 181), not a flat box · lobby
backdrop = dim Mars + circuit HUD (bank 7 idx1), not the bright menu starfield. Panels
are CONNECTED ~1px hairline frames tiling edge-to-edge — never spaced rounded cards
with gaps/shadows. Type is chunky upscaled-bitmap, never modern sans. Any of these
drifts = automatic gate FAIL.

## Visual loop (per surface — every step, every time)

1. Edit the `.cppx` / primitives.
2. Build: `pkill -f "Silencer.app/Contents/MacOS/Silencer"; bash clients/silencer/build.sh`
   (build only through build.sh).
3. Capture at 1920×1080: `bash tools/cap/cap_menus.sh` / `bash tools/cap/cap_lobby.sh`
   → `/tmp/cppx_renders/<screen>.png`. For in-game surfaces, extend the cap tooling
   (see `tests/cli-agent/e2e/51_ingame_ui_overlays.sh` for the drive pattern).
4. Measure: `python3 tools/cap/pixdiff_tolerant.py <render> <golden>` — the gate is
   its printed verdict: PASS requires global < 1% AND zero hot tiles (no ~80px region
   over 5% diff). It prints full-res coords of the worst tiles — go look at exactly
   those regions in both images. A global % under 1% with hot tiles is still a FAIL:
   the old global-only metric passed renders with 30px-misplaced buttons, which is why
   the tile gate exists. Tiles in the 5–10% band can occasionally be renderer grain
   rather than a defect — adjudicate by eye at the printed coords, and record the
   verdict + reason in PARITY.md (never by relaxing the threshold). (Raw
   `tools/pixdiff/build/pixdiff` is ~38% at perfect parity due to origin's
   point-upscale striping — coarse regression signal only.)
5. ACTUALLY LOOK: open BOTH images with the Read tool and compare region by region.
   A number is not a verdict.
6. Gate: run `tools/cap/visual_parity_gate.js` (single screen) or
   `tools/cap/gate_all.js` (sweep) via the Workflow tool — 5 adversarial visual critics
   + a code-hygiene critic. Iterate until overall=PASS.
7. NEVER BLESS=1, never re-capture a golden from cppx, never verify a render against
   itself. Goldens are origin truth only.

## Functional loop (per behavior)

1. Derive expected behavior from origin source (origin-capture worktree); run the
   origin binary when the code is ambiguous.
2. Reproduce against cppx via the control port (`clients/cli/`, op reference
   `shared/skills/cli/SKILL.md`): drive the real binary — hover, click, type, navigate,
   screenshot, inspect.
3. If diverged: fix, then encode the behavior as a numbered scenario in
   `tests/cli-agent/e2e/` (copy `00_ping.sh` as template, end with `PASS NN_name`).
   Every behavior you verify gets a scenario so it can never silently regress.
4. `bash tests/cli-agent/run.sh` must stay fully green — a fix that breaks another
   scenario isn't done.

## The verification pass (legit, non-self-referential — required before claiming done)

When you believe everything passes, run a final audit with FRESH context:

1. Rebuild from clean, re-run ALL captures, re-run `gate_all.js` across every visual
   target on the SHIPPED renders (not cached ones), and re-run the full e2e suite.
2. Dispatch verifier subagents that did not do the implementation work: one sweeps
   PARITY.md and spot-checks 5+ random rows against actual evidence (opens the images,
   runs the scenarios); one plays adversary hunting for surfaces or behaviors in origin
   source that PARITY.md never enumerated; one reviews the whole shipped tree (not just
   the diff) for architecture-bar violations — baked coords, comment bloat,
   overengineering, vector-redrawn sprites, prop drilling, state ownership, MVC-shaped
   dispatch — with the guard green and /Users/hv/repos/ui as the idiom reference.
3. Anything they find reopens the loop. Done means their findings list is empty.

Before reporting progress at any point, audit each claim against a tool result from
this session. Only report work you can point to evidence for; if something is not yet
verified, say so explicitly. If tests fail, say so with the output; if a step was
skipped, say that.

## Known constraints (flag, don't block on)

- Font: `[` `]` `+` and em-dash glyphs render wrong; heading face bank-135 may be
  blank → use the Title face.
- Full-bleed backdrop sprites point-scale (banding) unless < 520px native
  (draw_executor LINEAR gate); chrome frame sprites (628×441) must stay NEAREST/crisp.
- Retained-tree capacity caps can silently truncate ("failed to commit errors=N") —
  virtualize large lists.
- Per-run e2e logs: `/tmp/silencer-e2e-<port>.log`.

## Loop discipline

You are operating autonomously. The owner is not watching in real time and cannot
answer questions mid-task. For reversible actions that follow from this goal, proceed
without asking. Before ending a turn, check your last paragraph: if it is a plan, a
question, or a promise about work you have not done, do that work now. End only at the
stop gate or when blocked on input only the owner can provide — and "blocked" means you
already tried to unblock yourself.

Commit after each surface/behavior reaches PASS (issue → branch → PR workflow per the
root CLAUDE.md). DM progress with screenshots via `discord-dm` at meaningful milestones;
finished work goes to In Review with a snapshot attached — never self-Done.
