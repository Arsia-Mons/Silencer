# Font / `DrawText`

**Source:** `clients/silencer/src/renderer.cpp::DrawText` (line 1463),
font sprite banks `132..136` in `shared/assets/bin_spr/`.

## Font banks

Glyphs are ordinary sprites, one per character. The bank indexes
the font; the glyph index inside a bank picks the character.

| Bank | Approx. cap height | Used by main menu? |
| ---- | ------------------ | ------------------ |
| 132  | ~7 px              | No (debug/HUD only) |
| 133  | ~11 px             | **Yes** — version overlay (`textwidth = 11`) |
| 134  | ~15 px             | No (toggle labels) |
| 135  | ~19 px             | **Yes** — `B196x33` button labels (`textwidth = 11`*) |
| 136  | ~23 px             | No (announcement messages) |

> *The `textwidth` argument is the **advance** between glyph
> origins (i.e. character spacing), not the glyph height. The
> menu uses bank 135 with advance `11` for button labels, which is
> tighter than the glyph cap height — glyph art overlaps slightly
> by design.

## Character → glyph index

The mapping is a flat ASCII offset:

```
glyph_index = (unsigned)char_value - 33     # for banks 133, 134, 135, 136
glyph_index = (unsigned)char_value - 34     # for bank 132 only
```

Space (`0x20`) and non-breaking space (`0xA0`) are not glyph indices
— they advance the cursor by `textwidth` without blitting.

## `DrawText` signature

```
DrawText(surface, x, y, text, bank, advance,
         alpha = false, color = 0, brightness = 128, rampcolor = false)
```

| Parameter    | Effect (main-menu use only) |
| ------------ | --------------------------- |
| `x, y`       | Top-left of the first glyph in **screen** space (no camera offset) |
| `text`       | Null-terminated ASCII |
| `bank`       | One of 132..136 |
| `advance`    | Pixels to advance per character (regardless of glyph width) |
| `alpha`      | If true, glyphs blit through the alpha-blend LUT instead of opaque blit. Button labels pass `alpha = true`; the version overlay passes `alpha = false` |
| `color`      | Palette index for `EffectColor` tint. `0` = no tint. The menu doesn't tint text |
| `brightness` | `EffectBrightness` parameter, `128` = neutral. Button labels use the button's current `effectbrightness` (128..136 during hover ramp); other menu text passes 128 |
| `rampcolor`  | Switches the tint pipeline to `EffectRampColor`. Always false on the menu |

## Per-glyph render loop

```
xc = 0
for ch in text:
    if ch == ' ' or ch == '\xA0':
        xc += advance
        continue
    glyph_sprite = sprite_banks[bank][ch - ioffset]   # ioffset = 33 (or 34 for bank 132)
    g = glyph_sprite
    if color != 0 or brightness != 128:
        g = copy_of(glyph_sprite)
        if color: EffectColor(g, color)               # never on main menu
        if brightness != 128: EffectBrightness(g, brightness)
    blit(g) onto surface at (x + xc, y), alpha=alpha
    xc += advance
```

The blit honors the sprite's transparency (index 0 → skip), so
glyph anti-aliasing is just per-pixel palette indices the artist
authored into the glyph sprite.

## Centering helpers (used by `Button`)

`Button::GetTextOffset` (`clients/silencer/src/button.cpp:178`)
computes the centered label position:

```
xoff = (width - strlen(text) * advance) / 2
yoff = 8                       # for B196x33
textX = button.x - sprite.offset_x + xoff
textY = button.y - sprite.offset_y + yoff
```

So the label is laid out relative to the **rendered sprite top-left**
(after applying the anchor offset from
[sprite-banks.md](sprite-banks.md)), not the object's logical
position.
