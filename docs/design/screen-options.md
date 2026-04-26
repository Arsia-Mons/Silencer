# screen-options — Options menu hub

The Options hub. A near-stripped-down main-menu shape: same background,
same B196x33 button widget, four buttons that route to the three
sub-screens (Controls / Display / Audio) plus a Go Back exit.

Reference dump: `/tmp/real_options_dump.ppm` (640×480 P6, sub-palette 1,
captured via `SILENCER_DUMP_STATE=OPTIONS` after a 60-tick settle).

## Sub-palette

`1` (the menu palette). Same as `screen-main-menu`. See
[`palette.md`](palette.md).

## Object inventory

In z-order (background first):

| z | Object | Type | Bank | Index | x | y | Notes |
| - | --- | --- | --- | --- | --- | --- | --- |
| 0 | Background | overlay | 6 | 0 | 0 | 0 | Fullscreen starfield + planet plate. Same as `screen-main-menu`. |
| 1 | Controls button | B196x33 | — | — | -89 | -142 | Text: `Controls`, uid=1, INACTIVE on dump |
| 2 | Display  button | B196x33 | — | — | -89 |  -90 | Text: `Display`,  uid=2, INACTIVE on dump |
| 3 | Audio    button | B196x33 | — | — | -89 |  -38 | Text: `Audio`,    uid=3, INACTIVE on dump |
| 4 | Go Back  button | B196x33 | — | — | -89 |   15 | Text: `Go Back`,  uid=0, INACTIVE on dump |

Coordinates are object-anchor (engine convention). Apply
`top_left = anchor + screen_center - sprite.offset` per
[`widget-overlay.md`](widget-overlay.md), with screen-center
`(320, 240)` and the B196x33 sprite anchor offset from
[`widget-button.md`](widget-button.md).

## What's NOT on this screen (vs main menu)

- No bank-208 logo overlay.
- No version-string overlay.
- No bank-7/bank-X "panel" frame (the Controls / Display / Audio
  sub-screens each layer additional UI; the hub does not).

These absences are load-bearing: if the candidate carries a logo or
version overlay over from main-menu code, the dump will not match the
reference.

## Activation / state

The screen is composed (`CreateOptionsInterface`-equivalent) on entry
and remains static. There is no fade-in animation, no logo-style hold
frame, no bank-208 dependency.

For dump purposes, the canonical reference fires after a **60-tick
settle** (no animation pin available — the menu is fully static once
the screen has been entered for one full pass). The candidate harness
should therefore tick its world for ≥60 ticks before dumping.

## Button widget defaults at dump time

All four buttons are in **INACTIVE** state on the canonical dump (no
hover, no selection pulse showing). Per `widget-button.md`:

- `state = INACTIVE`
- `effectbrightness = 128`
- `res_index = 7` (the resting frame)

If a candidate paints the Controls button as ACTIVATING (the engine's
default tabbed-selection state for `activeobject = 0`) the button's
internal bevel will pulse and the dump will show a brighter
`Controls` row than the other three. The reference shows all four
buttons at uniform brightness — i.e., **the canonical dump captures the
post-tab-selection-cycle steady state**, where the active button has
already finished its pulse and settled. Match this in the candidate.

## Tab order (informational, not visible in dump)

`Controls → Display → Audio → Go Back`, with `activeobject = 0`
(Controls receives focus on entry) and `buttonescape = Go Back`
(Esc routes there). Visible only via input handling, irrelevant to
the static dump.

## Spec gaps to flag

- The "60-tick settle" pin is not animation-state-anchored the way the
  main-menu pin is (bank-208 idx-60). If the engine is later modified
  to add a button-pulse animation, the 60-tick number may need to
  advance to a multiple of the pulse period.
- The button x-anchor (-89) is hard-coded in the engine's
  `CreateOptionsInterface`. The B196x33 sprite is 196 px wide; with
  screen-center 320 and offset_x ≈ 9 (from `widget-button.md`'s anchor
  convention), the resulting on-screen `top_left.x = -89 + 320 - 9 = 222`,
  putting the button right edge at `222 + 196 = 418`. That places the
  button column noticeably right of true center — confirmed by the
  reference dump showing the button column biased ≈30 px right of
  geometric center, the planet starting where the buttons end. If a
  candidate centers the buttons (`x = 0`), they'll sit too far left.

## Cross-references

- [`screen-main-menu.md`](screen-main-menu.md) — same background, same button widget
- [`widget-button.md`](widget-button.md) — B196x33 button sprite, states, anchors
- [`widget-overlay.md`](widget-overlay.md) — anchor convention
- [`widget-interface.md`](widget-interface.md) — Interface composition
- [`palette.md`](palette.md) — sub-palette index for menu screens
- [`tick.md`](tick.md) — tick semantics for dump pinning
