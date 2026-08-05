# clients/launcher — Silencer launcher

A standalone desktop launcher built on the **game client's own cppx
retained UI engine + SDL3**. (It won the issue-#334 A/B against a
Tauri prototype, since deleted.) Screens: topbar (WEBSITE/DISCORD links +
lobby sign-in popover), a NEWS | RELEASES | SETTINGS content panel, and
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
- **Settings** — three read-only locations, nothing editable and nothing
  else: where the launcher itself is installed (`SDL_GetBasePath`, trimmed
  to the `.app` on macOS), the launcher's own `launcher.json`, and the
  game's per-user data dir (`config.cfg`, `keybinds/`, `level/`, `replays/`,
  palette caches). The one path the user *can* change — where games install
  — stays in the channel drop-up, next to the control that acts on it.

## Layout

| File | Owns |
|---|---|
| `src/config.{h,cpp}` | `launcher.json` load/save + defaults (per-channel installs), plus `game_data_dir()` for the SETTINGS tab. |
| `src/net.{h,cpp}` | TCP-connect latency ping, detached process spawn, SHA-256 hex. |
| `src/app.{h,cpp}` | `App`: config + a single background worker (manifest/news/releases fetch, ping, lobby auth, install/uninstall) + the snapshot the UI polls + intents. |
| `src/ui.h` | The view's public surface: `Intents`, `ViewModel`, `LauncherContext`, `make_launcher_screen()`. |
| `src/ui/` | The views. See below. |
| `src/main.cpp` | SDL3 window + event loop (incl. text-input gating for the Input fields, the async folder-picker marshal for BROWSE..., and the shot-mode input script); drives `PipelineHost` and blits its RGBA. |

### The views (`src/ui/`)

**Every view is authored as JSX in a `.cppx`, with its props in the matching
`.hx`** — the same authoring pipeline `clients/silencer/src/client/ui/` uses,
through the same `tools/cppx_transpile.py`. A `.cppx` returns a tag tree;
anything the grammar can't express (a `std::vector` of rows, a conditional
child) is hoisted into a local above the `return` and interpolated as `{expr}`.
The transpiler only recognises JSX in `return` position or as a tag child, so
never assign a tag to a variable — call the component function directly
(`Label(LabelProps{...})`), which is also how you get an element without a
component fiber.

| File | Owns |
|---|---|
| `tokens.h` | The palette, the glyph faces, the wrap widths, the style presets. **The only file that may hold a raw `::ui::Color`** — a view never spells out paint. |
| `state.h` | `UiState` (tab, selections, which popover is open) + the `SILENCER_LAUNCHER_UI_STATE` seeding. Owned by `RootView`'s fiber via `use_state`, passed down as an `st` prop. |
| `theme.cpp` | `::ui::Theme`, the `LauncherContext` providers, and the `use_snapshot()` / `use_intents()` hooks every view reads instead of prop-drilling the snapshot. |
| `app_button.hx/.cppx` | **`AppButton` — the launcher's one button.** Every pressable surface is this component with a different `variant` (Chrome, Chip, Primary, Tab, Segment, ListRow, ChannelRow, Scrim); the variant carries both the paint and the box, `tone` picks the label colour. A new kind of button is a new variant, never another hand-rolled `ui::components::Button`. |
| `primitives.hx/.cppx` | The rest of the shared pieces: `Label`, `Dot`, `Divider`, `Spacer`, `Bullet`, `CenteredNote`, `ListRow`, `ListColumn`, `DetailPane`. |
| `root.cppx` | `RootView` + the `UiScreen`: topbar, content, playbar, then the overlays in paint order. |
| `top_bar.cppx`, `content_panel.cppx`, `play_bar.cppx` | The three bands. |
| `news_tab.cppx`, `releases_tab.cppx`, `settings_tab.cppx`, `font_qa.cppx` | The content panel's bodies. |
| `auth_popover.cppx`, `channel_dropup.cppx`, `confirm_overlay.cppx` | The root-anchored overlays. |

**A JSX `layout={...}` replaces the whole `LayoutStyle`** — it does not merge
over `ButtonProps`/`InputProps` defaults the way field-by-field assignment did.
Anything a control relies on has to be spelled out, including `ButtonProps`'
132pt default width (`kControlW`), which is what keeps a row of chrome buttons
one size whatever their labels say. Dropping it silently shrink-wraps the
control to its text.

**`Button` renders `label` only when it has no children.** Passing children —
even two `::ui::empty()` slots — silently blanks the label, which is why the
account chip branches into two tags instead of one with conditional children.

