# Orchestrator prompt — cppx UI visual parity to origin/main (v00058)

> Paste the block below into a fresh `ultracode` Claude Code session in the
> `hv/cppx-migration-cc` worktree. It is self-contained.

---

ultracode

# Mission: make the cppx UI visually MATCH origin/main (v00058), screen by screen, with a real golden-diff gate

You are in the Silencer repo, worktree `/Users/hv/repos/Silencer/.worktrees/cppx-migration-cc`
(branch `hv/cppx-migration-cc`). The UI was migrated from the legacy Clay UI to a
retained cppx engine (hooks + Yoga flex + premultiplied-RGBA draw IR). The migration
left most screens looking nothing like the real game. Your job: rebuild each cppx
screen so it READS AS the original origin/main design.

## Ground truth (this is new — trust it)
`tests/cli-agent/e2e/golden/*.png` are now **authentic origin/main v00058 captures**
(960×720), freshly captured and verified. Read `tests/cli-agent/e2e/golden/ORIGIN_GOLDENS.md`
first — it lists which screens are real targets and which are deferred. The real
targets are: mainmenu, options, options_audio, options_display, options_controls,
lobby_connect, character_create, lobby_screen, cc_alias, cc_select_agency,
create_game, game_staging, tech_select. (in-game hud/scoreboard/chat are DEFERRED —
not captured yet; skip them. gallery/message_modal/password_modal have no origin
target — skip.)

## NON-NEGOTIABLE verification discipline (a prior run shipped a wrong lobby by skipping this)
1. **Golden-diff gate before "done".** For each screen, in the SAME work cycle: build →
   capture the cppx render at 960×720 → run `tools/pixdiff/build/pixdiff` (and visually
   open) the render AGAINST `tests/cli-agent/e2e/golden/<screen>.png` → SHOW the diff.
   You may NOT say "parity/done" without that comparison. Comparing a render to your own
   earlier render does NOT count — that is the exact mistake that produced a wrong lobby.
2. **Never `BLESS=1` to a cppx render.** The goldens are origin truth; blessing destroys
   them. The suite SHOULD show diffs until parity — that is intended.
3. **Re-run the adversarial visual critic on the EXACT render being shipped**, after the
   final edit. A critic that ran on a since-discarded draft gates nothing.
4. **"shadcn / first principles" = ARCHITECTURE ONLY** (tokens + composable primitives).
   It is NOT a license for a modern card/dashboard aesthetic. The golden is the only
   source of LOOK: dense connected green-phosphor HUD, hairline panels, condensed pixel
   type, sprite chrome. If your result looks like spaced rounded cards with big type, it
   is wrong.
5. **Audit each golden WHOLE-COMPOSITION first** (regions of the entire image, including
   large faint/background areas) before planning layout. A prior run missed the lobby's
   central Mars/starfield backdrop + "In Lobby" presence float and the bottom lamp strip
   by enumerating only the obvious panels.
6. **End each screen In Review + DM a screenshot for approval, not self-Done.**

## The lobby is the worst offender — rebuild it to match `lobby_screen.png` exactly
Current cppx lobby = a spaced-card dashboard (rounded panels + gaps, oversized Hero
type, an 11-pill emblem strip mounted top AND bottom, flat blue avatar box). The golden
is a DENSE connected green HUD. Required:
- Full-bleed Mars/starfield backdrop with a small bordered **"In Lobby" presence float**
  over the lower-center panels (not a side card).
- Connected ~1px dim-green hairline panels tiling edge-to-edge with ~4px seams (kill the
  rounded-card fills/gaps/black gutters).
- Top-left AgentCard: real agent **portrait sprite** (not a blue box) + WINS/LOSSES/XP/LV,
  with the 6-row StatColumn (ENDURANCE/SHIELD/JETPACK/TECH SLOTS/HACKING/CONTACTS) BESIDE it.
- "Create Game" as a small top-center floating green oval (not a panel hosting the stats).
- "Active Games" panel right; "Lobby" chat panel lower-left with dense scrollback.
- A single BOTTOM strip of ~7 soft-glowing oval lamp emblems; remove the top strip.
- Condensed pixel type (drop ScreenTitleVariant::Hero) so density returns.
Cross-check create_game / game_staging / tech_select against their goldens too — same
shared persistent console chrome.

## Tooling
- Build: `bash clients/silencer/build.sh` (kill a running daemon first — it locks the
  link: `pkill -f "Silencer.app/Contents/MacOS/Silencer"`).
- Capture cppx renders at 960×720 via the e2e harness: `70_visual_regression.sh` (menus),
  `71_visual_regression_lobby.sh` (lobby via the Go lobby server). Source
  `tests/cli-agent/e2e/lib.sh` (bash, not zsh). Drive screens with `cli` ops
  (screenshot/click/set_text/key/wait_for_state/inspect; lobby needs the Go server +
  login + `create_initial_character`). For staging/match, copy `shared/assets/level/*.SIL`
  into the lobby `-maps-dir`, select a map by `--label` (NOT numeric `--id` — string ids
  crash the client), name the game, click Create.
- Diff: `tools/pixdiff/build/pixdiff [--crop x,y,w,h] <render> <golden>`.
- cppx screens: `clients/silencer/src/client/ui/screens/*.cppx`; primitives in
  `clients/silencer/src/ui/components/`; design tokens in `src/ui/.../tokens.h`.
- Design language spec (useful, but the golden overrides it): `docs/plans/2026-06-06-cppx-visual-design-language.md`.
  Per-screen plans in `docs/plans/screens/*.md` exist but the lobby one missed the
  viewport/backdrop — re-derive from the golden, don't trust it blindly.

## Known constraints
- Font: punctuation glyphs `[ ] + —`(em-dash) render wrong in cppx (origin renders them
  fine). Heading face bank-135 renders blank → use the Title face. Don't block on these;
  flag them.
- Retained-tree capacity caps can silently truncate a screen ("failed to commit
  errors=N"); they were bumped + now self-report. Virtualize large lists.
- Sprite-first, nine-slice by default; whole-sprite fixed-aspect for some chromes. Use the
  ORIGINAL sprite art (fix the sprite-bake), not vector redraws.

## Suggested workflow shape (adapt)
1. UNDERSTAND (parallel): whole-composition audit of each golden → per-screen target spec
   (regions, density, chrome, type scale, sprites) cross-checked against the image.
2. RECREATE (pipeline, one screen per item): edit → build → capture@960×720 → pixdiff vs
   golden → iterate until it reads as the same design.
3. VERIFY (adversarial, on the SHIPPED render): independent critics compare each final
   render to its golden; loop until parity; then In Review + DM for approval.
Start with the lobby cluster (most wrong), then character-create cluster, then re-confirm
the menu cluster.
