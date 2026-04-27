# Palette

**Source:** `clients/silencer/src/palette.cpp`, `clients/silencer/src/palette.h`,
`shared/assets/PALETTE.BIN`.

## File layout

`shared/assets/PALETTE.BIN` is **8,448 bytes** on disk. The real client
(`clients/silencer/src/palette.cpp:43..54`) reads each sub-palette `s`
starting at file offset:

```
offset(s) = 4 + s * (768 + 4)
```

i.e. a 4-byte file prefix, then 11 records of `[4-byte sub-header,
768-byte color table]`. The 4-byte prefix and per-sub headers are
treated as filler (all zeros). The color table is 256 entries × 3
bytes `(R, G, B)`.

The arithmetic doesn't actually fit: `4 + 11 × 772 = 8,496`, and the
file is 8,448 bytes. The real client over-reads off the end of the
file for the last few sub-palettes; the `SDL_ReadIO` calls just
return shorter and the unread bytes stay zero. Match this behavior
in a port — don't compress the layout to 11 × 768 contiguous, even
though *that* arithmetic would fit cleanly. The differing offsets
mean both schemes produce different palette indices for the same
sprite art.

Channels are 6-bit (0..63 in the file). On load, each byte is
shifted left by 2 (`v << 2`) to expand to 8-bit. Index 0 is
treated as transparent at the blit layer (see
[sprite-banks.md](sprite-banks.md)).

## The active sub-palette is state-driven (this is the part the old spec missed)

The renderer always blits from the *currently active* sub-palette.
`Game` switches sub-palettes when entering each top-level state via
`Palette::SetPalette(N)`:

| Game state | Active sub-palette | `game.cpp` site |
| ---------- | ------------------ | --------------- |
| MAINMENU | **1** | `game.cpp:494` |
| LOBBYCONNECT, LOBBY, OPTIONS* | 2 | `game.cpp:519, 543, 583, 726` |
| INGAME, FADEOUT (default), MISSIONSUMMARY | 0 | `game.cpp:814, 1020, 1729, 1846` |
| MAINMENU (return path from misc) | 1 | `game.cpp:977` |

**For the main menu, sub-palette 1 is active.** Every
component-color claim in this spec is implicitly *"under the
main-menu sub-palette."* The same palette index decodes to a
different RGB triple under sub-palettes 0 or 2, which is why a
hydration that loads only sub-palette 0 produces visually wrong
menus even when every other layer is correct.

## Loading lookup tables

Adjacent to `PALETTE.BIN`, the engine writes per-sub-palette LUT
caches `PALETTECALC{N}.BIN` to the user data dir. Each is 256 × 256
× 3 bytes (brightness, color-tint, alpha) for one sub-palette.
Computed on first launch from `Palette::Calculate(2, 256-30)`.

For the main-menu subset, only the **brightness LUT** is exercised
(button hover ramps `effectbrightness` from 128 → 136). A hydration
can compute the brightness LUT on the fly via nearest-match against
the active sub-palette; see `widget-button.md` and the SDL3
hydration's `widgets/button.cpp` for the inline form.

## Indexed → RGBA

```
for each pixel index i in surface:
    if i == 0: output transparent (the sprite blitter usually skips
               these before they reach the framebuffer, but a final
               surface→RGBA conversion can map them to (0,0,0) or
               leave alpha=0)
    else: rgba = (R<<0) | (G<<8) | (B<<16) | (0xFF<<24)
          using palettes_[active_subpalette][i]
```

## Index ranges relevant to the main menu

Only a handful of palette regions matter for the menu:

- **0**: reserved transparent. Never appears as a visible pixel.
- **1**: black (rendered).
- **2..113**: 7 color groups × 16 brightness levels. Buttons,
  background plate, version text, and logo sprite all draw indices
  in this range under sub-palette 1.
- **114..225**: upper palette mirror — entered transiently when
  `EffectBrightness(>128)` shifts a base color upward.
- **226..255**: parallax sky colors. Not used by the main menu;
  values present in the menu sub-palette (1) but not consumed.

(The full per-group breakdown is in the archived monolithic spec
under `### Color Ramp Groups`. We'll re-add it here when we
hydrate a screen that actually relies on naming the groups; the
main menu never references "team blue" or "shield green" by name.)
