# shared/fonts — Silencer pixel fonts for HTML/CSS

Generated OpenType fonts of the four bitmap fonts used by
`clients/silencer`. Drop `index.css` into a web project to get the
same fonts (and the same colors via CSS `color:`) the game uses.

| File                    | Source bank | Native em | Advance | Used for in-game            |
| ----------------------- | ----------- | --------- | ------- | --------------------------- |
| `silencer-tiny.otf`     | 132         | 5px       | 4px     | HUD digits (`DrawTinyText`) |
| `silencer-ui.otf`       | 133         | 11px      | 7px     | Main UI text                |
| `silencer-ui-large.otf` | 134         | 13px      | 9px     | Toggle labels / headers     |
| `silencer-title.otf`    | 136         | 24px      | 16px    | Announcement titles         |

## Source of truth

These `.otf` files are **generated artifacts** — never hand-edit
them. Source data lives in:

- `shared/assets/BIN_SPR.DAT` (sprite bank counts)
- `shared/assets/bin_spr/SPR_{132,133,134,136}.BIN` (glyph bitmaps)
- `shared/assets/PALETTE.BIN` (palette ramp definitions)

To regenerate after the source bitmaps change:

```
cd shared/fonts/tools
uv sync          # first time only
uv run extract.py
```

## How the colors work

Each glyph is built with SVG-in-OpenType. Every pixel renders as a
1×1 square using `fill="currentColor"` with `fill-opacity` derived
from the pixel's brightness within its palette ramp (see
`docs/design/palette.md` for the ramp layout). This mirrors how
`Renderer::EffectRampColor` re-tints text in-game: the brightness
ramp is preserved while the hue is substituted.

Set `color:` to whatever you want — the AA shading follows.

```css
.in-game-default { color: #187C14; }   /* native ramp 13 brightness 10 */
.alert           { color: #FF3333; }
.muted           { color: #888888; }
```

## Pixel-perfect rendering

Use `font-size` at an integer multiple of the native em (5/11/13/24).
Non-integer sizes get sub-pixel scaling, which softens the edges.

```css
.hud         { font-family: "Silencer Tiny";     font-size: 5px;  }
.body        { font-family: "Silencer UI";       font-size: 11px; }  /* or 22, 33 */
.section     { font-family: "Silencer UI Large"; font-size: 13px; }
.banner      { font-family: "Silencer Title";    font-size: 24px; }
```

## Character coverage

- Bank 132 covers ASCII codepoints 34..122 (`"` through `z`).
- Banks 133/134/136 cover ASCII 33..186 (`!` through `º`); codepoints
  127..159 land on Unicode C1 control range and aren't normally
  typed but render if referenced numerically (`&#128;`, etc.).
- `U+0020` (space) and `U+00A0` (NBSP) are zero-glyph and advance
  by the bank's advance width — matching `Renderer::DrawText`.

## Browser support

- SVG-in-OpenType: Firefox, Chrome/Edge 65+, Safari 16.4+.
- Older renderers fall back to the monochrome `glyf` outlines (one
  square per non-zero pixel, no AA shading).
