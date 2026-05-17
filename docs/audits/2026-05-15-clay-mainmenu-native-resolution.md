# Clay UI does not scale to native display resolution — main menu

**Status:** resolved on `hv/clay-ui-migration` after review follow-up
**Branch:** `hv/clay-ui-migration` (finding captured at `1625f0f`)
**Baseline compared:** `origin/main` (`b934bf7`)
**Captured on:** primary display 2560×1440 (the desktop's native resolution)
**Relates to:** `docs/audits/2026-05-14-clay-ui-visual-regression.md`
(this extends that audit's "button-shape divergence" with a more
serious resolution-scaling regression)

## Resolution

The responsive follow-up renders menu UI in a compositor-owned virtual
coordinate space, magnifies it into a native-pixel region, and maps pointer
input through the same pixel-size/offset transform. Window resize handling
follows `SDL_GetWindowSizeInPixels()` /
`SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` for HiDPI drawables.

In-game rendering is intentionally back on the `origin/main` presentation
model: build one 640×480 paletted frame, including Clay HUD/overlays, then let
the render backend stretch that frame to the swapchain. A short-lived native
CPU upscale/composite path made fullscreen expensive and, after review
feedback, centered/aspect-corrected the world instead of filling the screen.
That path was removed on 2026-05-16.

The legacy `Config::scalefilter` final-present toggle remains the in-game
scaling mechanism. Menu surfaces can still render at native size because they
are not the hot gameplay frame path.

## Original Finding

At the desktop's native 2560×1440, the Clay-migrated main menu did not lay out
to the viewport: background art was pinned at native asset pixel size in the
top-left, ~85% of the screen was empty, and the button column was a tiny fixed
156×120 px block. Legacy `origin/main` filled the screen correctly at the same
display.

## Evidence

`docs/audits/2026-05-15-clay-mainmenu-native-resolution/`

| File | What it is |
|---|---|
| `00_composite_sidebyside.png` | side-by-side, start here |
| `01_clay_mainmenu_2560x1440_native.png` | this branch, true native 2560×1440 render |
| `02_originmain_640x480_upscaled_asseen.png` | origin/main as it appears fullscreen on this monitor (640×480 → 2560×1440, nearest, stretched — matches saved `scalefilter=0`) |
| `03_originmain_640x480_raw.png` | origin/main raw native render (its only resolution) |

Clay side: logo + planet/starfield occupy roughly the top-left
~512×400; buttons are a small block at ~center-right; the rest is
black. Legacy side: SILENCER wordmark spans the left, planet centered,
four large rounded buttons fill the right — coherent and full-screen.

## Why the two sides aren't symmetric (read before drawing conclusions)

- **Clay branch** has `Game::ResizeRenderSurface()`
  (`clients/silencer/src/game/game.cpp:354`, dispatched in
  `clients/silencer/src/net/controldispatch.cpp:325`). The render
  surface was resized to 2560×1440 and Clay genuinely renders at that
  size — this is a true native render, not an upscale.
- **origin/main** has no such op. `screenbuffer` is hardcoded
  `Surface(640, 480)`; the legacy engine always renders 640×480 and
  the GPU stretches the whole framebuffer to the window/monitor
  (`clients/silencer/src/render/sdl3gpubackend.cpp` `Present()` upscale
  pass: fullscreen triangle, no aspect correction). So it physically
  cannot render natively at 2560×1440. The "as seen" image is a
  post-process nearest-neighbor stretch to model exactly what the GPU
  does fullscreen on this monitor (saved config: `fullscreen=1`,
  `scalefilter=0`). The blockiness/horizontal stretch on that side is
  expected and is not the regression.

The regression is entirely on the Clay side: it has the whole
2560×1440 to work with and does not use it.

## Verified root cause (main menu)

Read `clients/silencer/src/client/ui/screens/main_menu/main_menu_screen.cpp`
@ `1625f0f`. The screen is authored entirely in absolute pixels tuned
for the legacy 640×480 surface, with no scale factor derived from the
actual surface size:

- `kMenuButtonW = 156`, `kMenuButtonH = 21`, `kRootPadX = 40`,
  `kRootPadY = 32`, `kButtonGap = 12` — fixed px constants
  (lines 29–36). On 640×480 the button column is a meaningful fraction
  of the screen; on 2560×1440 it is 156 px ≈ 6% of width.
- Logo node is `CLAY_SIZING_FIXED(360) × CLAY_SIZING_FIXED(160)`
  (lines 175–178) — never scaled to surface.
- `ComputeButtonLayout(dst.w, dst.h)` (lines 53–72) positions the
  button block *proportionally* (`x = width*0.62`,
  `y = (height-total)/2`) but keeps it *fixed-size*. That's why the
  buttons are not top-left but are still tiny: at 2560×1440 → block at
  ~(1587, 660), sized 156×120.
- `MainMenuRoot` element is `CLAY_SIZING_FIXED(dst.w, dst.h)` with
  `.image = PackImage(6, 0)` (lines 154–159). The element fills the
  surface, but the rendered art stays a small top-left block —
  consistent with the background image being drawn at native asset
  resolution rather than fitted to the (now large) element.

Net: the layout was designed for a single fixed coordinate space
(legacy 640×480). The migration made the surface resizable but the
screens still assume 640×480.

## Strong hypothesis: this is systemic, not main-menu-only

`CLAY_SIZING_FIXED(...)`, hardcoded `constexpr int k… = <number>`,
and `GetScreenBuffer()`-derived absolute coordinates appear **220×
across 21 files** under
`clients/silencer/src/client/ui/screens/` (options, lobby,
lobby_connect, mission_summary, update, …). Every Clay screen looks
authored against the legacy 640×480 space. Expect the same
under-scaling on every surface at non-640×480 resolutions; the
1280×720 captures in the 2026-05-14 audit already hinted at this
("layout drift at non-native viewports").

This is a hypothesis from a count, not a per-screen verification —
confirm by capturing the other screens at 2560×1440 (see repro).

## Open questions for the investigator

1. **Image fit.** Does the Clay compositor scale an element's
   `.image` to the element box, or blit at native asset size? Find
   where `imageData`/`PackImage` is consumed (start: `clay_ui_compositor`,
   `clay_bridge::Render`, `src/ui/runtime`, `src/render`). This decides
   whether the background needs a fit/cover mode or a scaled asset.
2. **Intended scaling model.** Is Clay UI *meant* to render at the
   physical resolution (so screens need a scale factor / relative
   sizing), or is the intended design to render Clay at a fixed
   virtual canvas (e.g. 640×480 or 1280×720) and upscale like legacy
   did? These lead to very different fixes. Check the Clay migration
   intent docs under `docs/plans/` / `docs/superpowers/` and the
   `ClientUi`/`ClayService` frame setup before assuming.
3. **Scope.** Capture every reachable screen at 2560×1440 and confirm
   which degrade. Use the `e2e-visual-regression` skill
   (`shared/skills/visual-regression-journeys/`) — but note its
   scripts are macOS-flavored and default to 640×480/1280×720; the
   resolution and the origin/main 640×480-only constraint here are not
   wired into it.

## Not yet investigated (deliberately — this is a handoff)

- No fix attempted or designed. The two open-question paths above
  (physical-resolution scaling vs fixed virtual canvas + upscale) are
  a genuine design fork; do not pick one without confirming intent.
- Only the main menu was read at source level. Other screens are
  inferred from the 220/21 grep, not read.
- Image-fit behavior in the compositor is unconfirmed.

## Reproduction (OS-agnostic)

Build the client (non-unity — see build note below) and drive it
headless via the CLI (`shared/skills/cli/SKILL.md`):

```
# Clay branch — true native render
<silencer> --headless --control-port <P>
cli --port <P> wait_for_state --state MAINMENU --timeout-ms 15000
cli --port <P> resize --w 2560 --h 1440
cli --port <P> wait_ms --n 3500          # let the fade-in settle
cli --port <P> screenshot --out clay.png

# origin/main — 640x480 only; `resize` returns UNKNOWN_OP
cli --port <P> wait_for_state --state MAINMENU --timeout-ms 15000
cli --port <P> wait_ms --n 3500
cli --port <P> screenshot --out main_640.png
# model fullscreen-on-this-monitor: nearest-neighbor, stretched to fill
magick main_640.png -filter point -resize '2560x1440!' main_asseen.png
```

Substitute 2560×1440 with whatever the target display's native
resolution is. The fade-in settle wait matters — capturing too early
yields a mid-fade frame.

## Build note (will bite the next agent)

The current branch's **unity build is broken** at `1625f0f`:
`kButtonGap` / `RegisterButton` are defined in anonymous namespaces in
both `main_menu_screen.cpp` and `options_screen.cpp`; the jumbo build
merges them into one TU → `C2086 redefinition` / `C2264`. The
non-unity build compiles them as separate TUs and is clean. Build with
the non-unity release preset (`win-ninja-release`), not
`win-ninja-unity`, until that collision is resolved (likely
introduced by `1625f0f` "Refactor UI automation registry to
interactions").

---

# Investigation findings (2026-05-15) — superseded by PR fixes

Investigated at `1625f0f`, non-unity `clients/silencer/build-release`
(confirmed in sync with HEAD). This section recorded the native 2560×1440
capture findings that drove the responsive follow-up. The temporary PNG
captures were removed from the repo after review because they were
debugging evidence, not stable test baselines.

## Severity upgrade: this is a default-config, user-facing regression

The original finding was captured via the CLI `resize` op, which could
have been read as a headless/automation-only artifact. It is not.

- `Config::LoadDefaults` sets `fullscreen = true`
  (`clients/silencer/src/platform/config.cpp:81`), and the window is
  created with `SDL_WINDOW_FULLSCREEN` whenever `Config::fullscreen`
  (`clients/silencer/src/game/game.cpp:283`).
- Going fullscreen makes SDL emit `SDL_EVENT_WINDOW_RESIZED`. The
  branch's production handler
  (`clients/silencer/src/game/events.cpp:176-181`) responds with
  `SDL_GetWindowSize(...)` → `ResizeRenderSurface(w, h)`. So on a real
  user's machine, fullscreen at the desktop's native resolution resizes
  `screenbuffer` to native and Clay lays out at native — exactly the
  broken state in the captures.
- The CLI `resize` op (`controldispatch.cpp:325` → `ResizeRenderSurface`)
  merely reproduces in headless what the SDL window handler does in
  production. `origin/main` has neither `ResizeRenderSurface` nor that
  `WINDOW_RESIZED` handler, so its 640×480 buffer is always GPU-upscaled
  (intended legacy behavior). The regression was introduced by adding the
  resize plumbing **without** the screen-side responsiveness work (which,
  per the intent docs below, was explicitly deferred).

Net: essentially every user not running at exactly 640×480 — i.e. all of
them — gets the broken layout by default.

## Open question 1 — image fit: ANSWERED (blits at native asset size)

The Clay compositor blits an element's `.image` at the **sprite's native
asset size**, anchored at the element box's **top-left**. It does not
scale to the element box; `bb.width`/`bb.height` are never read for
image sizing.

`DispatchImage` — `clients/silencer/src/render/clay_ui_compositor.cpp:229-255`:

- `UnpackImage(data.imageData, bank, index)` decodes the packed
  `bank<<16 | index` (`clay_ui_payloads.h:13` `PackImage`).
- `int x = bb.x; int y = bb.y;` then `int w = src->w; int h = src->h;`
  (lines 244-247) — width/height come from the **source surface**, not
  the bounding box.
- `Renderer::Rect dstrect{w, h, x, y};
  Renderer::BlitSurface(src, nullptr, dst, &dstrect);` (lines 253-254) —
  unscaled blit.
- The code comment at lines 242-244 states the design assumption
  verbatim: *"Blit at the bbox top-left using the sprite's natural size.
  Layout is expected to size the element to match the sprite; mismatches
  would require a scaled blit which the existing pipeline doesn't
  support."*

Nuance: a scaled blit primitive **does** exist in `Renderer` and is used
by other compositor paths — `CustomKind::TeamEmblem` calls
`Renderer::DrawScaled` (`clay_ui_compositor.cpp:478`) and
`CustomKind::Sprite` does src-rect sub-blits (lines 410-457). It is only
the `CLAY_RENDER_COMMAND_TYPE_IMAGE` path that has no scaling.

Consequence for the main menu: `MainMenuRoot` is
`CLAY_SIZING_FIXED(dst.w, dst.h)` with `.image = PackImage(6, 0)`. The
element box correctly fills 2560×1440, but `DispatchImage` ignores the
box and blits sprite (6,0) at its native asset size at (0,0) — the
small top-left art block in every capture. This confirms the doc's
"drawn at native asset resolution" hypothesis at source level.

## Open question 2 — intended scaling model: a real design fork

### What the code does today

`Game::ResizeRenderSurface` (`game.cpp:354-361`) resizes the singleton
`screenbuffer` (default `Surface(640,480)`, `game.cpp:61`).
`Game::PrepareClientUiFrame` feeds `screenbuffer.w/h` into
`clientUiInput.BuildFrame(surface.w, surface.h, …)` (`game.cpp:384`),
and `ClayService::BeginFrame` calls
`backend_.SetLayoutDimensions(input.width, input.height)`
(`ClayService.cpp:14`). So **Clay genuinely lays out at the physical
screenbuffer resolution.** The `SDL3GPUBackend` upscale pass still
exists (`sdl3gpubackend.cpp:551,792-796`, nearest/linear chosen from
`Config::scalefilter` at `game.cpp:350`), but `UploadFrame` sizes
`scene_tex` to `screenbuffer.w/h` (`game.cpp:482`) — so after a resize,
`scene_tex == native == swapchain` and the upscale is a 1:1 no-op with
no compensating downscale. The branch is in a half-migrated state:
infra renders Clay at physical resolution, screens are authored for a
fixed 640×480.

### What was *intended* (the only recorded decision)

`docs/plans/2026-05-11-lobby-clay-refactor.md`:

- Line 64: the layout root is specified as `Root (640×480, fullscreen
  background image)`.
- Lines 230-232, explicit and dated: *"Out of scope for this milestone:
  runtime resizing / web-responsive layouts. **The renderer's
  framebuffer stays at 640×480.** Treat full responsiveness as yet
  another later milestone."*

`docs/superpowers/specs/2026-05-14-clay-ui-architecture-completion-design.md`
scopes four refactor moves (arenas, read models, decomposition, input);
its verification pixdiffs at 640×480 and 1280×720 but it makes **no
statement that physical-resolution rendering is the target model** —
1280×720 was a reflow regression check, not a design decision. No
document after 2026-05-11 re-decides the scaling model. **The only
written intent is: fixed 640×480 canvas + GPU upscale, responsiveness
explicitly deferred to an unstarted milestone.** The ~21 screens being
authored in fixed 640×480 px is faithful to that intent, not a bug
against it.

### The fork (surfaced, not decided — this is a product/architecture call)

**Option A — honor the recorded intent: fixed virtual canvas + upscale.**
Clay always lays out at a fixed canvas (640×480, or a chosen larger
fixed canvas such as 1280×720); composite into a fixed-size surface; the
existing `SDL3GPUBackend` pass upscales it to the window/native. The
`WINDOW_RESIZED` handler and `resize` op change only the upscale/window
target, not Clay's layout dimensions.
- *Pros:* matches the only written decision; **zero per-screen work** —
  the fixed-px screens are correct as-is; smallest change; restores
  exact legacy behavior; `scalefilter` already toggles nearest/bilinear;
  the compositor's native-size IMAGE blit (Q1) is **correct** under this
  model, no compositor change.
- *Cons:* UI is pixel-upscaled (soft/blocky at high res — this is the
  legacy aesthetic `origin/main` ships); no crisp native UI; does not
  advance the deferred responsiveness milestone.

**Option B — complete the deferred milestone: render Clay at physical
resolution.** Make all Clay screens resolution-independent (a scale
factor derived from surface size, or Clay percent/grow sizing) **and**
make the compositor IMAGE path scale `.image` to its element box (Q1:
it does not today; `Renderer::DrawScaled` already exists and is used by
the TeamEmblem custom path, so the primitive is available); decide a
fit/cover model for fullscreen background art.
- *Pros:* crisp UI at any resolution; the modern end state; infra is
  already half-there (resize plumbing + Clay laying out at physical res).
- *Cons:* this is exactly the milestone 2026-05-11 explicitly deferred
  and no later doc has authorized; touches ~21 screen files + the
  compositor IMAGE path + the fullscreen-bg-image model + (per scope
  finding below) likely the non-Clay world-render path; largest change;
  needs a design decision (scale-factor vs relative sizing) and a new
  visual-verification baseline (pixdiff-vs-640×480 stops being valid).

Both options fully fix the visual regression; neither is favored on
correctness grounds. The decision is which direction the product takes.

## Open question 3 — scope: SYSTEMIC, confirmed (not main-menu-only)

Captured every screen reachable headless without a live lobby, at native
2560×1440, non-unity `build-release`, 3.5 s fade-in settle.

| # | Screen (state) | Result at 2560×1440 |
|---|---|---|
| 10 | Main menu (`MAINMENU`) | **Degraded** — bg art native-size top-left, button block fixed 156-px-ish center-right, ~85% black. Reconfirms original finding. |
| 11 | Options root (`OPTIONS`) | **Degraded** — same pattern. |
| 12 | Options controls (`OPTIONSCONTROLS`) | **Degraded** — small fixed "Configure Controls" panel top-center. Also shows the *separate* string-arena text-garble from the 2026-05-14 audit ("fR"/"àG"/"ND"); orthogonal bug, noted not conflated. |
| 13 | Options display (`OPTIONSDISPLAY`) | **Degraded** — tiny fixed block top-center, ~90% black. |
| 14 | Options audio (`OPTIONSAUDIO`) | **Degraded** — same. |
| 15 | Update screen (`update_screen`) | **Degraded** — tiny fixed message box "An update is required to play" + Update/Cancel, no bg art, ~95% empty. (Reached via "Connect To Lobby"; this build's version handshake routes to update-required. Bonus coverage of one hypothesized screen.) |
| 16 | In-game HUD via Tutorial (`SINGLEPLAYERGAME`) | **Degraded, different mode** — the renderer's **world view + HUD render into a ~640×480-equivalent block anchored top-left** of the 2560×1440 surface; remaining ~80% black. This shows the **non-Clay world-render path also does not scale** to a resized surface. |
| 17 | In-game playerlist / buy / tech / chat overlays | Same world-block-top-left degradation as 16. Overlays themselves **not visually confirmed** — tutorial mode does not bind the stations these overlays need (matches the 2026-05-14 audit's note). Flagged, not claimed. |

**Not captured** (still only inferred from the audit's 220/21 grep, not
source-verified): the genuine `LOBBYCONNECT` credential UI, the `LOBBY`
screen, `MISSIONSUMMARY`, and the password/message modals — these need a
live lobby or a game-completion flow. Their degradation remains a
hypothesis.

**Conclusion:** the systemic hypothesis is **confirmed** for every screen
reachable without a live lobby — 6 distinct Clay screens spanning 4
screen modules, plus the in-game world/HUD path. It is not a
main-menu-only bug, and screen 16 shows the non-Clay world renderer is
in the same class of failure (relevant to Option B's true blast radius).

## Reproduction note (extends the doc's repro)

Use the CLI `resize` and `screenshot` ops from `tests/cli-agent/e2e/lib.sh`
to regenerate native-resolution captures when needed. Substitute the target
display's native resolution for 2560×1440; the original investigation
machine's primary display was 2560×1440.

---

# RESOLUTION (2026-05-15) — fixed and verified

**Status:** FIXED. Implemented and verified end-to-end at 2560×1440 and
640×480, non-unity `build-release` and unity `build-unity` (the unity
ODR break was also fixed). Evidence: `2026-05-15-clay-mainmenu-native-resolution/resolution/`.

## Direction taken (user-confirmed)

Hybrid of the two forks: **menus use responsive Clay at native
resolution** (Option B for menu UI), while **gameplay stays the legacy
fixed 640×480 frame and is stretched by the render backend** (Option A
for gameplay). Menu UI keeps
bitmap text/chrome integer-magnified (no new font system) and
background images cover/contain-scaled like CSS `background-size`.

## How it works

- One compositor-owned integer `uiScale` + a virtual coordinate space.
  Clay lays out in virtual units; the compositor renders the command
  stream into a virtual-size scratch surface (all existing scissor /
  sprite-offset / alpha-LUT dispatch unchanged) then nearest-magnifies
  it into the native surface, skipping transparent index 0 so the UI
  composes over what's beneath.
- Menus: virtual = native / uiScale → screens reflow responsively
  (roots changed `CLAY_SIZING_FIXED(dst.w/h)` → `GROW`; redundant
  absolute-coordinate registrations removed — Clay self-resolves hit
  bounds; `DispatchImage` now cover/contain-fits its element box;
  `PackImageContain` added for discrete graphics like the logo).
- In-game: world, minimap/system insets, and HUD/overlays render into the
  single legacy 640×480 screenbuffer; the backend final-present pass stretches
  that frame to the window, matching `origin/main` and avoiding fullscreen CPU
  framebuffer work.
- Pointer input maps into the active virtual space (verified: a
  `click` at 2560×1440 routes via Clay-resolved bounds and transitions
  state).

## Verified (screenshots in `resolution/`)

| Screen | 2560×1440 result |
|---|---|
| Main menu | Fills screen; bg covers; logo uncropped; buttons crisp; click works |
| Options / Audio / Display | Full-screen, centered, scaled — fixed |
| Options→Controls | Panel/scrollbar/buttons responsive+scaled (the garbled keybind labels are the **separate pre-existing string-arena bug** from the 2026-05-14 audit — orthogonal, not this fix) |
| Update dialog | Centered, legible — fixed |
| In-game (Tutorial) | World fills screen through backend upscale; HUD overlays remain on the same 640×480 gameplay frame; world not erased |

640×480 (menus and in-game) is byte-faithful to the legacy look
(`uiScale==1` takes the original direct-render path). Fullscreen gameplay uses
the same final-present stretch as `origin/main`, including the configured
nearest/bilinear scale filter.

## Not done (deliberate, out of scope)

- **Per-edge HUD reflow** (plan Phase 5, explicitly optional): the
  in-game HUD scales as a unit from 640×480 space; it is legible and
  bottom-anchored, nothing visibly broken, so the optional reflow was
  not done (combat overengineering).
- **LOBBY / genuine LOBBYCONNECT / MISSIONSUMMARY / modals**: received
  the identical proven transformation and build clean, but are not
  headless-reachable in this build (same limitation this audit
  documented) so were not screenshot-verified.
- **Text-input click-to-focus at uiScale > 1**: the `TextInput`
  primitive does not self-register Clay bounds (unlike `BankButton`),
  so its absolute-geometry registration was preserved; mouse
  click-to-focus on a text field at native res may be imprecise
  (keyboard focus unaffected). Pre-existing architectural limitation,
  narrowed not introduced; a clean follow-up is to make `TextInput`
  self-register like `BankButton`.
