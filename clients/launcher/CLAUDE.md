# clients/launcher — Silencer launcher

A standalone desktop launcher built on the **game client's own cppx
retained UI engine + SDL3**. (It won the issue-#334 A/B against a
Tauri prototype, since deleted.) Screens: topbar (WEBSITE/DISCORD links +
lobby sign-in popover), a NEWS | RELEASES list/detail content panel, and
a playbar (lobby ping, channel drop-up with per-channel installs to
`{base_dir}/{channel}`, split INSTALL/PLAY button). Its whole point is
reuse: the hard parts (HTTPS download + progress + SHA-256 verify, zip
extract, the retained UI runtime + flex layout + focus/hover, the lobby
wire protocol) already live in `clients/silencer/` and
`clients/lobby-sdk/cpp/` and are **compiled in directly** — this dir only
adds the launcher screens, config, fetch plumbing, TCP ping, and process
spawn.

Data sources:

- **News** — the version-2 block-AST feed compiled by `shared/news/`
  (rendered natively by a block walker; unknown block types skip).
  Feed text is glyph-folded to the pixel fonts' byte coverage
  (`fold_glyphs` in `app.cpp`).
- **Releases** — the GitHub Releases API: stable = non-prereleases,
  nightly = the `latest` prerelease; "What's Changed" bullets are
  cleaned (markdown links stripped, `by @user in url` trailers cut).
- **Sign-in** — real TCP `opAuth` via the lobby SDK (auto-registers new
  usernames; the version handshake is skipped). Session handoff to the
  game and persisted auth are future phases — the game still prompts.

## Layout

| File | Owns |
|---|---|
| `src/config.{h,cpp}` | `launcher.json` load/save + defaults (per-channel installs). |
| `src/net.{h,cpp}` | TCP-connect latency ping, detached process spawn, SHA-256 hex. |
| `src/app.{h,cpp}` | `App`: config + a single background worker (manifest/news/releases fetch, ping, lobby auth, install/uninstall) + the snapshot the UI polls + intents. |
| `src/ui.{h,cpp}` | Phosphor-green `::ui::Theme`, the `client::ui::UiScreen`, and the view components (composed from the reused `ui::components` primitives; per-session UI state lives in `RootView`'s fiber via `use_state`). |
| `src/main.cpp` | SDL3 window + event loop (incl. text-input gating for the Input fields, the async folder-picker marshal for BROWSE..., and the shot-mode input script); drives `PipelineHost` and blits its RGBA. |

`CMakeLists.txt` compiles the reused `clients/silencer/` sources by absolute
path (no changes to that tree), the lobby SDK
(`clients/lobby-sdk/cpp/src/*`), and transpiles
`box`/`text`/`button`/`input`/`scroll_view.cppx` with the client's own
`tools/cppx_transpile.py`.

## Build

macOS: Homebrew SDL3/SDL3_ttf + system libcurl (same as the client); no vcpkg.

```bash
cmake -S clients/launcher -B clients/launcher/build \
      -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build clients/launcher/build -j8
```

Windows: MSVC + vcpkg (this dir has its own `vcpkg.json`: sdl3, sdl3-ttf,
curl, minizip). From a vcvars64 shell (resolve VS/VCPKG_ROOT the same way
`clients/silencer/build.ps1` does):

```
cmake -S clients\launcher -B clients\launcher\build -G Ninja ^
      -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
      -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build clients\launcher\build -j8
```

The daily config is **RelWithDebInfo** (optimized + symbols), not Debug: the
retained UI rebuilds its tree every frame through many tiny abstractions, and
MSVC `/Od` compiles them literally — measured 4–9× on the frame loop, enough
to drop a vsync'd 60fps to 30. `build-release/` (plain Release) exists for
perf measurement.

The binary is `build/SilencerLauncher` (macOS) / `build\silencer-launcher.exe`
(Windows).

Yoga is reused offline from `clients/silencer/build*/_deps/yoga-src` when
present (else shallow-cloned). `Python3` is required (cppx transpile).

## Run

```bash
clients/launcher/build/SilencerLauncher
```

Env overrides (all optional):
- `SILENCER_LAUNCHER_CONFIG=<path>` — use a different config file (default is
  the shared contract path below). Handy for testing without clobbering it.
- `SILENCER_LAUNCHER_FONT_DIR=<dir>` — UI `.otf` faces (default: baked-in
  `shared/fonts`).
- `SILENCER_LAUNCHER_ASSETS_DIR=<dir>` — game sprite assets for the backdrop
  cycle (default: baked-in `shared/assets`). `src/backgrounds.cpp` decodes a
  curated set of screen-sized sprites (the bank 0–4 parallax backdrops with
  their junk bottom rows cropped, plus bank 007/208 menu screens) and shows
  one picked at random for the whole session; missing assets just mean a
  plain fill.
- `SILENCER_LAUNCHER_SHOT=<png>` — **offscreen self-verify**: render until the
  async states settle (both manifests, news, releases, ping, auth, any
  in-flight install), write the frame to `<png>`, quit. This is the real
  render pipeline, so the PNG is exactly what the window shows.
- `SILENCER_LAUNCHER_UI_STATE=<tokens>` — seed the UI state for shots:
  comma-separated `releases`, `drop`, `auth`, `confirm`, `fontqa` (font QA
  panel in place of the content panel).
- `SILENCER_LAUNCHER_SCRIPT=<steps>` — shot-mode input script: semicolon-
  separated `tab` / `backtab` / `click:x,y` (logical 900×600 coords) steps, replayed one
  step per few frames once the async states settle; the shot then waits for
  the last activation to render. Reaches what UI_STATE can't (focus rings,
  pointer flows, drop-up row clicks).
