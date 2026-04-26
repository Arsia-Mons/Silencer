# shared/design/sdl3 — main-menu hydration

C++17 + SDL3 reference renderer for the Silencer main menu, built only
from `docs/design/` + binary assets in `shared/assets/`. Primary QA
target for the design-system spec — produces a 640x480 indexed
framebuffer dump (PPM) compared visually against the real client.

## Build

```
cd shared/design/sdl3
cmake -B build
cmake --build build
```

## Run (dump mode)

```
SILENCER_DUMP_DIR=/tmp/sdl3_dump \
  ./build/silencer_design /Users/hv/repos/Silencer/shared/assets
```

Writes `${SILENCER_DUMP_DIR}/screen_00.ppm` (binary P6, 640x480).

## What's faithful

- `PALETTE.BIN` decoder: per-sub offset `4 + s * 772`, 6-bit -> 8-bit,
  zero-filled past EOF (matches engine over-read).
- Sprite RLE codec: linear (mode 0) and tile (any non-zero mode, 64x64
  tiles, 4-pixel chunks).
- BIN_SPR.DAT 16,384-byte index, count at byte offset 2 of each
  64-byte record.
- Anchor convention: `top_left = object - sprite.offset`.
- Main-menu sub-palette = 1.
- Logo (bank 208) ticked to the hold frame (`res_index = 60`).
- Buttons in INACTIVE state (no hover) — `res_index = 7`,
  `effectbrightness = 128`.
- Version text at `(10, 463)` via bank 133 advance 11.

## What's faked / skipped

- No mouse/keyboard input. We render a single static frame.
- Brightness LUT is computed on the fly only when text actually requests
  `brightness != 128`; on the menu's INACTIVE buttons, brightness is 128
  so the LUT is never built in this path.
- No alpha-blend LUT. The spec notes that on the main menu, alpha vs.
  opaque produce visually equivalent results because button-pill
  interiors are transparent (palette idx 0).
- No window/event loop in dump mode — straight to PPM and exit.
