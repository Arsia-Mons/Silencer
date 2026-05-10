# UI v2 — progress + handoff

**Companion design doc:** [2026-05-10-ui-v2-declarative-layout.md](2026-05-10-ui-v2-declarative-layout.md).

This doc is **mutable**. Each session updates the checklist below and
rewrites the "Handoff prompt" section at the bottom so the next
session can pick up cold.

---

## Status

- **Current branch:** `hv/layout` (worktree at `.worktrees/clay/`)
- **CMake `cxx_std_20`.** Bumped this session; macOS Apple Clang
  builds clean with no source changes beyond the cxx flag. Windows
  MSVC unity build still has an unresolved `std::abs(int)` ambiguity
  in the unity TU — to be addressed in a follow-up (no Windows-only
  workaround in tree).
- **PR #1.5 (this session):** Clay layout engine wired into the
  build. `clay_impl.cpp` defines `CLAY_IMPLEMENTATION` once;
  `layout.{h,cpp}` walks the Node tree and emits Clay scopes for
  container kinds, reading back rects via `Clay_GetElementData`.
  New container `NodeKind`s: `VStack`, `HStack`, `Center`,
  `Padding`, `Spacer`. New `Node` fields: `rect_x/y/w/h`, `gap`,
  `pad`. Chainable `.withGap(n)` / `.withPadding(n)`. Render +
  dispatch consume `rect_*` when populated (`rect_w > 0`); fall
  back to absolute `.at()` + sprite-anchor when not.
- **MainMenu:** kept on the absolute `.at()` path (no Clay
  containers). Rationale: legacy MainMenu's staggered button
  positions (per-row x offsets, logo overlapping the center column)
  are not naturally container-shaped, and the design doc's `.at()`
  escape hatch is the right tool. **Byte-identical to legacy
  preserved** — this reverses the prior session's "Pixel-parity
  decision" carve-out (see below).
- **PR #1** (foundation + mouse interaction + `hot_t` animation):
  committed pre-session (commit `afd6b98`).

## Pixel-parity decision — REVERSED (2026-05-10)

The prior plan was to carve MainMenu out of the byte-identity gate
and ship a centered VStack redesign. **User reversed this** mid-
session: "You must be able to match the original placement. If
clay is not expressive enough for this then it's not a viable
path." The staggered legacy layout (buttons at chrome-relative
(350,154), (390,221), (350,288), (310,355) — alternating x offsets
of 0/+40/0/-40) isn't a natural container shape; expressing it
through Clay would require floating-with-offset per button, which
is absolute positioning wearing a costume.

Resolution: **MainMenu uses absolute `.at()` per the design doc's
documented escape hatch** ("screens keep absolute `.at()` when
faithfully replicating a quirky legacy layout"). Clay is wired in
and exercised by `Layout()` every frame (with zero scopes emitted
for the MainMenu subtree), ready for the first screen that has a
container-natural layout — Options is a likely candidate. Byte-
identity vs legacy is restored as the MainMenu merge gate.

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

### PR #1.5 — Clay layout wired in (done; PR open)

User chose C++20 to satisfy Clay's cross-platform header gate. Clay
is wired in and exercised by `Layout()` every frame; MainMenu stays
on the absolute `.at()` path per the design doc's escape hatch (see
"Pixel-parity decision — REVERSED" above). First Clay container use
will land with the first container-shaped screen (Options is a
likely candidate).

- [x] Vendor Clay v0.14 at
      `clients/silencer/third_party/clay/clay.h` (4393 lines).
- [x] Bulk-rename bare `abs(...)` → `std::abs(...)` across 22
      legacy `.cpp/.h` files (100 sites).
- [x] **Decide C++ standard:** C++20. macOS Apple Clang clean with
      only the cxx flag bumped; Windows MSVC unity build still has
      the `std::abs(int)` ambiguity from the prior session — to be
      diagnosed in a follow-up commit. No Windows-only path in tree.
- [x] `clay_impl.cpp` defining `CLAY_IMPLEMENTATION` once (in
      `src/ui/v2/`).
