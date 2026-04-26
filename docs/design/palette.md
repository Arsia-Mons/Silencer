# Palette

**Source:** `clients/silencer/src/palette.cpp`, `clients/silencer/src/palette.h`,
`shared/assets/PALETTE.BIN`.

## File layout

`shared/assets/PALETTE.BIN` is **8,448 bytes** on disk.

Authoritative seek formula — the **first byte of sub-palette `s`'s
color table** is at file offset:

```
color_offset(s) = 4 + s * (768 + 4)
```

That offset already points directly at the first R byte. **Do not
add another `+ 4` on top of this.** The `+ 4` term in the formula
is the file-level prefix; the `* 4` term is the trailing filler that
sits *after* each sub-palette's color table, not before. Iterating
the loader looks like:

```
seek to color_offset(s)
read 256 × (R, G, B) bytes
# the next 4 bytes (positions [color_offset(s) + 768 .. + 771])
# are filler; the next sub-palette's color table starts at
# color_offset(s+1) = color_offset(s) + 772
```

Concrete byte addresses for sub 0..2 (raw 6-bit values from the file,
before the `<< 2` expansion):

| s | color_offset | idx 0 (R G B) | idx 1 (R G B) | idx 2 (R G B) |
| - | ------------ | ------------- | ------------- | ------------- |
| 0 | 4            | `00 00 00`    | `00 00 00`    | `00 00 00`    |
| 1 | 776          | `00 00 00`    | `02 27 00`    | `10 02 04`    |
| 2 | 1548         | `00 00 00`    | `00 00 00`    | `02 02 02`    |

A hydration reading 4 bytes too deep will produce a recognizable-but-
wrong main menu (the black bg becomes blue, the teal buttons become
coral/red — the colors shift by one slot). Use the table above as a
sanity check before rendering anything.

The arithmetic doesn't fit: `4 + 11 × 772 = 8,496`, and the file is
8,448 bytes. The real client over-reads off the end of the file for
the last sub-palettes; the `SDL_ReadIO` calls return shorter and the
unread bytes stay zero. Match this behavior in a port — don't
"correct" the layout to 11 × 768 contiguous, even though that
arithmetic fits cleanly; the differing offsets mean the two schemes
produce different palette indices for the same sprite art.

Channels are 6-bit (0..63 in the file). On load, each byte is
shifted left by 2 (`v << 2`) to expand to 8-bit. Index 0 is
treated as transparent at the blit layer (see
[sprite-banks.md](sprite-banks.md)).

### Reference loader pseudocode

```
buf = read entire PALETTE.BIN
for s in 0..10:
    co = 4 + s * 772
    for i in 0..255:
        # bounds-check: writes past EOF (~last sub-palette) stay zero
        r = (co + i*3 + 0 < |buf|) ? buf[co + i*3 + 0] : 0
        g = (co + i*3 + 1 < |buf|) ? buf[co + i*3 + 1] : 0
        b = (co + i*3 + 2 < |buf|) ? buf[co + i*3 + 2] : 0
        palettes[s][i] = (r << 2, g << 2, b << 2)
```

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
computes the brightness LUT on the fly as follows:

```
for each src palette index i in 2..255:
    rgb = palette[active][i]
    if brightness > 128:
        t   = (brightness - 127) / 128.0          # 0..1
        rgb = lerp(rgb, white, t)                 # toward (255,255,255)
    elif brightness < 128:
        t   = brightness / 128.0                  # 0..1
        rgb = scale(rgb, t)                       # toward (0,0,0)
    # else: identity
    lut[i] = nearest palette index j in 2..255 of palette[active]
             by squared-Euclidean RGB distance
             (∑ (rgb[c] - palette[active][j][c])²)
```

Indices `0` and `1` skip the LUT (they're protected — transparent
and black). For `brightness == 128` the LUT is the identity map and
should fast-path; the inner loop costs ~256² distance computes per
non-identity LUT and is acceptable to recompute every frame on the
menu (called once per active button hover-ramp tick).

There is no luminance weighting; the engine uses plain Euclidean
RGB distance. The pre-computed `brightness` table inside
`PALETTECALC{N}.BIN` is the same mapping serialized.

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
