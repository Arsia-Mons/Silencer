# shared/fonts/tools — font extractor

Decodes the bitmap font glyphs from `shared/assets/bin_spr/SPR_*.BIN`
and emits the four `.otf` files in the parent `shared/fonts/`
directory. Run after the source sprite banks change.

## Run

```
uv sync          # one-time: install fontTools into ./venv
uv run extract.py
```

## What it produces

Four OpenType fonts, one per bitmap font bank:

| Bank | Output                              |
| ---- | ----------------------------------- |
| 132  | `shared/fonts/silencer-tiny.otf`     |
| 133  | `shared/fonts/silencer-ui.otf`       |
| 134  | `shared/fonts/silencer-ui-large.otf` |
| 136  | `shared/fonts/silencer-title.otf`    |

Each font has both:
- A `glyf` table with monochrome 1×1 pixel outlines (universal fallback).
- An `SVG ` table with per-pixel `fill-opacity` preserving the AA
  ramp; uses `currentColor` so CSS `color:` re-tints the way the
  game's `EffectRampColor` does.

## How it works

`extract.py` mirrors `Resources::LoadSprites` in
`clients/silencer/src/resources/resources.cpp`:

1. Reads `BIN_SPR.DAT` for the per-bank sprite count.
2. For each font bank, reads `SPR_NNN.BIN` and decompresses the RLE
   stream (handles both linear and 64×64-tile modes).
3. Crops trailing empty rows/cols per glyph for clean metrics.
4. Maps each glyph's palette indices to per-pixel opacity using
   the brightness-within-ramp formula `((idx - 2) % 16) / max_used`
   (see `docs/design/palette.md` for ramp structure).
5. Builds the font with fontTools, mapping sprite index N to ASCII
   codepoint `(ioffset + N)` to match `Renderer::DrawText`.

See `shared/fonts/CLAUDE.md` for the consumer-side documentation.
