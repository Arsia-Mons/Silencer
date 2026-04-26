# shared/design/sdl3

C++17 + SDL3 hydration of the Silencer main menu, built only from
[`docs/design/`](../../../docs/design/) plus the binary assets in
[`shared/assets/`](../../assets/). Reference renderer for the
design-system spec — produces a 640×480 indexed framebuffer dump
(PPM) that should match the real client's `Game::Present` output.

This tree was authored by a spec-only subagent that did not have
access to `clients/silencer/`, so its existence is the falsifiability
gate for `docs/design/`. If the spec changes such that a fresh
spec-only build no longer produces a matching dump, the spec
regressed.

## Build

```
cd shared/design/sdl3
cmake -B build
cmake --build build
```

Requires SDL3 (`brew install sdl3` on macOS).

## Capture

```
SILENCER_DUMP_DIR=/tmp/sdl3_dump \
  ./build/silencer_design /Users/hv/repos/Silencer/shared/assets
```

Writes `${SILENCER_DUMP_DIR}/screen_00.ppm` (binary P6, 640×480).
Compare against the real client's dump via the env-gated
`SILENCER_DUMP_PATH` path in `Game::Present`. See
[`.claude/skills/visual-regression-testing/SKILL.md`](../../../.claude/skills/visual-regression-testing/SKILL.md)
for the full A/B workflow.

## What's faithful

- `PALETTE.BIN` decoder using the disambiguated `4 + s × 772` color-
  table seek formula from `docs/design/palette.md`. First three RGB
  triples of sub-palettes 0/1/2 verified against the table in that
  doc before rendering anything.
- Sprite RLE + tile codec (mode 0 linear, any non-zero tile-mode).
- `BIN_SPR.DAT` 16,384-byte index, count at byte offset 2 of each
  64-byte record.
- Anchor convention: `top_left = object - sprite.offset`.
- Main-menu sub-palette = 1.
- Logo (bank 208) ticked to the hold frame (`res_index = 60`).
- Buttons in INACTIVE state (no hover) — `res_index = 7`,
  `effectbrightness = 128`.
- Version overlay at `(10, 463)` via bank 133 advance 11.

## Faked / skipped

- No interactive mode. The binary writes one PPM and exits when
  `SILENCER_DUMP_DIR` is set. Without that env var the binary still
  exits cleanly but renders nothing visible.
- Brightness LUT is computed only when `effectbrightness != 128`.
  The menu's INACTIVE buttons hold at 128, so the LUT is not built
  in the captured frame.
- No alpha-blend LUT — opaque blit only. Per `docs/design/font.md`,
  alpha and opaque produce visually equivalent output on the menu
  because button-pill interiors are transparent.
- No window/event loop in dump mode.

## Layout

```
src/
  main.cpp        single-file palette/sprite/font/widget/screen impl
CMakeLists.txt    SDL3 detection + build target
```

Splitting into per-component files (palette.{h,cpp}, sprite.{h,cpp},
…) is fine when adding more screens; the current single-file form is
deliberate scope discipline for a one-screen subset.
