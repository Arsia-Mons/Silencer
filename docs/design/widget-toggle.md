# `Toggle` widget

**Source:** `clients/silencer/src/toggle.h`,
`clients/silencer/src/toggle.cpp`,
render dispatch in `clients/silencer/src/renderer.cpp:808..813`
(plus the standard sprite path for the icon itself).

A binary radio-group element: visually it's a sprite-backed icon
that brightens when selected and dims when not. Multiple Toggles
sharing the same `set` value form a radio group — clicking one
deselects siblings (handled by Interface dispatch, not by Toggle
itself).

The lobby uses five Toggles for the agency-icon picker (Noxis,
Lazarus, Caliber, Static, Black Rose); each renders a sprite from
bank 181.

## Properties

| Field | Type | Default | Notes |
| ----- | ---- | ------- | ----- |
| `x`, `y` | i16 | `0`, `0` | Position in screen coords. |
| `res_bank` | u8 | `0xFF` | Sprite bank for the icon (caller-set). Toggles in scope use bank 181 (agency icons) or 7 (UI checkboxes). |
| `res_index` | u8 | `0` | Initial frame; `Tick` may rewrite it per the per-bank rule below. |
| `selected` | bool | `false` | Radio-group selection. **Caller** sets the initial state; Interface flips it on click. |
| `set` | u8 | `0` | Radio-group id. Toggles with the same non-zero `set` are mutually exclusive — Interface deselects siblings on click. `set = 0` means "not in a group." |
| `width`, `height` | u8 | `0`, `0` | Hit-test bounds. **Tick rewrites these** to match the live sprite frame's `(spritewidth, spriteheight)`. |
| `text` | char[64] | empty | Optional label drawn beneath the icon. Lobby agency Toggles don't use it. |
| `effectcolor`, `effectbrightness` | u8 | `0`, `128` | Standard sprite-tint params; `Tick` rewrites these per the per-bank rule. |

## Tick — per-bank visual state

Per `toggle.cpp:15..37`, `Toggle::Tick` rewrites visual params based
on `res_bank` and `selected`:

| Bank | Selected = true | Selected = false |
| ---- | --------------- | ---------------- |
| `181` (agency icons) | `effectcolor = 112`, `effectbrightness = 128` (full) | `effectcolor = 112`, `effectbrightness = 32` (very dim) |
| `7` (UI checkboxes) | `res_index = 18` | `res_index = 19` |
| anything else | (no Tick rewrite) | (no Tick rewrite) |

After the visual rewrite, `width` / `height` are pulled from the
current sprite frame's dimensions — so the hit-rect tracks whatever
frame is currently rendered.

## Hit-test

```
inside =
    object.x - sprite.offset_x  <  mx  <  object.x + width - sprite.offset_x
    AND
    object.y - sprite.offset_y  <  my  <  object.y + height - sprite.offset_y
```

(`width` / `height` are set by Tick from the current sprite's
dimensions; for bank-181 agency icons they're 28..32 wide and
25..30 tall — see [sprite-banks.md](sprite-banks.md).)

## Render

The icon goes through the standard sprite blit path (anchor
shift, `EffectColor` if non-zero, `EffectBrightness` if not 128).
Then a separate pass at `renderer.cpp:808..813` draws the optional
text label centered horizontally on `(object.x, object.y)` using
font bank 134 advance 9 — but only if `text[0] != '\0'`.

For the lobby agency Toggles `text` is empty, so only the icon
renders.

## Radio-group dispatch (Interface side)

When a Toggle with `set != 0` becomes `selected = true` (via mouse
click handled by `Interface::ActiveChanged` /
`NotifyToggleSelected`), every other Toggle in the same set has its
`selected` flipped to `false`. The newly-selected one's `Tick` then
brightens it; the deselected ones' `Tick` dims them.

For a static-frame hydration that captures default state, no
dispatch is needed — set `selected = true` on whichever Toggle the
default `Config::defaultagency` should highlight, leave the others
`false`, and call `Tick()` once before drawing so the visual params
get rewritten correctly.

## How the lobby uses it

Five Toggles in the character sub-interface
([screen-lobby.md](screen-lobby.md)):

| `uid` | Agency  | Position `(x, y)` | `res_bank` | `res_index` |
| ----- | ------- | ----------------- | ---------- | ----------- |
| 1     | Noxis   | (20, 90)  | 181 | 0 |
| 2     | Lazarus | (62, 90)  | 181 | 1 |
| 3     | Caliber | (104, 90) | 181 | 2 |
| 4     | Static  | (146, 90) | 181 | 3 |
| 5     | Black Rose | (188, 90) | 181 | 4 |

All in `set = 1`. The Toggle whose icon matches
`Config::GetInstance().defaultagency` (default `NOXIS = 0`,
matching the Noxis Toggle's `uid = 1`) is `selected = true` on
construction; the others are `false`. Tick brightens the selected
one to `effectbrightness = 128` and dims the others to `32`.

The icons are NOT in `tabobjects` (the constructor adds them to
`objects` only — see `game.cpp:2917..2926`); keyboard navigation
skips them. Selection is mouse-only on this screen.
