# shared/design/sdl3

SDL3 (C++17) hydration of `docs/design/`, currently scoped to the
**main menu**. This is the reference rendering used to verify the
per-component spec docs are faithful and complete; visual A/B
against `clients/silencer`'s framebuffer dump is the validation
gate.

## Sources

- [`docs/design/`](../../../docs/design/) is the only behavioral spec.
- `shared/assets/PALETTE.BIN`, `BIN_SPR.DAT`, `bin_spr/SPR_NNN.BIN`
  are the authoritative pixel content. The loader in
  `src/sprite.cpp` implements the RLE codec from
  [docs/design/sprite-banks.md](../../../docs/design/sprite-banks.md).
- This program intentionally does **not** read anything from
  `clients/silencer/` — it must stand on the spec alone.

## Build

```
cmake -B build
cmake --build build
```

Requires SDL3 (`brew install sdl3` on macOS, `apt install libsdl3-dev`
on recent Debian/Ubuntu, or build from source).

## Run

```
./build/silencer_design [path/to/shared/assets]
```

Default asset path is `../../../assets/` relative to the executable,
which resolves to the in-repo assets when running from `build/`.

## QA dump (recommended for spec validation)

```
SILENCER_DUMP_DIR=/tmp/d ./build/silencer_design ../../../assets
sips -s format png /tmp/d/screen_00.ppm --out /tmp/d/screen_00.png
```

Bypasses macOS Screen Recording / Spaces friction. The PPM is the
indexed framebuffer resolved through the active sub-palette
(palette `1` for the main menu); compare against the real client's
output:

```
SILENCER_DUMP_PATH=/tmp/real.ppm \
  /path/to/Silencer.app/Contents/MacOS/Silencer
```

Both dumps wait for the bank-208 logo to reach its steady-state
frame (`res_index = 60`), so timing differences don't matter.

## Layout

```
src/
  main.cpp                  entry point + dump-mode harness
  palette.{h,cpp}           PALETTE.BIN loader; brightness helpers
  sprite.{h,cpp}            BIN_SPR.DAT + SPR_NNN.BIN loader, RLE codec, blit
  font.{h,cpp}              text drawing using sprite banks 132–136
  widgets/
    widget.h                shared base
    button.{h,cpp}          Button (B196x33 only)
    interface.{h,cpp}       focus manager / keyboard+mouse dispatch
    overlay.{h,cpp}         sprite or text Overlay (incl. bank-208 logo)
    primitives.{h,cpp}      Clear / FilledRect / Line / Circle / Checkered
  screens/
    screen.h                Screen interface + factories
    screen_main_menu.cpp    composition (per docs/design/screen-main-menu.md)
```

## Faithfulness notes

- Logical surface is **640 × 480 8-bit indexed**, converted to RGBA
  through the active sub-palette and presented through
  `SDL_LOGICAL_PRESENTATION_LETTERBOX`.
- Active sub-palette is **palette 1** (main menu) for the entire
  session — see [docs/design/palette.md](../../../docs/design/palette.md).
- `EffectBrightness` is computed inline by re-matching to the active
  palette via nearest-neighbour. Other effects (`EffectColor`,
  `EffectRampColor`, `EffectTeamColor`, `EffectAlpha`,
  `EffectShieldDamage`, `EffectHacking`, `EffectWarp`, `EffectHit`)
  are not implemented — none of them are needed for the menu.
- Button alpha-blend on label glyphs is currently flat-blit; the
  real client passes `alpha = true` to `DrawText` for buttons. Visual
  difference on the menu is negligible because the pill interiors are
  mostly transparent. Add an alpha LUT to `font.cpp` when this matters.