- [x] `third_party/` is already on the include path; sources are
      `GLOB_RECURSE`d so `clay_impl.cpp` is picked up automatically.
- [x] `MeasureSpriteText` callback in `layout.cpp` — fixed-width
      per-glyph advance from `cfg->fontSize`, line height from
      `cfg->lineHeight`. Wired via `Clay_SetMeasureTextFunction`.
- [x] New `NodeKind`s: `VStack`, `HStack`, `Center`, `Padding`,
      `Spacer`. Chainable `.withGap(n)` / `.withPadding(n)`.
- [x] `Node` fields: `rect_x/y/w/h` filled by Layout, read by
      Render + Dispatch. `rect_w == 0` ⇒ "no layout, fall back to
      absolute `.at()` + `ChromeFor.width/height`" — the design
      doc's documented escape hatch.
- [x] `layout.{h,cpp}` — `Layout(root, ctx)` walks tree, emits CLAY
      scopes per container kind, calls `Clay_BeginLayout` /
      `Clay_EndLayout`, reads back rects via `Clay_GetElementData`.
      Single per-process arena, `Clay_MinMemorySize()` bytes,
      malloc'd on first call. IDs use `Clay_GetElementIdWithIndex`
      with a static base string + per-emit counter.
- [x] Render uses computed rects (`rect_w > 0`) for Buttons inside
      Clay subtrees; dispatch's `ButtonHit` reads `rect_*` for hit
      test on the same condition. Both paths fall back to absolute
      `.at()` + sprite anchor when `rect_w == 0`.
- [x] `BuildMainMenu`: kept on absolute `.at()`. Layout runs but
      emits zero scopes for this tree. Byte-identical to legacy
      preserved (verified via PPM cmp).
- [x] Preview wired to call `Layout(tree, ctx)` before both render
      and dispatch — required since hit-test consults `rect_*`.
- [x] Smoke-test: interactive launch verified (binary opens, no
      Clay errors). Window-resize relayout test deferred until a
      screen actually uses container layout.

#### C++20 bump status

- **macOS (Apple Clang 17):** clean. Only the `cxx_std_14 →
  cxx_std_20` flip in `CMakeLists.txt:75` was needed. v2 PPM
  byte-identical before vs after the bump.
- **Windows MSVC unity build:** still hits the `std::abs(int)`
  ambiguity from the prior session even after the 100-site
  `std::abs(...)` qualification. Trigger appears to be header-
  ordering pollution inside a unity TU (a minimal isolated repro at
  C++20 builds clean). Not yet diagnosed. **Open follow-up.**
- **Linux GCC/Clang:** not exercised locally; CI will surface.

If the MSVC fallout proves intractable, the live fallback is C++17
+ a 1-line patch to Clay's `clay.h` guard, documented in
`third_party/clay/CLAUDE.md`. Both paths leave macOS+Linux clean.

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

- ~~C++ standard upgrade for Clay cross-platform.~~ **Decided
  2026-05-10: C++20.** macOS clean; Windows MSVC unity build needs
  follow-up diagnosis on `std::abs(int)` ambiguity.
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
Silencer client (`clients/silencer/`, C++20/SDL3). **Cross-platform
build is non-negotiable** — your environment is macOS (Apple Clang);
the same code also has to compile on Windows (MSVC) and Linux
(GCC/Clang) via CI. Read the design doc at
`docs/plans/2026-05-10-ui-v2-declarative-layout.md` and the progress
doc at `docs/plans/2026-05-10-ui-v2-progress.md` (this file). The
"Phases" section is the checklist; "Discovered quirks" is gotcha
context.

### What's done

