# clients/launcher-tauri — Silencer game launcher (Tauri prototype)

Desktop launcher for the Silencer client: pick a release channel,
apply auto-updates, browse servers by latency, read news, and launch
the game against a chosen lobby. Built with Tauri 2 — a Rust backend
plus a vanilla-TS webview. **A/B prototype**: a competing C++ launcher
lives in `clients/launcher-cppx/`; the two share the on-disk config
contract (below) and are otherwise independent.

## Stack

- **Backend:** Rust (`src-tauri/`). All network I/O, SHA-256 hashing,
  zip extraction, TCP ping, config read/write, and process spawning are
  Tauri commands. The webview never touches the network — this sidesteps
  webview CORS entirely.
- **Frontend:** vanilla TypeScript, no framework, no state library. Bun
  bundles `src/main.ts` → `dist/main.js`; `scripts/build-frontend.ts`
  copies the static shell. Tauri serves `dist/` directly (no dev server).
- **Formatter:** oxfmt. **Runtime:** Bun.

## Layout

```
src/                     frontend (TS + HTML + CSS)
  main.ts                view logic; all backend calls via invoke()
  index.html styles.css  static shell (phosphor-green CRT theme)
scripts/build-frontend.ts  Bun.build bundle step (beforeDev/BuildCommand)
src-tauri/
  src/main.rs            Tauri builder + invoke_handler
  src/commands.rs        the 8 commands + Config/Manifest/Announcement
  tauri.conf.json        window (900x600), CSP, bundle icons
  capabilities/default.json  core:default for the one window
  icons/                 generated set; icon-source.svg is the source
```

## Build / run

Run Bun from the **repo root** (`bun install`), never inside this
package — it shares the root `bun.lock` (this dir is in the root
`package.json` `workspaces`). Everything else runs from here:

```sh
bun run tauri dev            # or: bunx tauri dev — opens the window
bun run tauri build --debug  # debug .app -> src-tauri/target/debug/bundle/macos/
bun run typecheck            # tsc --noEmit over src/
bun run build:frontend       # just the JS bundle -> dist/
```

`tauri dev`/`build` run `build:frontend` first via `beforeDevCommand`
/`beforeBuildCommand`, so you don't bundle by hand.

## Config contract (shared with launcher-cppx)

JSON at `~/.config/silencer-launcher/launcher.json` — **exact path and
keys are a cross-launcher contract; do not deviate.** Created with
defaults on first run, re-read at startup, written on change. Keys:
`channel`, `installed_version`, `install_dir`, `game_binary`,
`servers` (`[{name,host,port}]`), `last_server`, `manifest_url_stable`,
`manifest_url_nightly`, `announcements_url`. Defaults live in
`Config::default()` in `src-tauri/src/commands.rs`.

## Manifest shape

Update manifests mirror `services/lobby/update.go`:
`{version, macos_url, macos_sha256, windows_url, windows_sha256}`.
`apply_update` picks the macOS asset on this platform, streams the
download while hashing, verifies SHA-256, then extracts into
`install_dir`. Hash mismatch → discard + error, no partial install.

## Local testing

Serve the repo-root dev `update.json` and point a channel at it:

```sh
cd /Users/hv/repos/Silencer && python3 -m http.server 8000
# then in launcher.json set e.g.
#   "manifest_url_stable": "http://127.0.0.1:8000/update.json"
```

The default GitHub release manifest URLs return 404 until those assets
exist — that surfaces as the graceful "manifest unavailable for
<channel>" state, which is expected, not a bug.

## Gotchas

- **Command args are camelCase from JS, snake_case in Rust** (Tauri's
  default rename): `invoke("apply_update", { installDir, onProgress })`
  → `apply_update(install_dir, on_progress)`.
- **Icons:** regenerate with `bun run tauri icon src-tauri/icons/app-icon.png`
  after editing `icon-source.svg` (rasterize the SVG → `app-icon.png`
  first with the root `sharp` dep). The `tauri icon` command also emits
  iOS/Android sets — delete them; this is desktop-only.
- **No dev server:** the frontend is static; there's no HMR. Re-run
  `tauri dev` (or `build:frontend`) to pick up frontend edits.
- **Detached launch:** `launch_game` spawns the game and does not wait,
  so it outlives the launcher. Missing/invalid `game_binary` disables
  Play (via `path_exists`) rather than crashing.
