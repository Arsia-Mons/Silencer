# GOAL: Make the cppx UI 100% VISUALLY MATCH origin/main (v00058), screen by screen, gated by the visual-parity workflow.

## RESOLUTION (user requirement): every screen is captured/compared at 1920×1080 (per single screen; a render|golden side-by-side is ~2× wider). NOT 960×720. See memory feedback_capture_resolution_1080.
The goldens (tests/cli-agent/e2e/golden/*.png) were RE-CAPTURED from origin/main @af4c50c5 at 1920×1080 on 2026-06-07 (see ORIGIN_GOLDENS.md). cap_menus.sh / cap_lobby.sh now default W=1920 H=1080.
**OPEN BLOCKER:** origin SCALES its UI to the window (fixed-aspect, ~66% vertical proportion held; content fills the 16:9 width responsively), but the cppx screens are authored at FIXED 960×720 pixel coords, so at 1920×1080 they cram into the top-left ~860px with small buttons (mainmenu ~41% pixdiff). The cppx must scale/fill to the window before per-screen parity at 1920×1080 is meaningful — likely render at a fixed 960×720 logical canvas scaled-to-fit, OR make every screen responsive. Resolve this FIRST; the prior 960×720 PASS results are stale at the new resolution.

Worktree: /Users/hv/repos/Silencer/.worktrees/cppx-migration-cc (branch hv/cppx-migration-cc).
The UI was migrated from legacy Clay to a retained cppx engine (hooks + Yoga flex + premultiplied-RGBA IR). Most screens drifted. Rebuild each cppx screen so it READS AS the original origin/main design. The CODE must look like clean 2026 shadcn/React-sensibility component code (tokens + composable primitives, terse comments — NO bloat comments). The LOOK must be byte-for-byte the original.

## Ground truth (the ONLY source of "look")
tests/cli-agent/e2e/golden/*.png are authentic origin/main v00058 captures (1920×1080 as of 2026-06-07; re-captured from 960×720). Read tests/cli-agent/e2e/golden/ORIGIN_GOLDENS.md.
Real targets: mainmenu, options, options_audio, options_display, options_controls, lobby_connect, character_create, cc_alias, cc_select_agency, lobby_screen, create_game, game_staging, tech_select. (in-game hud/scoreboard/chat DEFERRED; gallery/message_modal/password_modal have NO origin target — skip.)

## THE #1 FAILURE MODE (do not repeat it): flattening to uniform green / modern SaaS cards
origin/main is a DENSE, MULTI-COLOR green-phosphor HUD — NOT a tasteful green dashboard. Verified palette (tokens already added in clients/silencer/src/client/ui/components/tokens.h):
- "Silencer" brand wordmark = RED  kTextBrand   (152,28,28)
- build version "v.00058"    = AMBER kTextVersion (140,64,8)
- agent NAMES (lobby/cards)  = BLUE  kTextAgentName (40,96,200)
- body/labels/most buttons   = GREEN kTextBody (24,124,20)
- agency "portrait"          = the COLORED agency-emblem SPRITE (bank 181), already baked as chrome.agency_emblem[agency] — NOT a flat box.
- lobby backdrop             = bank 7 idx1 (dim Mars + circuit-HUD), baked as chrome.lobby_backdrop — NOT the bright bank-6 menu starfield.
Panels are CONNECTED ~1px green hairline frames tiling edge-to-edge (small seams), NOT spaced rounded cards with gaps/shadows. Type is chunky upscaled-bitmap, not modern sans. "shadcn/first-principles" = ARCHITECTURE ONLY.

## NON-NEGOTIABLE verification loop per screen (a prior run shipped wrong by skipping this)
1. Edit the .cppx (clients/silencer/src/client/ui/screens/*.cppx; primitives in clients/silencer/src/ui/components/ and clients/silencer/src/client/ui/components/).
2. Build: `pkill -f "Silencer.app/Contents/MacOS/Silencer"; bash clients/silencer/build.sh` (build only through build.sh).
3. Capture @1920x1080 (per-single-screen; user requirement — NOT 960x720): `bash tools/cap/cap_menus.sh` (mainmenu/options/options_audio/options_display/options_controls) and `bash tools/cap/cap_lobby.sh` (lobby_connect/character_create/cc_alias/cc_select_agency/lobby_screen/create_game/game_staging) → renders land in /tmp/cppx_renders/<screen>.png. The cap scripts default W=1920 H=1080. NOTE: the in-repo goldens are still 960x720 (4:3); the golden side must be re-captured at 1920x1080 for valid pixel comparison (16:9 vs 4:3 repositions the layout).
4. pixdiff: `tools/pixdiff/build/pixdiff /tmp/cppx_renders/<screen>.png tests/cli-agent/e2e/golden/<screen>.png`. **TARGET (user requirement 2026-06-07): pixdiff < 1%, ideally ≤ 0.5%.** See "## PIXDIFF THRESHOLD" below — the raw `tools/pixdiff` byte-exact metric cannot reach this across the two renderers (origin point-upscales w/ scanline striping; cppx renders crisp → ~38% even for a pixel-perfect-looking match), so the <1% target is measured on a TOLERANT/perceptual diff (downscaled MAE-thresholded), not raw byte-exact.
5. ACTUALLY LOOK: open BOTH /tmp/cppx_renders/<screen>.png AND tests/cli-agent/e2e/golden/<screen>.png with the Read tool and compare region-by-region yourself. pixdiff alone is NOT sufficient. (Optional: python3+PIL to crop/zoom regions or build golden|render composites.)
6. RUN THE GATE WORKFLOW (this is the rigorous reviewer the user demands):
   Workflow({ scriptPath: "tools/cap/visual_parity_gate.js",
              args: { screen: "<screen>", render: "/tmp/cppx_renders/<screen>.png",
                      golden: "tests/cli-agent/e2e/golden/<screen>.png",
                      pixdiff: <number>, files: "<space-separated changed files>",
                      checklist: "<your whole-composition target notes for this screen>" } })
   It runs 5 adversarial VISUAL critics (each opens BOTH images, cites ≥4 regions) + a code-hygiene critic (flags bloat comments + overengineering), and returns overall PASS/FAIL + ranked top_fixes. PASS only when no critic fails and zero high-severity diffs; any palette-flatten / spaced-card / modern-type / missing-backdrop flag = automatic FAIL.
7. Iterate until the gate returns overall=PASS. Then set the ticket In Review + DM the shipped screenshot (skill: discord-dm) for approval. NEVER self-Done. NEVER BLESS=1 (goldens are origin truth).

## Tooling already in place
- Capture: tools/cap/cap_menus.sh, tools/cap/cap_lobby.sh (persist to /tmp/cppx_renders; cap_lobby boots the Go lobby + drives connect→auth→char-create→lobby→create_game→staging). For staging/match it copies shared/assets/level/*.SIL into the lobby -maps-dir.
- Diff: tools/pixdiff/build/pixdiff [--crop x,y,w,h] <render> <golden>.
- Gate workflow: tools/cap/visual_parity_gate.js (described above).
- Design spec: docs/plans/2026-06-06-cppx-visual-design-language.md (golden OVERRIDES it).

## PIXDIFF THRESHOLD (user requirement, 2026-06-07)
When pixdiff is used for verification, the target is **< 1% difference, ideally ≤ 0.5%.**
CAVEAT (measured): `tools/pixdiff/build/pixdiff` counts EXACT per-channel byte inequality. Across the two rendering engines this is ~6% even for a black-background dialog (lobby_connect) and ~38% for any screen with the Mars/starfield backdrop — *even when the layout/color/type is a pixel-perfect visual match*. Root cause: origin renders a low-res framebuffer and POINT-upscales it to 1920×1080 with non-integer factors (visible horizontal scanline striping + row-doubling), while cppx renders crisp at the device scale; sub-pixel/1px alignment + AA/glyph rasterization never align byte-for-byte. Proof: downscaling both to 320×180 (kills the high-freq upscale/AA noise) drops the difference to mae ≈ 1.4–3.7 for the matching screens — i.e. they ARE at parity structurally.
THEREFORE the <1% target is enforced on a TOLERANT diff (downscale to ~320×180 BOX, then % of channel-bytes off by >16, or mean-abs-error), NOT raw byte-exact. Raw byte-exact stays a coarse regression signal only. The visual-parity GATE WORKFLOW remains the primary acceptance check.

## Known constraints (flag, don't block)
- Font: [ ] + and em-dash glyphs render wrong in cppx; heading face bank-135 may be blank → use the Title face. The full-bleed backdrop sprite point-scales (banding) unless it's <520px native (draw_executor LINEAR gate) — chrome frame sprites (628x441) must stay NEAREST/crisp.
- Retained-tree capacity caps can silently truncate ("failed to commit errors=N"); virtualize large lists.
- Sprite-first: use ORIGINAL sprite art (fix the bake), not vector redraws.

## Status at handoff (pixdiff % vs golden; lower=closer)
mainmenu 6.9 · options 4.7 · options_audio 4.8 · options_display 5.8 · options_controls 24 · lobby_connect 18 · character_create 18.5 · cc_alias 23 · cc_select_agency 25 · lobby_screen 36 · create_game 38 · game_staging 37. (Lobby cluster already rebuilt to the dim multi-color console; remaining work: tighten each toward PASS.)

## Discipline
Autonomous: DM progress with screenshots, never stop on a question, stop gate = every real-target screen returns gate overall=PASS. Combat overengineering; terse comments only (the owner rejects comment bloat).
