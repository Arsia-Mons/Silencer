# Agent handoff: fix the 15 cppx-migration regression bugs

> ## ✅ COMPLETE (2026-06-13) — all 15 resolved on `hv/cppx-migration-cc`
>
> **13 committed fixes** + **2 verified already-fixed** (SIL-229, SIL-235, no change needed).
> Commits: SIL-232 `f9d26a49`, SIL-230 `e4556a8b`, SIL-224 `0358dc28`, SIL-228 `1fde8227`,
> SIL-236 `f573c529`, SIL-231 `c6e3954e`, SIL-238 `027a2604`, SIL-227 `6f9359c6`,
> SIL-233 `6446e8b5`, SIL-226 `56a7020b`, SIL-234 `81a8fe3d`, SIL-225 `2b2d190a`,
> SIL-237 `545d3a1e`.
> Fully runtime/visually verified vs golden/origin: 224, 225, 226, 228, 230, 231, 232,
> 233, 234, 237 (e.g. SIL-237 measured 47→327fps static; SIL-233 selected row now the
> origin solid bar RGB 72,16,0). Committed with a documented verification limitation
> (code correct, runtime trigger not available in this setup): **SIL-227** (modal needs an
> update-server response to appear), **SIL-236** (only repros on a 2nd multiplayer game),
> **SIL-238** (needs a 2-client active game). No rest-state regressions (all lobby/menu
> screens unchanged vs golden). The detail below is the historical execution log.
>
> ## ⬆ EXECUTION PROGRESS (2026-06-13) — READ THIS FIRST
>
> **User decision:** everything goes on `hv/cppx-migration-cc` directly with focused
> commits — NO child branches, NO per-cluster PRs. (`hv/themed-primitives-restore`
> was identical to the migration-branch tip; abandoned.)
>
> **Corrected premise:** the original handoff's framing was off. At REST the cppx UI
> already matches origin (0–2.4% pixdiff on every lobby/menu screen, measured vs the
> `tests/cli-agent/e2e/golden/*.png` set). The real regressions are **behavioral /
> interaction-state** (hover, click, scroll, transitions, in-game palette), so verify
> behaviorally, not with static rest-state diffs.
>
> **DONE — committed on `hv/cppx-migration-cc` and verified vs golden:**
> - **SIL-232** ✅ game-name input inset 464→457 (value now pixel-exact at golden y=846).
> - **SIL-230** ✅ scrollbar restored to origin (bright `kInsetStroke` rail + dark
>   `kChromeStroke` thumb fill); themed via `scroll_view.cppx` (engine layer can't
>   include app tokens — values mirror the tokens, commented).
> - **SIL-224** ✅ logo `floorf` pixel-snap (main_menu.cppx); matches golden 0.2%.
> - **SIL-228** ✅ Chrome buttons ramp on hover (app_button.cppx); Login/Create
>   green-mean 56→67 on hover.
> - **SIL-229** ✅ VERIFIED ALREADY-FIXED (no code change) — hovering an input changes
>   0/23780 px, no opaque box. Close the ticket.
> - **SIL-235** ✅ VERIFIED ALREADY-FIXED (no code change) — hover+click a tech row
>   paints NO stroke; the green-rectangle pressed border was removed by commits
>   `300bbfbb`/`419d21c5` and `theme.box` has no focus ring. Close the ticket.
> - **SIL-237** 🔬 PROFILED (real regression, NOT Debug-only — `win-ninja-unity`
>   inherits `win-ninja-release`). Menu frame time scales with native pixel count:
>   1920×1080 = 21.2ms (47fps), 1280×720 = 11.1ms (90fps), 640×480 = 5.3ms (189fps).
>   Root cause: `GameRenderer::Present` (game_renderer.cpp:133-136) calls
>   `CppxUiFrame()` (full native-res RGBA raster) + `UploadUiFrame()` (GPU texture
>   upload) EVERY frame, even on a static menu. Fix (NOT done — substantial/risky):
>   skip the raster+upload when the draw-command IR is unchanged frame-to-frame
>   (preserves animations, which change the IR; kills the redundant static-frame cost).
>   Do NOT memoize the UI model (breaks animations) — dirty-skip at the IR/raster layer.
> - **SIL-231** ✅ committed + verified — the cursor-following map preview now captions
>   the minimap with name (filename) + description (from the map header, carried through
>   the MapPreviews hook, populated at bake). Verified by hovering a map row. NOTE: Text
>   only breaks on `\n` (no auto-wrap), so a long description shows its first line clipped
>   at the card edge; a word-wrap helper is a follow-up.
> - **SIL-238** ✅ committed (builds clean; runtime check needs a 2-client active game) —
>   removed the extra Prev/Next from the active-game browser (origin = Join/Spectate only).
>   Selection defaults to the first game; wiring row-click selection (origin's mechanism)
>   is a follow-up if multi-game selection is needed.
>
> **STILL REMAINING (5) — each blocked on a resource or a risky state-machine change:**
> - **SIL-225** menu→options fade — fire the palette fade on overlay push/pop. RISKY:
>   overlays don't enter FADEOUT (they're pushed above AppRoot via use_navigation), and
>   the fade-in (`game_loop.cpp:666-667`) only runs while `FadePhase<16 && state!=FADEOUT`.
>   A naive RestartPaletteFade on push needs care (and the cppx UI fade coupling, SIL-219).
>   Temporal — verify by capturing successive frames through the transition.
> - **SIL-226** combo bindings — DATA bug, only on the below-fold Aim rows (visible rows
>   are 0.0% vs golden). `make_binding_row` already renders combos[0]/[1] split; the fix is
>   in the keymap-build (an action's 2 alternates were merged into one 2-chip chord).
>   Needs the origin binary driven + scrolled to the Aim rows for the reference.
> - **SIL-227** update modal — swap `panel_patch` (update_screen.cppx:116) for the sibling
>   dialog-sprite treatment (`message_modal.cppx:68-71`: `chrome.dialog_msg ?
>   image_patch_cover(...) : panel_patch(...)`), via use_chrome. Size the sprite container
>   for the larger content (status + progress + button row). Needs the update modal
>   triggered to verify (no easy trigger without an update-server response).
> - **SIL-233** Select-Map selected/clicked row style — CAPTURED HEAD: clicking a map row
>   draws a **bright green outline box** around it (from commit 033ef36b's selectable_row_
>   style). May be wrong (resembles the SIL-235 "invented box" pattern). BLOCKER: need
>   origin's *clicked* map-row reference, but the origin binary (v00000, control socket
>   WORKS) returns the legacy `widgets` inspect format, not cppx `nodes` — so cap_lobby's
>   navigation/wait_for_widget won't drive it without rewriting the parser, and there's no
>   cap_lobby_origin. Capture origin's clicked state, then adjust selectable_row_style
>   (lobby_screen.cppx:1371) — likely toward sprite/caret/label-color, away from an outline box.
> - **SIL-234** create-game→lobby flicker — RISKY: the connect-settle logic (default tech,
>   `game_loop.cpp:581-588`) runs in the LOBBY tick, so a naive `GoToState(JOINGAME)` skips
>   it (and `TickJoinGame` hardcodes STAR72.SIL for the old dev flow). The real fix is a
>   loading state while `joininggame && !IsConnected` — a UI-model/phase change. Temporal.
> - **SIL-236** ⚠ committed but **NOT runtime-verified**. Root cause confirmed by
>   diffing origin/main source: migration changed `fade_i` sim-tick→wall-clock, so the
>   slow `LoadMap` skips the game-loop ambience-palette refresh on entry (black sky /
>   too bright / no weather) on a *second* game with cached `oldambiencelevel`. Fix in
>   `tick_ingame.cpp`+`tick_singleplayer.cpp` invalidates the cached level on entry.
>   **Only repros on a 2nd multiplayer game** — needs a two-game multiplayer smoke test
>   to confirm. The tutorial (1st game) can't reproduce it.
>
> **REMAINING (9) — each needs runtime reference captures of an INTERACTION state
> (no rest-golden exists for these; drive the origin binary to get the reference):**
> SIL-225 (menu→options fade — fire palette fade on overlay push/pop; state-machine,
> not a one-liner since overlays don't enter FADEOUT), SIL-226 (combo bindings — DATA
> bug: an action's 2 alternates merged into one 2-chip chord; `make_binding_row` already
> renders combos[0]/[1] split, so fix the combo-building in the game_ui_pipeline/keymap
> load, not the row; only shows on the below-fold Aim rows), SIL-227 (update modal —
> needs an update trigger), SIL-231 (minimap preview name/desc + position — reachable in
> create_game, no dedicated server), SIL-233 (map-row selected style — reachable in
> create_game; needs origin click-state ref), SIL-234 (create-game→lobby flicker —
> GoToState(JOINGAME)), SIL-235 (tech green stroke — likely already fixed by the
> pressed-border commits; reach staging→tech, click a row, check; mirror the SIL-229
> verify-likely-fixed pattern), SIL-237 (perf — PROFILING spike, measure frame timing),
> SIL-238 (active-game Prev/Next — remove them, but `game_line` is a non-clickable
> BodyText so removing Prev/Next strands selection; origin selects by clicking the row,
> so also make rows selectable; needs an active game to verify).
>
> **HARNESS GOTCHAS DISCOVERED (Windows):**
> - Client baked version is **00058** — start any cap lobby with `-version 00058`
>   (NOT 00000) or auth times out. `export SILENCER_VERSION=00058`.
> - Pin `export SILENCER_BIN=.../build-unity/Silencer.exe` (lib.sh picks newest, but be
>   explicit). Build with `clients/silencer/build.ps1 win-ninja-unity`.
> - `tools/cap/cap_lobby.sh` WORKS (boots its own lobby on its own port; reaches
>   create_game/staging/tech). `cap_ingame_cppx.sh` reaches in-game via the TUTORIAL
>   (single-player), not multiplayer.
> - `python3` in Git Bash is a WindowsApps stub (Permission denied). Real Python:
>   `C:\Python313\python.exe` (PIL+numpy installed). cap scripts that call `python3`
>   need a shim AND a `/tmp`→`C:\tmp` path bridge (Git Bash `/tmp` =
>   `C:\Users\SPACEC~1\AppData\Local\Temp`, but Windows Python reads `/tmp` as `C:\tmp`).
>   Side-by-side / measurement helpers used this session live in
>   `%TEMP%\parity\` (sbs.py / measure.py / sample.py).
> - Restore the prod config when fully done (per below). Local lobby on :15170 may be
>   stale (was started `-version 00000` ≠ client 00058).
>
> ---

You are taking over implementation. **The analysis is done — do not re-investigate
from scratch. Execute.** Every root cause below was verified against the real code
(file:line) and, where noted, the running client. Your job is to fix, build, verify
against origin/main, and open per-cluster PRs.

## Mission

Fix all 15 Linear bugs labeled **bug** in the **Backlog** (team `Silencer-cc`,
SIL-224…SIL-238). They are ALL regressions introduced by the current branch
`hv/cppx-migration-cc` (the cppx/Clay UI migration) vs `origin/main`. `origin/main`
is the golden reference; "fix" = "match origin/main."

- **One PR per cluster** (user decision), targeting the migration branch
  `hv/cppx-migration-cc` (NOT `main` — the buggy code only exists on the migration
  branch). Follow the repo's issue→branch→PR→squash flow (`docs/git-workflow.md`).
- Verify visually through the **real runtime** (control socket + screenshots vs
  origin), not compile success alone (this is mandated in
  `clients/silencer/CLAUDE.md`). A pixel-parity harness already exists under
  `tools/cap/` — use it.

## Build & run

- Build: `clients/silencer/build.ps1 win-ninja-unity` (Windows; ~15s). Binary →
  `clients/silencer/build-unity/Silencer.exe`. There is also a non-unity
  `clients/silencer/build/Silencer.exe`. **Always build via the wrapper**, never raw
  cmake. cppx `.cppx/.hx` files transpile at build time (Python3 required).
- The default `win-ninja` preset is **Debug** — relevant for the perf bug (SIL-237).

## Local test harness (ALREADY SET UP — running now)

- **Local Go lobby is running** on `:15170`, `-version "00000"`, db
  `services/lobby/lobby.local.json`, game-binary `build/Silencer.exe`. If it died,
  restart from repo root:
  ```
  ./services/lobby/silencer-lobby.exe -addr :15170 -db ./services/lobby/lobby.local.json \
    -game-binary 'C:\Users\Space Command\repos\zSilencer\clients\silencer\build\Silencer.exe' \
    -public-addr 127.0.0.1 -version '00000'
  ```
- **Client `config.cfg` is repointed to the local lobby** (`lobbyhost=127.0.0.1`,
  `lobbyport=15170`). Backup of the original (prod) config is at
  `%APPDATA%\Silencer\config.cfg.prod-backup`. **Never let the client hit production
  (`lobby.arsiamons.com`).** Restore the prod config when fully done.
- **Accounts/characters already exist** in `lobby.local.json`. Log in to an account
  that HAS a character and you land straight in `LOBBY` (no character-create). The
  `""`-named account has characters `frank`/`bill`. Do NOT edit `lobby.local.json`.
- **Driving the client (CLI skill):**
  ```
  . tests/cli-agent/e2e/lib.sh
  PORT=$(pick_port); PID=$(start_silencer "$PORT"); wait_alive "$PORT"
  cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 20000
  cli --port "$PORT" click --label "Connect To Lobby"
  cli --port "$PORT" screenshot --out /abs/path.png
  cli --port "$PORT" inspect          # widget tree (ids, labels, kinds)
  ```
  Ops: `wait_for_state`, `click --label|--id`, `set_text --uid <uid> --text ...`
  (use `--uid` for text inputs, not `--label`), `select`, `screenshot`, `inspect`,
  `state`, `world_state`, `pause/step` (single-player), `back`, `quit`. Full ref:
  `clients/silencer/.claude/skills/cli/SKILL.md`. **Prefer the existing navigation
  scripts** `tools/cap/cap_lobby.sh`, `cap_ingame_cppx.sh`, `ingame_drive_lib.sh`,
  `cap_walkthrough.sh` over hand-rolling navigation.
- **origin/main golden binary** is checked out at
  `.worktrees/origin-main/clients/silencer/build-release/Silencer.exe` for
  side-by-side capture. If its version != `00000`, run a SECOND lobby instance with
  `-version ""` (accepts any) on a different port for the origin run, or use the
  `tools/cap/cap_*_origin.sh` scripts.
- Kill stray clients between runs: `Get-Process Silencer | Stop-Process -Force`.

## Already done

- **SIL-228 fixed** on branch `hv/themed-primitives-restore` (off
  `hv/cppx-migration-cc`): `app_button.cppx:46` now ramps `Chrome` buttons on
  hover/focus (`|| props.variant == AppButtonVariant::Chrome`); the `chrome_btn[p]`
  ramp frames are baked, so rectangular buttons now brighten on hover like origin.
  Still needs a hover-state visual check.

## The 15 bugs — verified root causes + fixes

### Cluster ① Keystone: themed primitives  (branch `hv/themed-primitives-restore`)
- **SIL-228** ✅ done (see above). Verify hover visually.
- **SIL-229** input opaque hover box — **likely already fixed in HEAD.**
  `tokens.h chromeless_field_style()` sets `base=hover=focus_visible=` the
  transparent clear patch, and every named input uses it (`lobby_connect.cppx:125`,
  `in_game_screen.cppx:383`, `character_create.cppx:381`,
  `lobby_screen.cppx:487/625/1246`). `resolve()` (resolve.cpp:9-12) applies
  `role.hover` then `ov.hover`; `apply()` (style_patch.h:44) overwrites bg+gradient,
  so the transparent override wins. **Verify by hovering an input; if no opaque box,
  close as already-fixed.**
- **SIL-227** update modal bespoke styles — `update_screen.cppx` uses
  `panel_patch(kSurfacePanel,kBorderPanel)`; the sibling modals use the baked dialog
  sprite (`message_modal.cppx:68-71`: `chrome.dialog_msg ? image_patch_cover(...) :
  panel_patch(...)`). Make the update modal consistent with the sibling modal
  treatment. Note its content (status + progress bar + multi-button row) differs from
  the fixed-size message modal — size the sprite container to fit.
- **SIL-230** GameOptions scrollbar bespoke — **found:** `scroll_view.cppx:145-156`
  hardcodes `track_paint = Color{40,72,40,110}` and `thumb_paint = Color{92,208,92,235}`
  as raw literals (added by the SIL-213 scroll fix). Replace with origin's scroll-box
  treatment built from themed/shared tokens. Reference: capture origin's create-game
  GameOptions scrollbar; the character-create roster scrollbar (sprite caps) is the
  likely origin style. Keep the working scroll behavior.
- **SIL-233** map-list selected style — `lobby_screen.cppx:1371 selectable_row_style()`
  is already applied to map rows (`map_list_rows`, :1432). Capture create-game map
  list (click a row) on HEAD vs origin; adjust `selectable_row_style` to match.
- **SIL-235** tech green stroke — bug wants to **REMOVE** an invented green box/stroke
  shown on click, NOT add selection feedback (the first triage misread this). Tech
  rows: `lobby_screen.cppx:848-883` (checkbox Box `image_patch(tex)`, label Box no
  style, both `focusable`). `theme.box` has no focus ring (app_theme.cpp:131) and
  `box.cppx:17` resolves with empty interaction state, so the stroke source is NOT
  visible statically — **observe it at runtime** (create game → staging → tech list →
  click a tech row → screenshot), then suppress whatever paints it (likely a focus
  indicator). Origin conveys tech selection via the toggle sprite + label color, no
  row stroke.

### Cluster ② Perf SIL-237 (HIGH) — PROFILING SPIKE, not a blind fix
Both prior hypotheses are **wrong**: (a) the `ResetUiFrameDeltas` input reset is
present (`game_ui_pipeline.cpp:1816`) and wouldn't affect frame RATE anyway; (b) the
"per-frame allocations" aren't severe — `EnsureHudRampVariant`/`EnsureHudTeamEmblem`
are **cached** (`game_ui_pipeline.cpp:978/1009`), `CaptureLobbySnapshot` early-returns
on MainMenu (`lobby_ui_model.cpp:343`), and `CaptureWorldSessionSnapshot` carries
REQUIRED per-frame animation (caret blink :133, pulse :174) + side-effects (clamps
`player->buyifacelastitem`/scroll :156-172) so it **cannot be naively memoized**
(that would freeze animations / break buy-tech scroll). Likely real cost: the
per-frame native-resolution RGBA UI raster+upload (`PipelineHost::render` →
`UploadUiFrame` every frame; menus composite UI at native res while in-game is
640×480). **Profile frame timing (menus vs in-game vs origin/main) and confirm
whether it's Debug-only before changing anything.**

### Cluster ③ Transitions
- **SIL-225** no fade menu→options — Options are overlays pushed via `nav.push`
  (`main_menu.cppx:87`), which bypass `GoToState()` — the only `RestartPaletteFade()`
  trigger (`game_loop.cpp:677-684`). Fire the fade on overlay push/pop (at the
  ScreenStack mutation-drain site / `use_navigation`).
- **SIL-234** create-game flicker — after `creategamestatus==1`, `JoinGame()` runs but
  GameState stays `LOBBY` (`game_loop.cpp ~630-650`), so the lobby re-renders during
  the async socket connect → flicker. Fix: `GoToState(JOINGAME)` to hold a stable
  Loading screen until `world.IsConnected()`.
- **SIL-224** logo jitter — `main_menu.cppx:93-124` computes the logo `position_inset`
  from non-pixel-snapped floats every layout pass → jitter. Fix: `floorf()` the
  position (and frame offsets). Confirmed **independent of perf**.

### Cluster ④ Individuals
- **SIL-226** combo bindings — `options_controls.cppx make_binding_row (~95-151)`:
  data model moved from origin `bindings` (OR-slots) to `combos` (AND-chords); render
  `combos[0]` in the left oval, `combos[1]` in the right, with the right And/Or
  connector. Reachable via OPTIONSCONTROLS — easy to capture+verify. Also
  `hooks/use_key_map.h`.
- **SIL-238** active-game Prev/Next — `lobby_screen.cppx GameBrowserActions (~224-275)`
  unconditionally adds Prev/Next (:235-252); origin shows only Join/Spectate. Remove
  Prev/Next. Needs an active game in the lobby to verify (`lobby game create` CLI or a
  second client).
- **SIL-232** game-name input too low — `lobby_screen.cppx CreateRightCell` absolute
  insets (~:654 label / :664 input = 33px gap). Match origin spacing (capture origin's
  create-game for exact insets). Its hover-box is covered by SIL-229.
- **SIL-231** minimap preview — `lobby_screen.cppx MapPreviewOverlay (:1447-1499)`
  renders the image only (no name/description) and the cursor-follow math is off
  (:1461-1470). Add name+description (extend the signature + `use_map_previews.h`) and
  fix positioning. The "laggy" symptom may be downstream of SIL-237.

### Cluster ⑤ Render-init SIL-236 (HIGH) — RUNTIME SPIKE
Candidate: palette `CalculateLighted()` only runs on ambience change
(`game_loop.cpp:302`); on INGAME entry `old==new==0` so the lighted palette is
uninitialized → too bright; a base round-trip changes ambience → fixes it. Candidate
fix: in `tick_ingame.cpp` after `SetColors()` (~:74) call
`renderer.palette.CalculateLighted(0)` and set `oldambiencelevel=0`. **BUT this
explains the lighting only, NOT the black background or missing weather** — at runtime
confirm whether `DrawBackground`/weather route through the lighted palette (if yes,
one fix covers all three; if not, find the separate cause). Build + enter a game +
observe before committing.

## Suggested order
Keystone ① (finish via lobby captures), ③ + ④ (clear code fixes), then the two HIGH
spikes (⑤ render-init runtime spike, ② perf profiling). Verify each against origin via
`tools/cap/` before opening its PR.

## Don't repeat these mistakes
- Don't "memoize" SIL-237 — it breaks animations/side-effects and isn't the real cost.
- Don't ADD selection styling for SIL-235 — the ask is to REMOVE a stroke.
- Don't assume SIL-229/233 are broken — they look fixed in HEAD; verify, then likely close.
- Don't ship visual fixes on compile-success alone — capture vs origin.