Branch `hv/layout` (PR #153). PR #1.5 is **complete on macOS** and
ready to commit/push. Clay is fully wired in:

- C++20 (`CMakeLists.txt` `cxx_std_20`). macOS Apple Clang clean.
- `clay_impl.cpp` defines `CLAY_IMPLEMENTATION` once.
- `layout.{h,cpp}` walks the Node tree, emits Clay scopes for
  container kinds, reads back rects via `Clay_GetElementData`.
  Single per-process arena, lazily initialized.
- New `NodeKind`s: `VStack`, `HStack`, `Center`, `Padding`, `Spacer`.
- New `Node` fields: `rect_x/y/w/h`, `gap`, `pad`. Chainable
  `.withGap(n)` / `.withPadding(n)`.
- Render + dispatch consume `rect_*` for layout-managed buttons;
  fall back to absolute `.at()` + sprite-anchor when `rect_w == 0`.
- `MeasureSpriteText` Clay callback in `layout.cpp`.
- MainMenu **kept on absolute `.at()` path** — byte-identical to
  legacy preserved (see "Pixel-parity decision — REVERSED" above
  for why we did not redesign).

### Open follow-ups before merging

1. **Windows MSVC `std::abs(int)` ambiguity** in the unity build at
   C++20. Same issue the prior session pre-pause hit; not diagnosed.
   On macOS the bump is clean. Live fallback: C++17 + 1-line Clay
   `clay.h` guard patch (documented in the C++20 section above).
   Address in CI or a follow-up commit before merging.
2. **Linux build** not exercised locally — CI will surface.
3. **Commit + push.** The branch has staged changes for clay_impl,
   layout, node container kinds, render/dispatch rect consumption,
   main_menu revert, CLAUDE.md/CMakeLists.txt C++20 bump, and this
   progress doc. Run `git status` to see exactly what's pending.

### Next phase — PR #2 (engine integration)

Replace the live MAINMENU state-handler `PushScreen(MainMenuScreen)`
with the v2 path; live game renders via v2. Engine owns a `UIState`,
threaded into `Context`. Then delete `MainMenuScreen` + components
it was the last user of. See "PR #2" in the Phases section.

After that, PR #3+ migrate the remaining screens one at a time;
screens that have natural container layouts (Options sub-router is a
likely first candidate) actually exercise the new Clay containers.

### Quirks to read first

See "Discovered quirks" section. Highlights:

- `UIState::EndFrame` GCs unvisited slots — animation state vanishes
  for hidden nodes (intentional).
- Render snaps when `ctx.state == NULL` (PPM dumps deterministic).
- Button factory auto-keys to `"btn:" + text`. Collisions need
  explicit `.withKey(...)`.
- Logo bank 208 animation frame, palette 1 index 114 = black is
  intentional (don't mistake for missing render).
- Clay's `Layout()` is safe to call on any tree — if there are no
  container nodes, zero Clay scopes get emitted and the tree falls
  through to the absolute `.at()` path.

### Build / verify (OS-agnostic)

```bash
# macOS / Linux (from repo root)
cmake -B build -S clients/silencer -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Windows (PowerShell, MSVC env imported via VsDevCmd.bat — snippet
# in "Build & verify cheat-sheet" above)
cmake --preset win-ninja-unity        # first time / new files
cmake --build --preset win-ninja-unity --parallel
```

Preview / verify:

```bash
# macOS:    ./build/Silencer.app/Contents/MacOS/Silencer ...
# Linux:    ./build/silencer ...
# Windows:  build-unity\Silencer.exe ...

<binary> --preview-screen main_menu --preview-impl v2
<binary> --headless --preview-screen main_menu --preview-impl v2 --dump-ppm v2.ppm
<binary> --headless --preview-screen main_menu --preview-impl legacy --dump-ppm legacy.ppm
cmp v2.ppm legacy.ppm   # MUST be byte-identical (MainMenu still on .at() path)
```

### User preferences (from memory)

Terse, no narrating internal deliberation, lightweight plans during
refactors, PR before review, don't invent constraints. **Cross-
platform is non-negotiable** — never propose a Windows-only or
macOS-only path. **Pick the right architecture, not a stepping-
stone we'd later replace** — this is why the prior session's
MainMenu-redesign carve-out was reversed: forcing a quirky legacy
layout through containers would have been a costume-wearing hack.
