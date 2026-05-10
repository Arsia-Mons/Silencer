# UI v2 — progress + handoff

**Companion design doc:** [2026-05-10-ui-v2-declarative-layout.md](2026-05-10-ui-v2-declarative-layout.md).

This doc is **mutable**. Each session updates the checklist below and
rewrites the "Handoff prompt" section at the bottom so the next
session can pick up cold.

---

## Status

- **Current branch:** `hv/layout` (worktree at `.worktrees/hv-layout/`)
- **CMake `cxx_std_14`** (working state). C++20 was attempted and
  reverted — see "C++ standard upgrade attempt" below.
- **PR #1.5 in flight:** Clay layout engine + MainMenu redesign.
  Clay header vendored at `clients/silencer/third_party/clay/clay.h`
  (v0.14, 4393 lines). Layout pass + new NodeKinds + MainMenu port
  not yet implemented.
- **PR #1** (foundation + mouse interaction + `hot_t` animation) was
  committed pre-session (commit `afd6b98` "ui/v2 declarative layout
  library + MainMenu parity") for the foundation, plus follow-on
  work this session for mouse + animation.
- **Last verified state:** Windows MSVC unity build green at
  `cxx_std_14`. v2 vs legacy MainMenu byte-identical PPM dump
  (will break once Clay redesigns MainMenu — see "Pixel-parity
  decision" below).

## Pixel-parity decision (2026-05-10)

The "byte-identical PPM as merge gate" rule from the design doc has
**one explicit carve-out: MainMenu when ported to Clay.** Legacy
MainMenu has buttons at non-uniform absolute positions (post-chrome-
offset: roughly (350,154), (390,221), (350,288), (310,355) — slightly
staggered, not column-aligned), which a clean `Center({VStack({...})})`
won't reproduce. The user chose to **redesign MainMenu** when porting
(centered VStack, evenly spaced) and **replace the diff gate with a
committed reference PPM** the user eyeball-approves once.

Other screens still ship byte-identical against their legacy
counterparts — the carve-out is MainMenu-specific because it's our
guinea pig for Clay.

## Phases

Phase ordering is a proposal, not a commitment — reorder freely.

### PR #1 — foundation + mouse interaction + hot_t (uncommitted)

- [x] New `clients/silencer/src/ui/v2/` directory
- [x] `node.h` — `Node` + `NodeKind` + `ButtonType` + factories
      (`Group`, `Background`, `Sprite`, `Label`, `Button`)
- [x] `context.h` — `Context { Resources&, logical_w, logical_h, scale,
      version, mouse_x, mouse_y, state*, dt }`
- [x] `render.h` / `render.cpp` — tree-walking render pass
- [x] `screens/main_menu.{h,cpp}` — declarative `BuildMainMenu` +
      `MainMenuHandlers`
- [x] `preview.cpp` — `Game::RunPreview()` harness with mouse, dt,
      UIState lifecycle
- [x] `Renderer::DrawSpriteAt` public helper (only engine touch)
- [x] CLI flags on `Silencer.exe`:
      `--preview-screen NAME --preview-impl v2|legacy --dump-ppm PATH --preview-scale N`
- [x] CMakeLists.txt include paths for `src/ui/v2{,/screens}`
- [x] `CMAKE_EXPORT_COMPILE_COMMANDS ON` for clangd
- [x] PPM dump capability (P6, palette-indexed → RGB)
- [x] `clients/silencer/src/ui/v2/CLAUDE.md` + `AGENTS.md` (stub)
- [x] Byte-identical PPM verification: v2 and legacy match for
      MainMenu (still passes pre-Clay; will diverge post-Clay)
- [x] `dispatch.{h,cpp}` — `ButtonHit` + `DispatchClick`
- [x] `button_chrome.h` — shared chrome facts (used by render +
      dispatch)
- [x] `ui_state.h` — `UIState { anim, seen_this_frame }`,
      `BeginFrame/EndFrame` (stale-ID GC), `Approach()` helper,
      FNV-1a `Hash64`, tag constants (`TAG_HOT`)
- [x] Ryan Fleury hot_t animation per Button (exponential approach,
      maps to chrome res_index step + brightness)
- [x] Mouse + dispatch wired in preview (window→logical scaling,
      mouse-down fires `DispatchClick`)
- [ ] Commit + push + open PR (deferred — likely bundle with PR #1.5)
- [ ] In-session code review (spec + style passes via subagents)

### PR #1.5 — Clay layout + MainMenu redesign (in progress)

User chose to land Clay now rather than later, and redesign MainMenu
in the same chunk (see "Pixel-parity decision" above).

- [x] Vendor Clay v0.14 at
      `clients/silencer/third_party/clay/clay.h` (4393 lines).
      **Header gate:** Clay's `clay.h` requires C99 / C++20 / MSVC.
      Windows MSVC at C++14 passes via `_MSC_VER`; macOS / Linux at
      C++14 fail. See "C++ standard upgrade" decision below.
- [x] Bulk-rename bare `abs(...)` → `std::abs(...)` across 22
      legacy `.cpp/.h` files (100 sites). Improves correctness even
      at C++14, prep for any future standard bump. Files touched:
      `actors/{bodypart,civilian,guard,player,playerai,robot}.cpp`,
      `gas/gasloader.h`, `objects/platform.cpp`,
      `projectiles/{blaster,grenade,laser,plasma,rocket,shrapnel,wall}*.cpp`,
      `render/{palette,renderer}.cpp`,
      `ui/components/interface.cpp`,
      `ui/screens/options/options_controls_screen.cpp`,
      `world/{hittable,map,world}.cpp`.
- [ ] **Decide C++ standard.** This is the unblocker for everything
      below. See "C++ standard upgrade attempt" below for what we
      learned trying C++20 in the Windows unity build.
- [ ] `clay_impl.cpp` (or `.c`) defining `CLAY_IMPLEMENTATION` once
- [ ] Add `clay_impl` source to CMake + include path for
      `third_party/clay/`
- [ ] `MeasureSpriteText` callback wiring to sprite-font glyph width
      (font is fixed-width per bank; cfg.fontId encodes the bank)
- [ ] New `NodeKind`s: `VStack`, `HStack`, `Center`, `Padding`,
      `Spacer`. Chainable `.gap(n)`, `.padding(n)`.
- [ ] `Node` fields: `rect_x/y/w/h` (filled by Layout, read by
      Render + Dispatch). `rect_w == 0` ⇒ "no layout, fall back to
      `(x, y)` + `ChromeFor.width/height`" — keeps the absolute
      `.at()` escape hatch for migrated screens.
- [ ] `layout.{h,cpp}` — `Layout(root, ctx)` walks tree, emits CLAY
      scopes per kind, calls `Clay_BeginLayout` / `Clay_EndLayout`,
      reads back rects via `Clay_GetElementData(id)`.
- [ ] Render uses computed rects (Sprite/Label/Button); dispatch
      uses computed rects (`ButtonHit` reads `rect_*` when set).
- [ ] Port `BuildMainMenu` to `Background → Center → VStack(buttons)`.
      Logo `Sprite(208, 60)` and version label keep `.at()` (anchor
      semantics — top-of-screen sprite + bottom-of-screen label).
- [ ] Capture v2 PPM at 640×480 as the new reference; commit to
      `clients/silencer/test/golden/main_menu_v2.ppm` (or similar).
      Eyeball-approve once.
- [ ] Smoke-test interactive launch + try a window resize (drag the
      SDL window) to confirm relayout works.

#### C++ standard upgrade attempt (2026-05-10)

User chose **C++20** to unblock Clay cross-platform without patching
the vendored header. Attempted, then reverted to C++14 to leave the
working tree green. What we learned:

- **MSVC C++20 + unity build breaks `std::abs(int)` resolution.**
  Even with all 100 `abs()` sites qualified to `std::abs(...)`, MSVC
  reports "ambiguous" against the C global `int abs(int)` (from
  `corecrt_math.h`) plus `long abs(const long) noexcept` and
  `__int64 abs(...)` (from `stdlib.h`) AND the std float/double
  overloads (from `cstdlib`). Notes show 6 candidates; an exact-int
  match exists but isn't picked.
- A **minimal isolated repro** (`std::abs((signed)(int_expr))` with
  `<cstdlib>` + `<math.h>` at C++20) builds clean. So the trigger is
  something in the unity TU's earlier headers polluting lookup —
  likely a `using ::abs;` interaction or an SDL3 / vcpkg header. Not
  diagnosed before pause.
- Likely sites lurking beyond `abs`: C++20 also tightens warnings on
  `'>': unsafe use of type 'bool'` (already seen as a warning at
  `events.cpp:174`), and `volatile` deprecations.

**Continuation must be cross-platform** — no Windows-only path.
Next session is on macOS (Apple Clang). Two live options:

1. **Push through C++20** (user's original pick). Diagnose the
   unity-batch interaction that breaks `std::abs(int)` lookup on
   MSVC, fix it, then sweep any other C++20 strictness issues
   (`volatile` deprecations, `'>': unsafe use of type 'bool'` at
   `events.cpp:174`, etc). On macOS Clang the abs problem may not
   reproduce — diagnose there first; fix may be Clang-trivial and
   the MSVC fallout addressed in CI separately.
2. **C++17 + 1-line Clay guard patch.** Verified C++17 builds
   clean on the Windows unity build (only one pre-existing
   unrelated warning); macOS Clang at C++17 should be at least as
   permissive. One-line patch to Clay's `clay.h` adding
   `|| (defined(__cplusplus) && __cplusplus >= 201703L)` to the
   guard. Document the diff in `third_party/clay/CLAUDE.md` so
   future Clay version bumps re-apply it.

### PR #2 — wire v2 MainMenu into the live engine

- [x] Input-dispatch shape: `Context.mouse_{x,y}` + `DispatchClick`
      on mousedown edge (preview-only so far)
- [x] Animation model: Ryan Fleury `hot_t`/`active_t` per ID, stored
      in `UIState`, exponentially approached each frame. Replaces
      the legacy 4-frame ACTIVATING/DEACTIVATING ramp.
- [ ] Keyboard / gamepad routing for v2 Node trees
      (focus = single hashed-ID slot in `UIState`; nav walks the
      tree to find the next focusable peer)
- [ ] Replace `MAINMENU` state-handler `PushScreen(MainMenuScreen)`
      with v2 path; live game renders via v2 (engine owns its own
      `UIState`, threaded into `Context`)
- [ ] Delete `MainMenuScreen` class + files
- [ ] Use the committed v2 MainMenu reference PPM as the gate
      (replaces the legacy diff)

### PR #3+ — migrate remaining screens

One PR per screen; each ships byte-identical against its legacy
counterpart before the legacy class gets deleted. Screens use Clay
containers when their layout is naturally container-shaped; they keep
absolute `.at()` when faithfully replicating a quirky legacy layout.
The carve-out from "byte-identical merge gate" applies only to
MainMenu — every other screen still has to match pixel-for-pixel
unless the user explicitly OKs a redesign for it too.

- [ ] `OptionsScreen` (the sub-router)
- [ ] `OptionsDisplayScreen`
- [ ] `OptionsAudioScreen`
- [ ] `OptionsControlsScreen`
- [ ] `LobbyConnectScreen`
- [ ] `MissionSummaryScreen`
- [ ] `UpdateScreen`
- [ ] `LobbyScreen` + the five panels under `screens/lobby/panels/`
- [ ] `MessageModal` / `PasswordModal`

### PR N+1 — in-game UI (chat / buy / tech)

- [ ] Migrate chat input out of `Player::Tick` (`actors/player.cpp`)
- [ ] Migrate buy menu
- [ ] Migrate tech menu
- [ ] Remove `Game::ProcessInGameInterfaces`
- [ ] Strip `Player::chatinterfaceid` / `buyinterfaceid` / `techinterfaceid`

### PR N+2 — Path B (resolution-flexible)

- [ ] `ui_scale = ChooseScale(window_h)` in renderer
- [ ] `Context.logical_{w,h} = window / scale`; `Context.scale` plumbed
- [ ] Renderer blits sprites at integer scale
- [ ] Hit-test divides input coords by `ui_scale`
- [ ] Drop the fixed-640×480 SDL3 GPU upscale path for the UI
      subsystem (world scene may keep it)
- [ ] Decide fate of `cfg.scalefilter` ("Smooth Scaling" toggle)

### Cleanup PR (after all screens migrated)

- [ ] Delete `clients/silencer/src/ui/components/`
- [ ] Delete `clients/silencer/src/ui/panels/`
- [ ] Delete `clients/silencer/src/ui/modals/`
- [ ] Delete `clients/silencer/src/ui/screens/` (legacy implementations)
- [ ] Delete widget-type `switch` arms in `interface.cpp` and
      `renderer.cpp`
- [ ] Retire `Game::currentinterface` global if no remaining use
- [ ] Rename `src/ui/v2/` → `src/ui/`

## Discovered quirks (worth remembering)

- **`PALETTE.BIN` palette 1 index 114 = (0, 0, 0).** Top-of-screen
  pixels in MainMenu are space (black) — *not* a missing render. The
  bg is space + Mars planet art; the orange you see is the Mars
  surface lower down. Don't mistake pixel-(0, 0)=black for "bg
  isn't rendering."
- **Logo animation state.** Bank 208 frame 0 has stale `.enabled` /
  `www.won.net` text baked in (pre-animation asset slot). v2 renders
  index 60 (the steady-state frame). Legacy needs ≥ 61 `Object::Tick`
  calls so `Overlay::Tick` for bank 208 advances `state_i` past the
  ramp.
- **Button defaults need a Tick.** `Button::Tick` sets the type-default
  `res_index` (e.g. 7 for B196x33). Without it, buttons render with
  `Sprite::res_index = 0` (the bg sprite slot) and look very wrong.
  `world.TickObjects()` once is enough for buttons specifically.
- **Renderer::Rect is private** but `BlitSurface` takes it. Added
  `Renderer::DrawSpriteAt(target, bank, index, anchor_x, anchor_y)`
  as the public entry point; v2's render path calls only that +
  `DrawText`.
- **`UIState::EndFrame` GCs by visit-tracking.** `BeginFrame()` clears
  `seen_this_frame`; `AnimSlot()` records visits; `EndFrame()` removes
  any entry not seen. If a render path skips a button (e.g. behind a
  modal) its `hot_t` slot vanishes — that's intentional. Re-entering
  the screen re-seeds at 0.
- **Hover snap when `ctx.state == NULL`.** PPM dump path passes a
  null `UIState`. Render's hot_t falls back to `hovered ? 1 : 0` so
  the static frame is "fully hovered or not" with no time component
  — keeps snapshots deterministic.
- **Default Button key.** `Button(text)` factory auto-fills
  `n.key = "btn:" + text`. Cheap, unique-per-screen for normal use,
  collisions are visible in any state-debug dump. Override with
  `.withKey("dialog:quit:ok")` when text isn't unique.
- **MSVC env for builds.** `cl.exe` isn't on PATH by default; agent
  needs to import VS 2022 BuildTools env from
  `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\
  Common7\Tools\VsDevCmd.bat -arch=x64 -no_logo`. Snippet in cheat-
  sheet below. Each PowerShell invocation is its own shell — env
  doesn't persist across tool calls, so chain in one PowerShell
  block.
- **Clay header gate.** Clay v0.14's `clay.h` errors if
  `__cplusplus < 202002L && !defined(_MSC_VER) && !C99`. C++14 +
  MSVC passes via the `_MSC_VER` arm; C++14 + GCC/Clang fails. Our
  Windows build is fine; Linux/macOS need the cross-platform plan
  in PR #1.5's checklist.

## Build & verify cheat-sheet

**MSVC env import** (each PowerShell call is a fresh shell, so chain
the import + the work in one block):

```powershell
$bat = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
cmd /c "`"$bat`" -arch=x64 -no_logo > nul 2>&1 && set" |
  Where-Object { $_ -match '^(PATH|INCLUDE|LIB|LIBPATH|VCINSTALLDIR|VSINSTALLDIR|WindowsSdkDir|UCRTVersion|VCToolsInstallDir|UniversalCRTSdkDir|WindowsSDKLibVersion|WindowsSDKVersion)=' } |
  ForEach-Object { $kv = $_ -split '=', 2; Set-Item -Path "Env:$($kv[0])" -Value $kv[1] }
$env:VCPKG_ROOT = "C:\Users\Space Command\vcpkg"
# ... now run cmake, the binary, etc. in this same call
```

**Build:**

```powershell
# From clients/silencer/ (after env import above)
cmake --preset win-ninja-unity            # first time only / new files added
cmake --build --preset win-ninja-unity --parallel    # ~5-15 s incremental
```

**Interactive preview (mouse hover + click):**

```powershell
$exe = "C:\Users\Space Command\repos\zSilencer\.worktrees\hv-layout\clients\silencer\build-unity\Silencer.exe"
& $exe --preview-screen main_menu --preview-impl v2
```

**Pixel-diff verification (must show "no differences" pre-Clay; will
diverge post-Clay for MainMenu — see "Pixel-parity decision"):**

```powershell
$bd  = "C:\Users\Space Command\repos\zSilencer\.worktrees\hv-layout\clients\silencer\build-unity"
Set-Location $bd
& $exe --headless --preview-screen main_menu --preview-impl v2     --dump-ppm v2.ppm
& $exe --headless --preview-screen main_menu --preview-impl legacy --dump-ppm legacy.ppm
fc.exe /b v2.ppm legacy.ppm
```

## Open decisions waiting on the user

Mirror of the design doc's "Open questions" — listed here too so the
checklist is self-contained:

- **C++ standard upgrade for Clay cross-platform.** Clay needs C99,
  C++20, or MSVC. C++14 + MSVC works (Windows), C++14 + GCC/Clang
  fails. Either bump project to C++17/20, or compile Clay impl as
  C99 in a `.c` file and bridge through a wrapper header.
- Bake sprite anchor offsets out of asset pipeline? (Affects layout
  primitives' coordinate system. Now that Clay is landing, becomes
  more relevant — Clay-positioned children inherit the anchor mess
  unless we strip offsets at asset-load time.)
- Hot-reload mechanism beyond restart-on-rebuild? (`dlopen` swap of
  the screen as a shared lib is on the table if iteration feels slow.)
- `UIState` lifetime / scope (per-screen, per-session, stack alongside
  screen stack). Decide when in-game UI lands.
- Logo animation parity (triangle-wave bank-208 frame ramp; doesn't
  fit `hot_t`/`active_t` naturally).
- When to migrate in-game UI (chat / buy / tech)?

---

## Handoff prompt — paste into a new session to continue

> *(Rewrite this block every session before stopping. The next session
> reads only this block + the linked design doc to pick up cold.)*

I'm continuing the **UI v2 declarative layout rewrite** for the
Silencer client (`clients/silencer/`, C++/SDL3). **Cross-platform
build is required** — you're on macOS (Apple Clang); the same code
also has to compile on Windows (MSVC) and Linux (GCC/Clang) via CI.
Read the design doc at
`docs/plans/2026-05-10-ui-v2-declarative-layout.md` and the progress
doc at `docs/plans/2026-05-10-ui-v2-progress.md` (this file). The
"Phases" section is the checklist; "Discovered quirks" is gotcha
context.

**Current task: continue Clay implementation (PR #1.5).**

### What's in the branch

Branch `hv/layout` (open PR — `gh pr view` to find it). Two
just-pushed commits sit on top of the foundation commit `afd6b98`:

1. **Mouse interaction + `hot_t` hover animation** in v2 preview.
   New files: `src/ui/v2/dispatch.{h,cpp}`, `button_chrome.h`,
   `ui_state.h`. Modified: `node.h` (key field), `context.h`
   (UIState*, mouse_x/y, dt), `render.cpp` (hot_t mapping to chrome
   res_index + brightness), `preview.cpp` (SDL mouse events,
   DispatchClick, dt + UIState lifecycle), `screens/main_menu.{h,cpp}`
   (MainMenuHandlers struct, default Button auto-key to `"btn:" + text`).
2. **Clay vendored + `abs(...)` → `std::abs(...)` cleanup.** Clay
   v0.14 single header at `clients/silencer/third_party/clay/clay.h`
   (4393 lines). `std::abs` qualification across 22 files (100
   sites) — improves correctness regardless of standard, prep for
   any future bump.

`CMakeLists.txt` is at `cxx_std_14` (working state).

### Step 0 — pick the C++ standard (UNBLOCKER)

Clay v0.14's header gate requires C99 / C++20 / MSVC. C++14 + Apple
Clang **fails** the gate. You can't include `clay.h` until this is
resolved. **Two options:**

- **C++20** (user's original pick). Bump `cxx_std_14 → cxx_std_20`
  in `CMakeLists.txt`. On macOS Clang this likely Just Works.
  Caveat: Windows MSVC unity build hits `std::abs(int)` ambiguity
  in the unity TU even after the cleanup — root cause not
  diagnosed before pause; minimal isolated repro of `std::abs((signed)
  expr)` builds clean at C++20, so something in the unity batch's
  earlier headers poisons lookup. May need a workaround. Verify
  macOS first; address Windows in CI / a follow-up commit.
- **C++17 + 1-line Clay guard patch.** C++17 was verified clean on
  the Windows unity build before pause. Patch `clay.h` line ~32
  from `(defined(__cplusplus) && __cplusplus >= 202002L)` to
  `(defined(__cplusplus) && __cplusplus >= 201703L)`. Document the
  diff in `third_party/clay/CLAUDE.md` so future Clay bumps
  re-apply. Bump `cxx_std_14 → cxx_std_17`.

Ask user before picking if unclear. Don't pick a Windows-only path.

### Steps 1–N — Clay implementation

Once Clay's gate is satisfied, proceed:

1. `clay_impl.cpp` (or `.c`) defining `CLAY_IMPLEMENTATION` exactly
   once. Add to CMake sources + add `third_party/clay/` include path
   in `CMakeLists.txt` next to existing `third_party/` entry.
2. Add new `NodeKind`s (`VStack`, `HStack`, `Center`, `Padding`,
   `Spacer`) + `rect_x/y/w/h` fields on `Node` + `.gap(n)` /
   `.padding(n)` chainables.
3. `layout.{h,cpp}`: `Layout(Node&, const Context&)` walks tree,
   emits CLAY scopes per kind, calls `Clay_BeginLayout` /
   `Clay_EndLayout`, reads back rects via `Clay_GetElementData(id)`
   into `node.rect_*`. Use FNV-1a hash of `node.key` as Clay's
   element ID (already have `Hash64` in `ui_state.h`). Init Clay
   arena once at first call.
4. `MeasureSpriteText` callback: fixed-width font, returns
   `{text.length * cfg.fontSize, line_height}`. Wire via
   `Clay_SetMeasureTextFunction(MeasureSpriteText, &resources)`.
5. Update `render.cpp` and `dispatch.cpp` to use `node.rect_*` when
   `rect_w > 0`; fall back to `n.x/n.y` + `ChromeFor.width/height`
   when not (legacy `.at()` escape hatch — keeps absolute positioning
   working for migrated screens that need pixel parity).
6. Port `BuildMainMenu` to `Background → Center → VStack(buttons).gap(N)`.
   Logo `Sprite(208, 60)` keeps `.at()` (sprite-anchor-driven).
   Version label keeps `.at(10, ctx.logical_h - 17)` (bottom-left
   anchored).
7. **Drop the v2-vs-legacy byte-identity gate for MainMenu** in the
   preview path (the design intentionally diverges — see "Pixel-
   parity decision" in this doc). Capture the new v2 PPM at 640×480
   as `clients/silencer/test/golden/main_menu_v2.ppm`. Get user
   eyeball-approval.
8. Smoke-test interactive launch + drag-resize the SDL window to
   confirm relayout works.

### Quirks to read first

See "Discovered quirks" section. Highlights:

- `UIState::EndFrame` GCs unvisited slots — animation state vanishes
  for hidden nodes (intentional).
- Render snaps when `ctx.state == NULL` (PPM dumps stay deterministic).
- Button factory auto-keys to `"btn:" + text`. Collisions need
  explicit `.withKey(...)`.
- Logo bank 208 animation frame, palette 1 index 114 = black is
  intentional (don't mistake for missing render).

### Build / verify (OS-agnostic)

Repo root has top-level docs and CI workflows; this PR's changes
live under `clients/silencer/`. From the repo root:

```bash
cd clients/silencer

# macOS / Linux
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Windows (PowerShell, MSVC env imported via VsDevCmd.bat — snippet
# in "Build & verify cheat-sheet" above)
cmake --preset win-ninja-unity        # first time / new files
cmake --build --preset win-ninja-unity --parallel
```

The `Silencer` binary runs the v2 preview via:

```bash
# macOS:    ./build/Silencer.app/Contents/MacOS/Silencer ...
# Linux:    ./build/silencer ...
# Windows:  build-unity\Silencer.exe ...

<binary> --preview-screen main_menu --preview-impl v2
<binary> --headless --preview-screen main_menu --preview-impl v2 --dump-ppm v2.ppm
```

### User preferences (from memory)

Terse responses, no narrating internal deliberation, lightweight
plans during refactors, PR before review, don't invent constraints.
**Cross-platform is non-negotiable** — never propose a Windows-only
or macOS-only path; the project ships on all three.
