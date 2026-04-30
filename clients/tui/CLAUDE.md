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
- **Input is split across two ops** because the engine has two input paths:
  - **Gameplay** (held keys: move/aim/fire/jump) goes through the canonical
    `Input` struct via the `input` control op, last-write-wins per tick.
    Terminal stdin → autorelease timer (no keyup events from cooked
    terminals) → snapshot sent at 24 Hz.
  - **Menu navigation + text input** uses a separate edge-triggered `key`
    op that calls `Interface::ProcessKeyPress(world, ascii)` on the current
    interface. Arrow keys map to magic chars 1-4, plus `\t`/`\n`/`\b`/0x1B
    and printable ASCII pass through. Mirrors the SDL client's
    `HandleSDLEvents` SDL_EVENT_KEY_DOWN translation.

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
- **Mouse coords default to 0xFFFF** (sentinel for "no mouse position",
  per `input.cpp:Serialize`). `TerminalInput.snapshot()` preserves this so
  the wire-level mouse path stays inactive in TUI mode.
- **Binary discovery** (`findBinary` in `index.ts`) checks
  `clients/silencer/build/Silencer.app/Contents/MacOS/Silencer` (macOS),
  then `build/silencer` (Linux), then `build/Silencer.exe` (Windows).
  Override with `SILENCER_BIN=<path>`.

## What's not yet here

- **Kitty graphics rasterizer** — design intent was both half-block and
  kitty graphics protocol in v1; only half-block is implemented today.
  C++ side stays unchanged when the kitty backend lands.
- **Per-platform npm packaging** — distribution shape (esbuild's
  `optionalDependencies` of `@silencer/tui-{darwin,linux,win32}-{arm64,x64}`)
  is designed but not built. `silencer-tui` is `private: true` until that
  lands.
- **Lobby + gameplay flows** are unverified end-to-end. Engine handles them
  through the same `screenbuffer` → `UploadFrame` path, so they should
  work, but contract testing against a real lobby is open work.