`CMakeLists.txt` compiles the reused `clients/silencer/` sources by absolute
path (no changes to that tree), the lobby SDK
(`clients/lobby-sdk/cpp/src/*`), and transpiles both
`box`/`text`/`button`/`input`/`scroll_view.cppx` and every launcher view in
`LAUNCHER_CPPX_VIEWS` with the client's own `tools/cppx_transpile.py`. Add a
view: write the `.hx` + `.cppx`, then add its stem to that list. Run
`python3 ../silencer/tools/cppx_format.py --check src/ui/*.cppx src/ui/*.hx`
before committing — the formatter canonicalises opening tags, and it is the
same one the client's `cppx_format_check` test runs.

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

The build output is `build/Silencer Launcher.app` (macOS — a real bundle, so
the resources travel with the binary and codesigning has something to sign) /
`build\silencer-launcher.exe` (Windows).

**The macOS bundle name has a space in it.** `OUTPUT_NAME` is
`Silencer Launcher`, so the binary is `Contents/MacOS/Silencer Launcher` and
the Finder name matches `CFBundleName`. Every path in `package-macos.sh`, the
CI action and `release.yml` is quoted for that reason, and the CI signing step
uses `find -print0 | xargs -0`. An unquoted path splits at the space and fails
on a *component*, which reads as a puzzling "no such file". Windows and Linux
keep the hyphenated `silencer-launcher` — the space buys nothing without a
bundle.

The bundle's `Info.plist` comes from `SilencerLauncher-Info.plist`, and its
icon is `shared/icons/icon.icns` — the game's icon, shared on purpose: one
product, one mark in the Dock. The version is **not a literal anywhere**:
`-DSILENCER_LAUNCHER_VERSION` fills `CFBundleShortVersionString` and
`CFBundleVersion`, `release.yml` passes `${GITHUB_REF_NAME#v}`, and a local
build gets `00000`.

Two plist keys are Xcode-generator-only and come out **empty** under Ninja —
`${EXECUTABLE_NAME}` and `${MACOSX_DEPLOYMENT_TARGET}`, which is why this
template uses `${MACOSX_BUNDLE_EXECUTABLE_NAME}` and
`${CMAKE_OSX_DEPLOYMENT_TARGET}` instead. (`clients/silencer/Silencer-Info.plist`
still has the Xcode spellings and therefore ships an empty `CFBundleExecutable`;
it only launches because Launch Services falls back to the bundle name, which
happens to match. Don't copy that file's spellings.) Check with
`plutil -p build/Silencer Launcher.app/Contents/Info.plist` — a wrong variable
name is silently an empty string, never an error.

The build **stages its own resources**: the five `shared/fonts/*.otf` faces and
the 12 sprite banks `backgrounds.cpp` actually decodes (0–4, 7, 208 for
backdrops and the logo; 132–136 for the glyph faces) are copied POST_BUILD into
`Contents/Resources/{fonts,assets}` on macOS, or beside the `.exe` elsewhere.
That is 6M of the 24M asset tree. Add a bank to `backgrounds.cpp` and you must
add it to `LAUNCHER_SPR_BANKS` in `CMakeLists.txt`, or an installed copy loses
that backdrop while your dev build keeps working.

Yoga is reused offline from `clients/silencer/build*/_deps/yoga-src` when
present (else shallow-cloned). `Python3` is required (cppx transpile).

## Package (macOS)

A fresh build links Homebrew SDL3/SDL3_ttf by absolute path, so it only runs
where those exist. Make the bundle self-contained:

```bash
clients/launcher/package-macos.sh   # defaults to build/Silencer Launcher.app
```

It copies every non-system dylib into `Contents/Frameworks` (SDL3, SDL3_ttf and
their transitive deps — freetype, harfbuzz, glib, graphite2, intl, pcre2,
png16), rewrites the linkage to `@rpath`, adds the
`@executable_path/../Frameworks` rpath, strips the duplicate literal `@rpath/`
entry that newer dyld hard-aborts on, re-signs, and **fails the script** if any
`/opt/homebrew` path survives. It mirrors
`.github/actions/build-silencer-macos/action.yml`, which does the same for the
game — keep the two in step.

## CI

`ci-build-launcher-{macos,windows,linux}.yml` build the launcher on every
relevant PR, through the **same composite actions** `release.yml` uses, then
run `tests/e2e/check-bundle-*` over the result. Their path allowlist includes
the `clients/silencer/` subtrees this `CMakeLists.txt` compiles by absolute
path — a change there can break the launcher while every game check stays
green, and nothing else catches it. Not required checks yet.

