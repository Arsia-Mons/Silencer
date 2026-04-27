# screen-options-controls — Controls options (key bindings)

The most structurally complex options screen: a bordered panel frame,
a scrollable 6-row form (each row = label + two key-name buttons + an
OR/AND connector), and Save / Cancel.

Reference dump: `/tmp/real_optionscontrols_dump.ppm` (640×480 P6,
sub-palette 1, captured via `SILENCER_DUMP_STATE=OPTIONSCONTROLS`).

## Sub-palette

`1`.

## Object inventory

In z-order:

| z | Object | Type | Bank | Index | x | y | Notes |
| - | --- | --- | --- | --- | --- | --- | --- |
| 0 | Background          | overlay | 6 | 0 | 0 | 0 | Fullscreen starfield+planet plate |
| 1 | Frame panel         | overlay | 7 | 7 | 0 | 0 | Bordered green-bevel panel covering most of the screen |
| 2 | Title text          | overlay (font 135, w=12) | — | — | `320 - len("Configure Controls")*12/2` | 14 | text=`Configure Controls` |
| 3..8 | Key-name labels (6×) | overlay (font 134, w=10) | — | — | 80 | 95 + i*53 | i=0..5 |
| 9..14 | Key1 buttons    (6×) | B112x33 | — | — | -30 | 0 + i*53 | uid=i |
| 15..20 | OR/AND connectors (6×) | BNONE | — | — | 383 | 95 + i*53 | uid=150+i, w=40, h=30, font 134, w=9 |
| 21..26 | Key2 buttons    (6×) | B112x33 | — | — | 120 | 0 + i*53 | uid=100+i |
| 27 | Scrollbar           | scrollbar | — | 9 | (engine-internal) | (engine-internal) | scrollpixels=53, scrollmax = numkeys − 6 |
| 28 | Save button         | B196x33 | — | — | -200 | 117 | text=`Save`, uid=200 |
| 29 | Cancel button       | B196x33 | — | — | 20 | 117 | text=`Cancel`, uid=201 |

Row stride is `+53`. Six rows visible; the scrollbar advances by 53 px
per click to reveal further keys.

## What's on the visible canonical dump

The reference dump shows row `i = 0..5` populated with these key
labels (left to right per row: `<label>: <key1> OR/AND <key2>`):

| i | Label             | Key1 | Connector | Key2  |
| - | ----------------- | ----- | --------- | ----- |
| 0 | `Move Up:`        | `Up`     | `OR`  | (empty) |
| 1 | `Move Down:`      | `Down`   | `OR`  | (empty) |
| 2 | `Move Left:`      | `Left`   | `OR`  | (empty) |
| 3 | `Move Right:`     | `Right`  | `OR`  | (empty) |
| 4 | `Aim Up/Left:`    | `Up`     | `AND` | `Left`  |
| 5 | `Aim Up/Right:`   | `Up`     | `AND` | `Right` |

The connector text alternates between `OR` (single-key bind, second
slot empty) and `AND` (two-key combo, both slots populated).

Empty key2 buttons render with no text glyph but still draw the
B112x33 button outline. The candidate should still produce the empty
button shape.

## Frame panel (bank 7 idx 7)

A new widget shape not used by main menu or options hub: a 4-corner
bevel panel that fills most of the screen interior. Renders as an
**overlay** at `(x=0, y=0)` (literal screen coords), with the sprite
itself supplying the panel art.

If `bank 7 idx 7` does not decode as a single full-frame overlay
(the sprite may use the tile-mode RLE codec for memory efficiency
given its size), the candidate must support tile-mode RLE — already
validated for `bank 196` button pills, same codec branch.

## Title-text and key-name fonts

- **Title** uses `textbank = 135, textwidth = 12` — the same large
  title font as Display / Audio.
- **Key-name labels** use `textbank = 134, textwidth = 10` — a smaller
  body font.
- **OR/AND connectors** use `textbank = 134, textwidth = 9`.
- **Key buttons** (B112x33) likely use the default button font from
  `widget-button.md`; check sprite-banks.md for `bank 196`'s
  button-text font (probably `bank 132 / 133`).

## B112x33 button widget

Half-width button used for individual key labels. Same construction as
`B196x33` but narrower (~112 px). Sprite-anchor convention is the
canonical one from [`widget-overlay.md`](widget-overlay.md):
`top_left = anchor - sprite.offset` (screen-centring is already in
the sprite's negative offset; do not add a screen-center term).

If a candidate has only the B196x33 sprite loaded, this screen will
fail to render the key buttons — extend the candidate's sprite-load
list to include the B112x33 sprite bank.

## BNONE button widget

The OR/AND connectors are rendered as `Button` objects with
`SetType(BNONE)` — i.e., **no button-pill sprite**, just a transparent
container that paints text inside a `width × height` bounding box.
At the rendering layer, a BNONE button is essentially equivalent to
a text overlay positioned at the button's center.

## Scrollbar widget (new)

Vertical scrollbar at the right edge of the frame panel. The reference
dump shows: a track + thumb (in upper position because
`scrollposition = 0` initially), with chevron caps at top and bottom.

Spec gap: there is no `widget-scrollbar.md` yet. Add one when the
candidate implements this. Minimal fields:
- `res_index = 9` (sprite index in the scrollbar bank)
- `scrollpixels = 53` (one row stride per scroll step)
- `scrollmax = numkeys - 6` (engine-internal — the candidate can
  hard-code 0 for the static dump since the dump is at scroll
  position 0)

For the canonical dump, `scrollposition = 0` and the thumb is at the
top of the track.

## Activation / state

Static screen, 60-tick settle pin.

Buttons render in INACTIVE state at dump time. The
engine's `activeobject = 0` initially highlights the first key1
button (Move Up's Up); the canonical dump captures post-pulse steady
state.

## Spec gaps to flag (significant)

- **`widget-button.md`** must be extended: B112x33 (key buttons),
  B220x33 (Display/Audio toggle rows), BNONE (text-only container).
- **`widget-scrollbar.md`** does not exist. Author it from the
  reference dump's visual + the scrollbar fields above.
- **`widget-frame.md` or sprite-banks coverage of bank 7 idx 7** —
  the bordered panel sprite is a new visual element. Confirm whether
  it's a single tile-mode overlay or composed from corner sprites.
- **The 6-row form pattern** (label / B112x33 / BNONE-OR-AND /
  B112x33) is reusable across future "list-of-key-bindings" screens.
  Worth its own widget doc once validated.
- The keyname strings (`Up`, `Down`, etc.) come from a `keynames[]`
  array indexed by `keyname[i + scrollposition]`. For the static
  dump at `scrollposition = 0`, the candidate can hard-code the six
  visible label/key strings from this doc's table — but for any
  parametric candidate, the keyname array would need its own spec.

## Cross-references

- [`screen-options.md`](screen-options.md) — parent
- [`screen-options-audio.md`](screen-options-audio.md), [`screen-options-display.md`](screen-options-display.md) — siblings (simpler forms)
- [`widget-button.md`](widget-button.md) — extend with B112x33 / B220x33 / BNONE
- [`widget-overlay.md`](widget-overlay.md), [`palette.md`](palette.md), [`sprite-banks.md`](sprite-banks.md), [`tick.md`](tick.md)
