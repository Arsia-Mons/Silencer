# clients/tui — Bun + TS terminal client for Silencer

`silencer-tui` spawns a headless `silencer --tui` engine and renders its
paletted framebuffer directly into a terminal. The whole multiplayer game
stack — lobby, menus, gameplay, audio — runs inside the same C++ binary as
the SDL client; the TS process is a pure view + input layer.

## How it fits

```
silencer-tui (TS)              silencer --tui (C++)
  ├─ TCP listen :A    ←─────  TUIBackend writes binary frames
  ├─ TCP listen :B    ←──→    ControlServer (JSON-line) — input + state
  ├─ raw stdin        →─────  control "input" op stamps world.localinput
  └─ stdout           ←─────  half-block / kitty rasterizer paints terminal
```

- **Engine side** (`clients/silencer/src/render/tuibackend.{h,cpp}`):
  `RenderDevice` impl that connects to `SILENCER_TUI_FRAME_HOST/_PORT` and
  ships `[u8 type][u32 len][payload]` messages.
  Type 0x01 = palette (256×RGBA), type 0x02 = frame (u16 w, u16 h, w·h indexed).
  Engine flag `--tui` (game.cpp) skips video init, keeps audio, swaps backends.
- **Input is split across multiple paths** because the engine has separate
  consumers:
  - **Gameplay** (held keys: move/aim/fire/jump) goes through the canonical
    `Input` struct via the binary input channel, last-write-wins per tick.
    Terminal stdin → autorelease timer (no keyup events from cooked
    terminals) → snapshot sent at 24 Hz.
  - **Menu navigation + text input** uses a separate edge-triggered `key`
    op on the JSON control socket that calls
    `Interface::ProcessKeyPress(world, ascii)` on the current interface.
    Arrow keys map to magic chars 1-4, plus `\t`/`\n`/`\b`/0x1B and
    printable ASCII pass through. Mirrors the SDL client's
    `HandleSDLEvents` SDL_EVENT_KEY_DOWN translation.
  - **Mouse** uses SGR mouse tracking (`CSI ?1000;1002;1006 h`) on stdin.
    `input.ts` parses `\x1b[<btn;col;row M|m`; `index.ts` converts cell
    coords to engine pixels using the latest frame size, then ships them
    via the binary input channel's `MSG_MOUSE` (type 0x03) — see
    `inputserver.h` for the wire format. Sent independently of scancodes
    so motion doesn't trample held keys.

## Run during dev

```bash
cmake --build clients/silencer/build       # build the engine first
bun ./clients/tui/src/index.ts              # interactive — needs a TTY

# Headless smoke + raster verification (no TTY required):
bun ./clients/tui/tests/smoketest.ts        # writes /tmp/silencer-tui-frame.ppm
bun ./clients/tui/tests/raster_test.ts      # rasterizes the PPM at 160×48
bun ./clients/tui/tests/probe.ts            # state probe over control socket
```

## Layout

- `src/index.ts` — entry: spawn binary, drive frame + control + input loops.
- `src/frame_parser.ts` — TUIBackend wire format → `{palette,frame}` events.
- `src/raster_halfblock.ts` — `▀` + truecolor renderer, diff-redraw cells.
- `src/term.ts` — alt screen / raw mode / cursor lifecycle (idempotent restore).
- `src/control_client.ts` — JSON-line TCP client with id-multiplexed replies.
- `src/input.ts` — keymap + autorelease timer → `InputState`.

## Gotchas

- **Tick rate is 24 Hz** in the engine (`game.cpp:361`, `wait = 42`). TUI
  mode adds `SDL_Delay(33)` per Loop because TUIBackend's TCP write doesn't
  block (no vsync gate). Without that, the loop ran at ~1500 fps.
- **Mouse cell→pixel mapping is approximate.** Half-block packs 2 engine
  rows per cell vertically and 1:1 horizontally, then nearest-neighbor
  scales to fit the terminal. Inverting gives center-of-cell sampling — a
  click on cell `(cx,cy)` lands at `floor((cx+0.5)*fw/cols)`,
  `floor((cy+0.5)*fh/rows)`. Good enough for menu hit-tests; in-game
  aiming will feel chunky vs. native SDL.
- **Binary discovery** (`findBinary` in `index.ts`) tries, in order:
  `SILENCER_BIN` env var, the matching
  `@arsia-mons/silencer-<process.platform>-<process.arch>` package
  (production install), then the dev build paths
  (`clients/silencer/build/{Silencer.app/...,silencer,Silencer.exe}`).

## npm distribution

Published via the `publish-npm` job in `.github/workflows/release.yml`
on every `v*` tag — five packages, all sharing the tag's version:

| Package | Contents |
|---|---|
| `@arsia-mons/silencer` | top-level: bundled `dist/index.js` from `bun build`; `optionalDependencies` pin all three platform packages at the same version |
| `@arsia-mons/silencer-darwin-arm64` | notarized + stapled `Silencer.app` |
| `@arsia-mons/silencer-linux-x64` | `silencer` binary, bundled `libSDL3*.so.0` (RUNPATH=`$ORIGIN`), `assets/` |
| `@arsia-mons/silencer-win32-x64` | `Silencer.exe`, vcpkg DLLs, `assets/` |
| `silencer-tui` (unscoped) | one-line redirect — `import '@arsia-mons/silencer'` and a matching `dependencies` entry |

`scripts/stage-npm-packages.ts` assembles the five package directories
from the release artifacts; the workflow runs it then `npm publish
--access public --provenance` on each. Versions in the source
`package.json` (`0.1.0`) are placeholders — the staging script
rewrites them to `${GITHUB_REF_NAME#v}` in lockstep before publish.

## What's not yet here

- **Kitty graphics rasterizer** — design intent was both half-block and
  kitty graphics protocol in v1; only half-block is implemented today.
  C++ side stays unchanged when the kitty backend lands.
- **Lobby + gameplay flows** are unverified end-to-end. Engine handles them
  through the same `screenbuffer` → `UploadFrame` path, so they should
  work, but contract testing against a real lobby is open work.
