# screen-lobby-connect — Lobby connection screen

The login form. Renders a panel-on-black layout (no starfield/planet
background — the panel's exterior is just `Clear(0)`), with a
multi-line connection-log textbox, two text input fields
(Username, Password), and Login / Cancel buttons.

Reference dump: `/tmp/real_lobbyconnect_dump.ppm` (640×480 P6,
**sub-palette 2**, captured via `SILENCER_DUMP_STATE=LOBBYCONNECT`
after a 60-tick settle. Note: the textbox content reflects runtime
state — the canonical dump shows `Connecting to 127.0.0.1: 517 /
Hostname resolved / Connection failed` because no lobby server was
running. The candidate renders the textbox structure; exact text
content is non-structural runtime data and not gated.).

## Sub-palette

`2` (the **lobby palette** — different from menu sub-palette 1 used
by main_menu and options-family screens). Set in the engine's
LOBBYCONNECT `stateisnew` block via `renderer.palette.SetPalette(2)`.

Implementation note: the candidate must support multiple sub-palettes
in a single dump-mode run (or pick the palette from screen state at
dump time). Until LOBBYCONNECT, every screen used sub-palette 1.

## Object inventory

In z-order (background first):

| z | Object | Type | Bank | Index | x | y | Notes |
| - | --- | --- | --- | --- | --- | --- | --- |
| 0 | Background panel | overlay | 7 | 2 | 0 | 0 | The full panel frame including the inner textbox border, divider, input-field wells, button-row well. Different idx from CONTROLS' bank-7 idx-7 frame. |
| 1 | Textbox | textbox | 133 (font) | — | 185 | 101 | width=250, height=170, lineheight=11, fontwidth=6. Multi-line scrollable text region; canonical dump shows connection log. |
| 2 | "Username" label | overlay (font 134, w=9) | — | — | 190 | 291 | text=`Username` |
| 3 | "Password" label | overlay (font 134, w=9) | — | — | 190 | 318 | text=`Password` |
| 4 | Username input | textinput | 133 (font) | — | 275 | 293 | width=180, height=14, fontwidth=6, maxchars=16. Active on screen entry — reference shows blinking cursor. |
| 5 | Password input | textinput | 133 (font) | — | 275 | 320 | width=180, height=14, fontwidth=6, maxchars=28, password=true. Empty on canonical dump. |
| 6 | Login button | B52x21 | — | — | 264 | 339 | text=`Login`, uid=0 |
| 7 | Cancel button | B52x21 | — | — | 321 | 339 | text=`Cancel`, uid=1 |

Coordinates here are **literal screen coords** (overlay/textbox/textinput
convention) for the static-position widgets. Buttons (`B52x21`) follow
the same canonical formula as B196x33 / B220x33 / B112x33:
`top_left = anchor - sprite.offset` per
[`widget-overlay.md`](widget-overlay.md). The login (264) and cancel
(321) anchors put the two buttons side-by-side on the panel's bottom row.

## What's NOT on this screen

- No starfield/planet bank-6 idx-0 background. The panel's exterior
  area (outside the bank-7 idx-2 sprite) is `Clear(0)` (black).
- No bank-208 logo overlay.
- No version overlay.
- No title overlay (the panel sprite includes the panel chrome itself).

These are load-bearing absences. A candidate that paints the menu's
starfield bg first (a habit from previous screens) will have it visible
*around* the panel, which the reference does not show.

## New widgets compared to prior screens

### B52x21 button variant

A small button (~52 × 21 px) used for compact form action rows.
Same sprite/anchor convention as the larger B196x33 / B220x33 / B112x33
variants. Spec gap: extend [`widget-button.md`](widget-button.md) to
list B52x21 alongside the others.

### TextBox widget

Multi-line scrollable text region. Engine fields:
- `x`, `y`, `width`, `height` — bounding rect (literal screen coords)
- `res_bank` — font bank (133 here)
- `lineheight` — vertical px per line (11)
- `fontwidth` — horizontal px per glyph advance (6)
- text content — line-by-line, may be truncated at `width`

For a static dump where no scrolling is happening, the candidate just
needs to render whatever text is present using the font path.
**Spec gap:** there is no `widget-textbox.md`. Author one when the
candidate implements this widget.

### TextInput widget

Single-line input field. Engine fields:
- `x`, `y`, `width`, `height` — bounding rect (literal screen coords)
- `res_bank` — font bank (133)
- `fontwidth` — glyph advance (6)
- `maxchars` — max input length
- `password` — when true, render as `*` characters
- `uid` — interface tab order

The active TextInput shows a blinking cursor on the canonical dump
(Username's cursor is visible at left edge). Cursor blink phase is
**not deterministic** — the reference may show cursor on or off
depending on settle-tick alignment. Treat cursor presence/absence
as non-structural; gate only on the input-field outline / position.

**Spec gap:** there is no `widget-textinput.md`. Author one when the
candidate implements this widget.

### Bank 7 idx 2 panel sprite

A different bank-7 sprite from CONTROLS' idx 7 frame. The idx-2
sprite includes:
- Outer bordered rectangle covering ~(80, 80) to (560, 400)
- Inner textbox border with right-side scrollbar (currently
  un-thumbed because no scrollable content)
- Horizontal divider below the textbox
- Username + Password input-field background wells
- Bottom button-row well

Likely uses tile-mode RLE (the same codec branch validated for
bank-6 / bank-7 idx-7 / bank-196 in prior Ralphs).

## Activation / state

Static screen modulo:
- Cursor blink in the active TextInput (Username on entry).
- Connection log in the TextBox grows as the engine attempts
  hostname resolution, connection, etc. — entirely runtime-dependent.

For dump purposes the **60-tick settle pin** captures a stable enough
state. The candidate doesn't need to simulate any animation or
runtime state — it can render: empty TextBox (or with placeholder
text), empty input fields, both buttons in INACTIVE state.

## Spec gaps to flag

- `widget-button.md` needs B52x21 alongside other variants.
- `widget-textbox.md` does not exist.
- `widget-textinput.md` does not exist.
- `palette.md` should explicitly call out sub-palette 2 as the lobby
  palette and document its activation point (LOBBYCONNECT and LOBBY
  states).
- `sprite-banks.md` — bank 7 idx 2 (lobby connect panel) is a
  separate sprite from idx 7 (controls panel); inventory should call
  out both.

## Cross-references

- [`screen-options-controls.md`](screen-options-controls.md) — also uses bank 7 frame sprite, but at a different idx and over a different background
- [`widget-button.md`](widget-button.md) — B52x21 variant (extend)
- [`widget-overlay.md`](widget-overlay.md) — anchor convention
- [`palette.md`](palette.md), [`sprite-banks.md`](sprite-banks.md), [`tick.md`](tick.md)
