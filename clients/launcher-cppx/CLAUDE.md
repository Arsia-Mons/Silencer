# clients/launcher-cppx — cppx Silencer launcher (prototype)

A standalone desktop launcher (channel toggle, self-update, news feed,
server browser with latency, Play) built on the **game client's own cppx
retained UI engine + SDL3**. It exists for an A/B test against the sibling
`clients/launcher-tauri/`. Its whole point is reuse: the hard parts
(HTTPS download + progress + SHA-256 verify, zip extract, the retained UI
runtime + flex layout + focus/hover) already live in `clients/silencer/`
and are **compiled in directly** — this dir only adds the launcher screens,
config, fetch plumbing, TCP ping, and process spawn.

## Layout

| File | Owns |
|---|---|
| `src/config.{h,cpp}` | `launcher.json` load/save + defaults. |
| `src/net.{h,cpp}` | TCP-connect latency ping, detached process spawn, SHA-256 hex. |
| `src/app.{h,cpp}` | `App`: config + a single background worker (manifest/news fetch, pings, update download→verify→extract) + the snapshot the UI polls + intents. |
| `src/ui.{h,cpp}` | Phosphor-green `::ui::Theme`, the `client::ui::UiScreen`, and the view components (composed from the reused `ui::components` primitives). |
| `src/main.cpp` | SDL3 window + event loop; drives `PipelineHost` and blits its RGBA. |

`CMakeLists.txt` compiles the reused `clients/silencer/` sources by absolute
path (no changes to that tree) and transpiles `box`/`text`/`button.cppx`
with the client's own `tools/cppx_transpile.py`.

## Build

macOS: Homebrew SDL3/SDL3_ttf + system libcurl (same as the client); no vcpkg.

```bash
cmake -S clients/launcher-cppx -B clients/launcher-cppx/build \
      -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build clients/launcher-cppx/build -j8
```

Windows: MSVC + vcpkg (this dir has its own `vcpkg.json`: sdl3, sdl3-ttf,
curl, minizip). From a vcvars64 shell (resolve VS/VCPKG_ROOT the same way
`clients/silencer/build.ps1` does):

```
cmake -S clients\launcher-cppx -B clients\launcher-cppx\build -G Ninja ^
      -DCMAKE_BUILD_TYPE=Debug ^
      -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build clients\launcher-cppx\build -j8
```

The binary is `build/SilencerLauncher` (macOS) / `build\silencer-launcher.exe`
(Windows).

Yoga is reused offline from `clients/silencer/build*/_deps/yoga-src` when
present (else shallow-cloned). `Python3` is required (cppx transpile).

## Run

```bash
clients/launcher-cppx/build/SilencerLauncher
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
  manifest/news/ping states settle, write the frame to `<png>`, quit. This is
  the real render pipeline, so the PNG is exactly what the window shows.
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

Local end-to-end test (dev manifest + news over loopback, which the
download allowlist permits):

```bash
cd /tmp && python3 -m http.server 8000 &   # serve update.json + announcements.json
SILENCER_LAUNCHER_CONFIG=/tmp/cfg.json SILENCER_LAUNCHER_SHOT=/tmp/shot.png \
  clients/launcher-cppx/build/SilencerLauncher
```

## Config (shared contract with launcher-tauri — do not deviate)

`~/.config/silencer-launcher/launcher.json`, keys: `channel`,
`installed_version`, `install_dir`, `game_binary`, `servers`, `last_server`,
`manifest_url_stable`, `manifest_url_nightly`, `announcements_url`. Reads are
type-guarded so a field the other launcher wrote with a different type falls
back to its default instead of aborting the parse.

## Gotchas

- **`PipelineHost::ensure()` creates the `UiPipeline` lazily** — call it once
  before touching `host.pipeline()`, or you dereference a null `unique_ptr`.
  `main.cpp` does an initial `ensure()` before `set_frame_provider`/`push_screen`.
- **TTF pixel-font sizes.** The UI `.otf` faces are rendered by SDL_ttf
  (the launcher does not bake the origin bitmap glyph atlases the game uses).
  They are generated pixel fonts (1×1-em squares), so `FontRegistry` opens
  them with hinting disabled — FreeType's hinter otherwise collapses/doubles
  glyph rows at non-native sizes (horizontal banding, seen on Windows).
  Unhinted, any size renders uniformly; integer multiples of the native em
  (Body 11/22, Large 13/26, Title 24) are pixel-perfect, others slightly soft.
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

Configure + build, then run with `SILENCER_LAUNCHER_SHOT` and inspect the PNG
(exercise both the graceful "manifest unavailable"/"no announcements" states
and — via a loopback dev server — the "update available" + news + enabled-Play
happy path). Do not claim done on compile success alone.