Those three `check-bundle-*` scripts are the game's, given a defaulted
exe-name argument. (The launcher no longer ships a stage-2 helper — its
self-update belongs to the bootstrap stub, see below.)

## Distribution

Three release jobs in `release.yml`, one per platform, each on a `v*` tag
alongside its game counterpart. All three build into `build-launcher/`, never
`build/`, so a launcher job and a game job never collide.

**Every shipped artifact is stub-first** (issue #347): what the user opens is
the bootstrap stub (`clients/launcher-stub/`), and this cppx launcher is the
seed payload nested inside — `payload/` beside the stub exe on
Windows/Linux, `Contents/Resources/payload/Silencer Launcher.app` inside the
stub's bundle on macOS. Each job also publishes a
`silencer-launcher-payload-*` archive (a stub version dir's contents) that
`update-launcher.json`'s `<plat>_payload_url` keys point at.

| Job | Composite action | Artifacts |
|---|---|---|
| `build-launcher-macos` | `build-launcher-macos` | `silencer-launcher-macos-arm64.dmg`, `silencer-launcher-macos-arm64.zip` (full bundle), `silencer-launcher-payload-macos-arm64.zip` |
| `build-launcher-windows` | `build-launcher-windows` | `silencer-launcher-windows-x64.zip`, `silencer-launcher-windows-x64-setup-*.exe`, `silencer-launcher-payload-windows-x64.zip` |
| `build-launcher-linux` | `build-launcher-linux` | `silencer-launcher-linux-x64.tar.gz`, `silencer-launcher-payload-linux-x64.tar.gz` |

Only macOS is signed. Windows and Linux ship unsigned, same as the game's, and
the release notes tell users what SmartScreen will say.

**The staging layout is the runtime contract, not packaging taste.**
`resolve_resource_dir()` probes `fonts/` and `assets/` *beside the payload
binary* for the sentinels `silencer-135.otf` and `PALETTE.BIN`. Flatten those
dirs and the launcher exits before a window opens (missing fonts are fatal —
see Gotchas). Both composite actions assert the sentinels before packaging,
so this fails in CI rather than in a user's hands.

### macOS

`release.yml`'s **`build-launcher-macos`** job is the whole pipeline:

`.github/actions/build-launcher-macos` (configure + build +
`package-macos.sh`, then build the stub's own bundle and nest this app
inside it as the seed payload) → the stub-based self-update e2e gate →
import the Developer ID cert → codesign inside-out (payload dylibs →
payload app → stub) with `--options runtime --timestamp` → `notarytool
submit --wait` → `stapler staple` + `validate` → `spctl -a -vv -t execute` →
`create-dmg` with an `/Applications` symlink → sign, notarize and staple
**the DMG itself** → upload `silencer-launcher-macos-arm64.dmg`.

The CI action calls the same `package-macos.sh` a developer runs, so the two
cannot drift. It reuses `release.yml`'s existing Apple secrets
(`APPLE_CERT_P12_BASE64`, `APPLE_CERT_P12_PASSWORD`, `APPLE_ID`,
`APPLE_APP_PASSWORD`, `APPLE_TEAM_ID`) — no new secret. It builds into
`build-launcher/`, not `build/`, so it never collides with the game job.

#### arm64 only, deliberately

The launcher is **not** a universal binary, and making it one would be worse
than not. `CMAKE_OSX_ARCHITECTURES=arm64;x86_64` is technically reachable in
CI — libcurl comes from `/usr/lib` (already universal), Yoga is a FetchContent
source build that follows the setting, and `setup-sdl` builds SDL3/SDL3_ttf
from source so it could build both slices. (Homebrew is arm64-only, so none of
it is buildable or testable on a dev Mac.)

The blocker is the payload, not the build: **the game ships arm64-only**
(`silencer-macos-arm64.dmg` is the only macOS artifact `release.yml`
produces). Rosetta translates x86_64 → arm64, not the reverse, so an Intel Mac
cannot run the game at all. A universal launcher would install fine there,
then fail at PLAY — a worse experience than not offering the download. Ship
universal only when the game does.

### Windows

`.github/actions/build-launcher-windows` configures against vcpkg (its **own**
binary cache, keyed on `clients/launcher/vcpkg.json` — the launcher's
dependency set is not the game's, and sharing a cache thrashes it), builds
this launcher and the stub, then stages the stub as the root
`silencer-launcher.exe` with the launcher exe + vcpkg DLLs + `fonts/` +
`assets/` under `payload\`. The release job zips that, then runs ISCC over
[`installer/silencer-launcher.iss`](installer/CLAUDE.md).

`resources.rc` gives the exe its icon and a **VERSIONINFO** block, so
`-DSILENCER_LAUNCHER_VERSION` is not a macOS-only no-op. VERSIONINFO needs a
numeric quad, and our tags are a zero-padded counter, so CMake maps `NNNNN` →
`0,0,N,0` (the same mapping `clients/tui` uses for npm semver) and writes it
into a generated `launcher_version.h` that `resources.rc` includes. Leading
zeros must go — `0042` is not a number to the RC compiler. `project()` names
CXX explicitly, so `enable_language(RC)` is called for `WIN32` rather than
relying on the platform module to imply it.

### Linux

`.github/actions/build-launcher-linux` builds this launcher and the stub,
stages the stub at the tarball root, and under `payload/` copies the launcher
plus `libSDL3.so.0` and `libSDL3_ttf.so.0` **dereferenced** (`cp -L` — the
binary's NEEDED entry is the soname, so the loader wants a regular file at
that path), `patchelf --set-rpath '$ORIGIN'` so they resolve from the payload
binary's own dir, then `fonts/` and `assets/`. It fails the build if `ldd`
reports anything "not found" (and if the stub's own `ldd` shows anything
beyond system libs). The release job tars it. There is no `.desktop` file or
icon install — the tarball is a run-from-anywhere bundle, not a distro
package.

### Self-update

**This process never updates itself.** The launcher's own updates belong
to the bootstrap stub (`clients/launcher-stub/`, issue #347): the user
launches the stub, the stub checks `manifest_url_launcher` (the
`<plat>_payload_url` keys), applies updates into a versioned store, and
runs this launcher as the *payload*. Any failure launches the existing
version; a payload that dies right after an update is rolled back. This
binary keeps exactly one self-update duty: printing
`[launcher] build <id>` on stderr at startup, which the stub's release
gate asserts from the payload it relaunched.

(The previous in-app mechanism — `SelfUpdateChip`, `App::self_update`,
the nested stage-2 helper — was removed with #347. The decision record
for the old design is `docs/plans/2026-08-04-launcher-self-update.md`,
kept as history.)

## Run

```bash
"clients/launcher/build/Silencer Launcher.app/Contents/MacOS/Silencer Launcher"
```

Fonts and assets resolve through `resolve_resource_dir()` (main.cpp), in order:
the env override below, then **beside the binary** via `SDL_GetBasePath()`
(the bundle's `Contents/Resources` on macOS, the `.exe` dir elsewhere) probed
with a sentinel file, then the compile-time source-tree path. An installed copy
hits the second; a bare `build/` run hits the third.

Env overrides (all optional):
- `SILENCER_LAUNCHER_CONFIG=<path>` — use a different config file (default is
  the per-OS path below). Handy for testing without clobbering it.
- `SILENCER_LAUNCHER_FONT_DIR=<dir>` — UI `.otf` faces. Wins outright, even if
  the dir is wrong, so a test can point at an empty dir and see the failure.
- `SILENCER_LAUNCHER_ASSETS_DIR=<dir>` — game sprite assets for the backdrop
  cycle. `src/backgrounds.cpp` decodes a
  curated set of screen-sized sprites (the bank 0–4 parallax backdrops with
  their junk bottom rows cropped, plus bank 007/208 menu screens) and shows
  one picked at random for the whole session; missing assets just mean a
  plain fill.
- `SILENCER_LAUNCHER_SHOT=<png>` — **offscreen self-verify**: render until the
  async states settle (both manifests, news, releases, ping, auth, any
  in-flight install), write the frame to `<png>`, quit. This is the real
  render pipeline, so the PNG is exactly what the window shows.
- `SILENCER_LAUNCHER_UI_STATE=<tokens>` — seed the UI state for shots:
  comma-separated `releases`, `settings`, `drop`, `auth`, `confirm`, `fontqa`
  (font QA panel in place of the content panel).
- `SILENCER_LAUNCHER_SCRIPT=<steps>` — shot-mode input script: semicolon-
  separated `tab` / `backtab` / `click:x,y` / `dblclick:x,y` (logical 900×600
  coords) / `text:<utf8>` / `key:<name>[+shift|ctrl|alt|super]` (names: left,
  right, home, end, backspace, delete, enter, a, c, v, x) steps, replayed one
  step per few frames once the async states settle; the shot then waits for
  the last activation to render. Reaches what UI_STATE can't (focus rings,
  pointer flows, drop-up row clicks, the #336 text-editing model — e.g.
  `click:700,114;text:hello world;key:left+shift+alt` shows a word selection).
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
  "clients/launcher/build/Silencer Launcher.app/Contents/MacOS/Silencer Launcher"
```

## Config

`launcher.json` in the per-OS app-data dir, mirroring the client's
`GetDataDir()` rather than hardcoding one convention everywhere:

| | Config dir (`launcher.json`) | Default `base_dir` (games) |
|---|---|---|
| macOS | `~/Library/Application Support/Silencer Launcher/` | `~/Applications/Silencer/` |
| Windows | `%APPDATA%\Silencer Launcher\` | `%LOCALAPPDATA%\Programs\Silencer\` |
| Linux | `~/.config/silencer-launcher/` | `~/.local/share/silencer-launcher/` |

Games install to the per-user application dir, not inside the launcher's own
config dir — on macOS `Silencer.app` then lands in `~/Applications`, where a
user expects to find it. (Both paths were `~/.config` + `~/.local/share` on
every OS while issue #334 needed one fixed contract the cppx and Tauri
prototypes could share; Tauri is gone, so the contract went with it.)

Keys: `channel`, `base_dir`,
`installed_stable`, `installed_nightly`, `manifest_url_stable`,
`manifest_url_nightly`, `manifest_url_launcher` (the launcher's **own**
build, for self-update — not a game channel, so it follows stable releases
only), `announcements_url` (the shared/news v2 feed —
production default `https://arsiamons.com/announcements.json`),
`releases_url` (GitHub Releases API), `lobby_host`, `lobby_port`. Each

All four `*_url` keys can point at one host: `services/admin-api` serves
`/api/launcher/manifest/{stable,nightly}`, `/api/launcher/news`, and
`/api/launcher/releases`, which is also the easiest local setup — run the API
and point the four keys at `http://localhost:24080/api/launcher/...` (loopback
`http://` is what the download allowlist permits). The shipped defaults still
go straight to GitHub / `arsiamons.com`; flip them once the endpoints are
deployed.

channel installs into `{base_dir}/{channel}`; the game binary path is
derived per-OS, never stored. Reads are type-guarded so one mismatched
field falls back to its default instead of aborting the parse.

## Gotchas

- **`PipelineHost::ensure()` creates the `UiPipeline` lazily** — call it once
  before touching `host.pipeline()`, or you dereference a null `unique_ptr`.
  `main.cpp` does an initial `ensure()` before `set_frame_provider`/`push_screen`.
- **Missing fonts are fatal, not a soft fallback.** `PipelineHost::ensure()`
  fails outright if the font dir has no faces, and `main()` exits before a
  window ever opens — verified by pointing `FONT_DIR` at an empty dir. Missing
  *assets* degrade gracefully (plain fill, TTF text); missing *fonts* do not.
  This is why the resource staging above exists: before it, the baked path was
  an absolute path into the build machine's checkout, so any copied launcher
  was dead on arrival.
- **Re-sign AFTER the last `install_name_tool` edit, never before.** Every
  rpath edit invalidates a Mach-O signature, and Apple Silicon SIGKILLs a
  binary whose signature does not match its contents. The app dies with
  **exit 137 and no message at all** — no dyld error, no crash log, nothing to
  search for. `codesign -v` is the only thing that names the problem
  ("invalid signature (code or signature have been modified)").
  `package-macos.sh` therefore signs the dylibs and the bundle as its final
  step; dylibbundler's own mid-run ad-hoc signing does not survive the rpath
  dedupe that follows it.
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
  duplication). `ui/tokens.h` authors everything at native; keep it that way.
  `UI_STATE=fontqa` shows the face × size ramp to eyeball this.
- **Text wraps only at a definite *points* width** (Yoga measures the text node
  against it); `percent` does not trigger wrap. The window is therefore a fixed
  900×600 and the news body uses a points width.
- **No `clients/silencer/` edits.** Reused sources are referenced by path from
  `CMakeLists.txt`. The reused UI closure is the game-dep-free set
  (`pipeline_host_tests`' sources minus `app_root.cpp`) plus `navigation_provider`.
- **A curl transfer is only cancellable from its progress callback.** `~App`
  sets `quit_` and then blocks in `worker_.join()`, so any fetch without an
  abort hook holds the process open past the window — up to `CONNECTTIMEOUT`
  (15s) per URL, unbounded on a stalled read, and the four metadata URLs run in
  sequence. `shutdown_` (atomic, because the curl thread doesn't hold `mtx_`)
  is set before the join and read by both progress callbacks. Any new
  `downloader_.Fetch` call must pass one of them, never `nullptr`.
- **Displayed paths go through `display_path` (ui/settings_tab.cppx)**: backslashes to
  forward slashes, trailing separator dropped. Not cosmetic — the atlas draws
  `\` as a dash, so an un-normalized `C:\Users\me` reads as `C:-Users-me`, a
  wrong path a user could copy down. Windows accepts forward slashes, so the
  displayed path stays a real one. The channel drop-up's base_dir row does
  **not** normalize yet and has the same bug on Windows.
- **The game's data dir is mirrored, not reused.** `game_data_dir()`
  (config.cpp) hand-copies the per-OS paths from `GetDataDir()` in
  `clients/silencer/src/platform/os.cpp` — macOS `~/Library/Application
  Support/Silencer/`, Windows `%APPDATA%/Silencer/`, else
  `$HOME/.config/silencer/`. Compiling os.cpp in is not an option: it
  includes the game's `shared.h`, far outside the launcher's dep closure,
  and `GetDataDir()` *creates* the dir as a side effect — opening a settings
  page must not conjure a data dir for someone who has never run the game.
  Change one, change the other.
- **`SDL_GetBasePath()` inside a macOS bundle returns
  `<app>/Contents/Resources`, not `Contents/MacOS`** — verified by running a
  staged bundle, not assumed. `launcher_install_dir()` (main.cpp) trims
  either tail to report the `.app` itself. Test path changes from a real
  bundle: copy the binary into `Foo.app/Contents/MacOS/` and shot from there.
- **`detail_pane` needs a row wrapper.** It takes `height: 100%`, which only
  resolves inside the flex row NEWS/RELEASES put it in. Dropped straight into
  the content panel's column it overflows and pushes the playbar off the
  window — `settings_content` wraps it in a `cols` box for that reason alone.
- **Updater reuse**: `updaterdownload` + `updatersha256` + `updaterzip` for
  installing the game (plain extract-and-replace — the game isn't running).
  **Not** `updater.cpp` (the game's state machine), and **not**
  `updaterstage2` — the launcher's OWN updates are the bootstrap stub's job
  (see Self-update). On macOS `updaterzip` shells out to `ditto`, so no
  minizip dependency.

## Verify

Configure + build, then run with `SILENCER_LAUNCHER_SHOT` and inspect the
PNGs. **Test packaging by copying `build/Silencer Launcher.app` somewhere
outside the repo and running it from there** — the resource staging is
invisible from a `build/` run, which happily falls back to the source tree.
Confirm the `backgrounds: N loaded from ...` line names the bundle's own
`Contents/Resources/assets`, not `shared/assets`. After `package-macos.sh`,
prove the dylibs too — `DYLD_PRINT_LIBRARIES=1` must show SDL3 loading from the
copy's own `Contents/Frameworks`, since `otool -L` alone only shows intent:

```bash
DYLD_PRINT_LIBRARIES=1 "/path/to/copy/Silencer Launcher.app/Contents/MacOS/Silencer Launcher" 2>&1 | grep SDL3
``` Cover: the fresh state (news + INSTALL), `UI_STATE=releases,drop`
(tabs + channel drop-up), `UI_STATE=settings` (paths panel — check the
playbar is still on screen), `UI_STATE=auth` (sign-in form),
`UI_STATE=confirm` (uninstall dialog), `TEST_INSTALL` against a loopback
manifest+zip (ends in PLAY enabled), and `TEST_SIGNIN` against a local
lobby (signed-in chip). Do not claim done on compile success alone.

Gotcha while styling: the glyph atlases cover ASCII 33..126 only — keep
UI strings ASCII (fold feed text via `fold_glyphs`) — and within that range
several punctuation cells hold the **wrong glyph**. Verified by shooting
`A\B/C-D_E|F~G^H`, which renders `A-B/C-D-EAFÃG-H`: `\` and `_` and `^` all
draw a dash, `|` draws an A, `~` draws an Ã. Only `/` is right. Nothing warns
you — the text just renders as a different, plausible string, so check any new
punctuation in a shot before trusting it. This is why `display_path` exists.
Also never give
a full-window element a fully transparent background fill (the solid-fill
fast path erases the frame beneath; use a real translucent wash).
