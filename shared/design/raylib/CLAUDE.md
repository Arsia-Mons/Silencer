# Silencer Design System — Raylib hydration

A self-contained Raylib (C99) demo program that renders the Silencer UI
design system directly from the binary assets in `shared/assets/`. Used to
verify the spec in `docs/design-system.md` is faithful and complete.

## Build

Requires raylib 5.x (Homebrew: `brew install raylib`).

```sh
cmake -B build
cmake --build build
```

## Run

```sh
./build/silencer_design [path/to/shared/assets]
```

The assets path defaults to `../../assets` (relative to the executable).

## Controls

- Left / Right arrow (or PageUp/PageDown): cycle demo screens
- Home: jump to the first screen
- Tab: cycle focus inside the current screen (TextInput screen)
- Mouse: hover/click widgets
- Esc: quit

## Layout

- `src/main.c` — entry point, demo navigator, RenderTexture2D pixel-perfect scaling
- `src/palette.{h,c}` — PALETTE.BIN loader (`v << 2` channel expansion); EffectColor/EffectBrightness
- `src/sprite.{h,c}` — BIN_SPR.DAT + SPR_NNN.BIN loader and RLE codec (linear + 64x64 tile-ordered)
- `src/font.{h,c}` — sprite-bank text rendering (banks 132-136), tiny text, drop shadow
- `src/widgets/*.{h,c}` — Button, Toggle, TextInput, TextBox, SelectBox, ScrollBar,
  Overlay, Interface, horizontal-stretch panel, modal dialog, loading bar, HUD bars, minimap
- `src/screens/*.{h,c}` — one demo screen per UI category, plus 4 representative compositions
  (Main Menu, Lobby, In-Game HUD, Buy Menu)

## Design fidelity notes

- All coordinates are in 640x480 logical pixels. Mouse coords are converted from
  the window-space cursor before reaching widget hit-tests.
- Widget animation state is driven by a fixed 23.8 Hz tick (42 ms accumulator)
  separate from the render frame rate.
- Sprite tinting (`EffectColor`/`EffectBrightness`) is done CPU-side per-pixel
  for non-trivial transforms (this is the documented engine behavior). Default
  brightness 128 / no tint goes through the cached RGBA texture for speed.

## Gotchas

- `BIN_SPR.DAT` is consulted only at offset `+2` per bank (sprite count). All
  other bytes per bank header are ignored.
- Some sprite RLE streams emit fewer pixels than `width * height`; the rest
  are zero-filled (palette index 0 = transparent).
- The minimap is a placeholder; the original is a 172x62 paletted buffer
  generated from level data, which is out of scope for the design demo.