- `SILENCER_LAUNCHER_TEST_SIGNIN=<user:pass>` — shot-mode only: run the real
  TCP opAuth flow on startup (point `lobby_host`/`lobby_port` at a local
  `go run ./services/lobby -addr :15170 ...`).
- `SILENCER_LAUNCHER_TEST_INSTALL=<channel>` — shot-mode only: run the real
  download→verify→extract flow on startup.
- `SILENCER_LAUNCHER_PERF_SCALE=<f>` + `SILENCER_LAUNCHER_PERF_FRAMES=<n>` —
  frame-cost measurement: raster at `f`× the logical size (synthetic HiDPI),
  inject a focus-nav repaint every 30 frames, quit after `n` measured frames,
  print repaint/idle/upload/present stats to stderr. Measured 2026-08-03
  (M-series, Release ≈ Debug), with the #331 IR-diff damage tracking + the
  span/ring solid-fill fast paths in the shared bridge: idle frames dirty-skip
  (~5–6 ms build/layout, resolution-independent); a focus-nav repaint totals
  ~8 ms at EVERY scale (1×/2×/4× ≈ 7.7/7.4/8.6 ms — raster is ~1–2 ms, the
  balance is the same build/layout idle frames pay). Full-surface repaints
  (first frame, resize, structural change, or `SILENCER_UI_DAMAGE=0`) are
  ~38 ms at 4× (were ~294 ms when every solid fill went through SDL's SW
  triangle rasterizer).

Local end-to-end test (dev manifest + news + releases over loopback, which
the download allowlist permits): serve `update.json`,
`announcements.json` (v2 — `bun run compile` in `shared/news`), a
GitHub-shaped `releases.json`, and optionally a `game.zip` whose sha256
the manifest carries, from any static server; point every `*_url` in a
scratch config at it.

```bash
SILENCER_LAUNCHER_CONFIG=/tmp/cfg.json SILENCER_LAUNCHER_SHOT=/tmp/shot.png \
  clients/launcher/build/SilencerLauncher
```

## Config

`~/.config/silencer-launcher/launcher.json`, keys: `channel`, `base_dir`,
`installed_stable`, `installed_nightly`, `manifest_url_stable`,
`manifest_url_nightly`, `announcements_url` (the shared/news v2 feed —
production default `https://arsiamons.com/announcements.json`),
`releases_url` (GitHub Releases API), `lobby_host`, `lobby_port`. Each
channel installs into `{base_dir}/{channel}`; the game binary path is
derived per-OS, never stored. Reads are type-guarded so one mismatched
field falls back to its default instead of aborting the parse.

## Gotchas

- **`PipelineHost::ensure()` creates the `UiPipeline` lazily** — call it once
  before touching `host.pipeline()`, or you dereference a null `unique_ptr`.
  `main.cpp` does an initial `ensure()` before `set_frame_provider`/`push_screen`.
- **Text renders from the origin bitmap glyph atlases, not TTF.**
  `bake_glyph_faces` (backgrounds.cpp) decodes sprite banks 132–136 +
  `PALETTE.BIN` from the assets dir and bakes them into the host's
  `GlyphFonts` — the same faces the game bakes, drawn 1:1 from origin's
  glyph pixels. Re-baked whenever `chrome_needs_bake()` (ensure() drops
  renderer-bound textures). The SDL_ttf `.otf` path still loads but is
  reached only if a face fails to bake (missing assets). Origin cells are
  cap-top-aligned (descender zone at the cell bottom), so the bake pads
  1–2 blank rows above each glyph: flex-centered line boxes then center
  the cap ink, and descenders hang below the box (baseline behavior).
- **Text sizes must be the face's native line height** (or an integer
  multiple) to stay pixel-perfect: Body 11/22, Large 15, Title 23, Tiny 7,
  Heading 19. Anything else nearest-scales the glyph cells (uneven pixel
  duplication). `ui.cpp` authors everything at native; keep it that way.
  `UI_STATE=fontqa` shows the face × size ramp to eyeball this.
- **Text wraps only at a definite *points* width** (Yoga measures the text node
  against it); `percent` does not trigger wrap. The window is therefore a fixed
  900×600 and the news body uses a points width.
- **No `clients/silencer/` edits.** Reused sources are referenced by path from
  `CMakeLists.txt`. The reused UI closure is the game-dep-free set
  (`pipeline_host_tests`' sources minus `app_root.cpp`) plus `navigation_provider`.
- **Updater reuse is partial**: `updaterdownload` + `updatersha256` +
  `updaterzip` only. **Not** `updater.cpp`/`updaterstage2.cpp` — the launcher
  is plain extract-and-replace (the game isn't running). On macOS `updaterzip`
  shells out to `ditto`, so no minizip dependency.

## Verify

Configure + build, then run with `SILENCER_LAUNCHER_SHOT` and inspect the
PNGs. Cover: the fresh state (news + INSTALL), `UI_STATE=releases,drop`
(tabs + channel drop-up), `UI_STATE=auth` (sign-in form),
`UI_STATE=confirm` (uninstall dialog), `TEST_INSTALL` against a loopback
manifest+zip (ends in PLAY enabled), and `TEST_SIGNIN` against a local
lobby (signed-in chip). Do not claim done on compile success alone.

Gotcha while styling: the glyph atlases cover ASCII 33..126 only — keep
UI strings ASCII (fold feed text via `fold_glyphs`), and never give
a full-window element a fully transparent background fill (the solid-fill
fast path erases the frame beneath; use a real translucent wash).
