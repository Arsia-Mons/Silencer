# screen-options-display — Display options

Two labeled toggle rows (Fullscreen, Smooth Scaling) plus Save / Cancel.
Same shape as `screen-options-audio` but with two rows instead of one.

Reference dump: `/tmp/real_optionsdisplay_dump.ppm` (640×480 P6,
sub-palette 1, captured via `SILENCER_DUMP_STATE=OPTIONSDISPLAY`).

## Sub-palette

`1`.

## Object inventory

In z-order:

| z | Object | Type | Bank | Index | x | y | Notes |
| - | --- | --- | --- | --- | --- | --- | --- |
| 0 | Background | overlay | 6 | 0 | 0 | 0 | Same fullscreen plate as options hub / audio |
| 1 | Title text | overlay (font 135, w=12) | — | — | `320 - len("Display Options")*12/2` | 14 | text=`Display Options` |
| 2 | Fullscreen button | B220x33 | — | — | 100 | 50 | text=`Fullscreen`, uid=0 |
| 3 | Fullscreen Off pill | overlay | 6 | 12 | 420 | 137 | uid=20 |
| 4 | Fullscreen On pill  | overlay | 6 | 15 | 450 | 137 | uid=40 |
| 5 | Smooth Scaling button | B220x33 | — | — | 100 | 103 | text=`Smooth Scaling`, uid=1 |
| 6 | Smooth Scaling Off pill | overlay | 6 | 12 | 420 | 190 | uid=21 |
| 7 | Smooth Scaling On pill  | overlay | 6 | 15 | 450 | 190 | uid=41 |
| 8 | Save button   | B196x33 | — | — | -200 | 117 | text=`Save`, uid=200 |
| 9 | Cancel button | B196x33 | — | — | 20 | 117 | text=`Cancel`, uid=201 |

Toggle row stride is `+53` per row (matches `screen-options-controls`'s
6-row stride). Y values for row `i ∈ {0,1}`: button `y = 50 + i*53`,
indicators `y = 137 + i*53`.

## Layout note: indicator-y is overlay-relative

All sprite widgets use the same canonical formula
`top_left = anchor - sprite.offset` per
[`widget-overlay.md`](widget-overlay.md). Screen-centring is baked
into each sprite's negative `offset_x`/`offset_y`. The numeric `y`
on the indicators (137, 190) appears much larger than the buttons'
(50, 103) for the same visual row because the **indicator sprites
have small / near-zero offsets** (they are positioned near the
top-left of the framebuffer in spec-coords) while the button sprites
have large negative offsets (~ −300) that translate the button
column into the screen interior. Different sprite-offset magnitudes
— same anchor formula.

If a candidate forgets to apply the sprite offset for the indicator
overlays, they render in the same logical position as their numeric
`(x, y)` but using the *button's* offsets — wrong half of the
screen.

## Activation / state

Static, 60-tick settle pin.

The On indicator on each row reflects the persisted user preference
on the canonical dump (typically Fullscreen=on, Smooth Scaling=on).
The candidate should render both indicators per the spec — render
both sprites; the relatively-brighter one IS the "on" state by
sprite-bank assignment, not by toggle state.

Buttons render in INACTIVE state at dump time.

## Spec gaps to flag

- Same `B220x33` widget gap as `screen-options-audio` — add to
  [`widget-button.md`](widget-button.md).
- The overlay-vs-button anchor convention difference (indicators use
  literal coords, buttons use center-relative) is not separately
  documented. Worth a one-line in
  [`widget-overlay.md`](widget-overlay.md) explicitly distinguishing
  Overlay (literal `(x,y)`) from Button (center-relative `(x,y)`).

## Cross-references

- [`screen-options-audio.md`](screen-options-audio.md) — sibling, single-row variant
- [`screen-options-controls.md`](screen-options-controls.md) — sibling, scrollable 6-row form
- [`widget-button.md`](widget-button.md), [`widget-overlay.md`](widget-overlay.md)
- [`palette.md`](palette.md), [`sprite-banks.md`](sprite-banks.md), [`tick.md`](tick.md)
