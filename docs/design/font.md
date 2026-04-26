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
| `alpha`      | If true, glyphs blit through the alpha-blend LUT instead of opaque blit. Button labels pass `alpha = true`; the version overlay passes `alpha = false`. The LUT is the `alphaed` table from `PALETTECALC{N}.BIN` (256×256 — input dst index along one axis, src index along the other; the table value is the resulting blended palette index). Building the LUT on the fly: for each `(src, dst)` pair, mix the RGB triples 50/50 (`(src.rgb + dst.rgb) / 2`) and find the nearest palette index in the active sub-palette by squared-Euclidean distance. **For the main menu specifically, alpha and opaque produce visually equivalent results** because the button-pill interiors are mostly transparent (palette idx 0) — there's no underlying pixel to blend with — and the label glyphs land on transparent. A first-pass hydration can implement opaque only and revisit when a screen actually needs alpha (e.g. chat overlay over the HUD). |
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

`blit` here means the **full sprite blit pipeline** from
[sprite-banks.md](sprite-banks.md), including the anchor-offset
shift `top_left = (x + xc - g.offset_x, y - g.offset_y)`. In
practice every glyph in banks 132..136 has `offset_x == offset_y
== 0` (verified by inspection of `bin_spr/SPR_133.BIN` etc.), so
the offsets are no-ops on the menu and a hydration that ignores
them produces identical output. Don't *rely* on the offsets being
zero in code — call the same blit path the rest of the renderer
uses. Just don't be surprised that simplified glyph blitters that
treat `(x + xc, y)` as the top-left also work.

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
