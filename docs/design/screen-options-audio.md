# screen-options-audio — Audio options

A simple two-row form: one labeled toggle widget, plus Save / Cancel.
The simplest of the options sub-screens — bootstraps the title-overlay
and toggle-indicator widgets used by Display and Controls.

Reference dump: `/tmp/real_optionsaudio_dump.ppm` (640×480 P6,
sub-palette 1, captured via `SILENCER_DUMP_STATE=OPTIONSAUDIO`).

## Sub-palette

`1`. Same as every menu screen.

## Object inventory

In z-order:

| z | Object | Type | Bank | Index | Coord origin | x | y | Notes |
| - | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | Background | overlay | 6 | 0 | screen | 0 | 0 | Same fullscreen plate as main menu / options hub |
| 1 | Title text | overlay | bank 135 (font), `textwidth=12` | — | screen | `320 - len("Audio Options") * 12 / 2` | 14 | text=`Audio Options` |
| 2 | Music button | B220x33 | — | — | center+anchor | 100 | 50 | text=`Music`, uid=0 |
| 3 | "Off" half-pill | overlay | 6 | 12 | center+anchor | 420 | 137 | uid=20 |
| 4 | "On" half-pill | overlay | 6 | 14 | center+anchor | 450 | 137 | uid=40, lit |
| 5 | Save button   | B196x33 | — | — | center+anchor | -200 | 117 | text=`Save`, uid=200 |
| 6 | Cancel button | B196x33 | — | — | center+anchor | 20 | 117 | text=`Cancel`, uid=201 |

The Music button is `B220x33` — a wider variant of the main-menu
button. Save and Cancel revert to `B196x33` (same as main menu).

The "Off" / "On" indicators are sprite overlays (not button widgets):
two halves of a pill, at adjacent x positions, where the bright half
indicates the active setting. On the canonical reference dump, the
**On** side (right, lit) reflects the user's persisted preference
(music enabled). The candidate must render both sprites side-by-side;
the brighter one is the "on" sprite by sprite-bank assignment, not by
runtime selection state.

## Title overlay convention

Title text is a plain `Overlay` (not a Button), centered horizontally
on the screen via the formula
`x = 320 - (len(text) * textwidth) / 2`. `textbank = 135` (the larger
title font), `textwidth = 12`. Vertical position is `y = 14` (top
margin).

This same title pattern appears on `screen-options-display`,
`screen-options-controls`, plus likely future settings screens. It is
NOT a separately-named widget; it's an `Overlay` with text content
plus the textbank/textwidth fields used by the font-blit path.

## Activation / state

Static screen, no animation. 60-tick settle pin (same as
`screen-options.md`).

Buttons render in INACTIVE state at dump time
(`effectbrightness=128`, `res_index=7`) — the engine's
`activeobject = 0` initially highlights Music, but the canonical dump
captures the post-pulse steady state, matching the options hub
convention.

## Spec gaps to flag

- Sprite-bank docs do not explicitly document **bank 6 indices 12 and
  14** as the off/on half-pill pair. Add this to
  [`sprite-banks.md`](sprite-banks.md) once validated.
- `widget-button.md` documents only `B196x33`. The `B220x33` variant
  needs a row in [`widget-button.md`](widget-button.md): same overall
  pill geometry, 220 px wide, 33 px tall, sprite anchor identical
  to B196x33 modulo the width delta.
- The "On" indicator's bank-6 idx-14 sprite is brighter than idx-12
  by virtue of the sprite contents themselves (different RLE payload),
  not via a brightness LUT. If a candidate tries to render both as
  the same sprite and apply a brightness shader, the dump will not
  match.

## Cross-references

- [`screen-options.md`](screen-options.md) — parent (Audio button routes here)
- [`screen-options-display.md`](screen-options-display.md) — sibling, two of these toggles instead of one
- [`screen-options-controls.md`](screen-options-controls.md) — sibling, more complex form
- [`widget-button.md`](widget-button.md) — B220x33 (extend), B196x33 (existing)
- [`palette.md`](palette.md), [`sprite-banks.md`](sprite-banks.md), [`tick.md`](tick.md)
