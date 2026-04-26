# `TextInput` widget

**Source:** `clients/silencer/src/textinput.h`,
`clients/silencer/src/textinput.cpp`,
render dispatch in `clients/silencer/src/renderer.cpp:780..781` /
`Renderer::DrawTextInput` at `renderer.cpp:1510..1532`.

A single-line text-entry field. No chrome — the field is just text
laid out with a blinking caret when focused. Lobby-connect uses two:
username (`uid=1`) and password (`uid=2`); the password field has
`password = true` so its contents render as `*` characters.

## Properties

| Field | Type | Default | Notes |
| ----- | ---- | ------- | ----- |
| `x`, `y` | i16 | `0`, `0` | Top-left of the text area. |
| `width`, `height` | u16 | `0`, `0` | Pixel bounds. Used for hit-test only — rendering doesn't clip. |
| `res_bank` | u8 | `133` (caller usually re-affirms) | Font bank for the displayed text. |
| `fontwidth` | u8 | `6` | Glyph advance. |
| `maxchars` | u32 | caller-set | Max characters accepted. Lobby-connect uses 16 (username) / 28 (password). |
| `maxwidth` | u32 | caller-set | Visible character cap (often equal to `maxchars`). |
| `password` | bool | `false` | When true, the rendered text is `'*' × strlen(text)`. The actual `text` buffer is unchanged. |
| `numbersonly` | bool | `false` | If true, `ProcessKeyPress` rejects non-digit input. Not used on lobby-connect. |
| `inactive` | bool | `false` | If true, the field renders dim (`effectbrightness = 64`) and no caret blinks. Set by Interface focus dispatch when another TextInput steals focus. |
| `showcaret` | bool | `false` | Set true when this input is the Interface's `activeobject`. |
| `caretcolor` | u8 | caller-set or default | Palette index used for the caret rectangle. |
| `text` | char[256] | empty | Buffer. |
| `effectbrightness` | u8 | 128 (inherited from Object) | Used as the text brightness when `!inactive`. |
| `effectcolor` | u8 | 0 (inherited) | EffectColor tint. Lobby-connect leaves at 0. |

## Initial state on lobby-connect

- `usernameinput`: `(275, 293)`, `width=180, height=14, fontwidth=6, maxchars=16, maxwidth=16, password=false, uid=1`. **Initially focused** — `Interface::activeobject` is set to the username input's id, then `ActiveChanged` is called in the constructor, which propagates focus and sets `showcaret = true`.
- `passwordinput`: `(275, 320)`, same `res_bank=133, fontwidth=6, width=180, height=14`, but `maxchars=28, maxwidth=28, password=true, uid=2`. Not focused initially.

Both `text` buffers are empty on entry — the user types into them.

## Rendering

Per `Renderer::DrawTextInput` (`renderer.cpp:1510..1532`):

```
1. Get the visible substring:
       text = textinput.text + textinput.scrolled
   (scrolled is the first visible char index when the typed string
    exceeds maxwidth — for empty fields, scrolled == 0)
2. If password:
       replace text with '*' * strlen(text)
3. effectbrightness = textinput.effectbrightness
   if textinput.inactive: effectbrightness = 64
4. DrawText(surface, x, y, text, res_bank, fontwidth, alpha=false,
            effectcolor, effectbrightness)
5. If !inactive AND showcaret AND state_i % 32 < 16:
       caret_x = x + strlen(text) * fontwidth
       caret_y = y - 1
       caret_w = 1
       caret_h = round(height * 0.8)
       DrawFilledRectangle(surface, caret_x, caret_y,
                           caret_x + caret_w, caret_y + caret_h,
                           caretcolor)
```

Caret is a 1-px-wide vertical bar, height ≈ 0.8 of the field height
(11 px for a 14-px-tall field), color = `caretcolor` (palette
index — typically a bright shade in the active sub-palette).

The caret blinks on a 32-tick cycle: visible for ticks 0..15, hidden
for 16..31, repeating. (`state_i % 32 < 16` is the visibility test.)
For a deterministic dump, fire when `state_i % 32 < 16` is true so
the caret renders.

## Hit-testing

`MouseInside(mx, my)` returns true when the click lands in the
field's pixel bounds:

```
return mx in (x, x + width) and my in (y, y + height)
```

(For the focused field, clicks toggle the caret position via
`SetCaretPosition`; for an unfocused field, the click steals focus
via `Interface::ActiveChanged` and the previous focus's TextInput
becomes `inactive = true`.)

## What lobby-connect's hydration needs

Just the rendering side (DrawText + caret) since the dump captures a
single static frame with empty input buffers and the username field
focused:

- Username: render empty text, draw caret at `(275, 292)` (caret_y =
  293 - 1) with height `round(14 * 0.8) = 11`. Caret is visible if
  `state_i % 32 < 16` at dump time.
- Password: render empty text (no caret because not focused). The
  field is rendered identically except `inactive` is false (the
  *unfocused* TextInput still renders text at full brightness; only
  `showcaret` is gated by focus). Since `text` is empty, no glyphs
  draw and no caret draws → the field is invisible in the dump.

A static-frame hydration can hardcode this: draw a single caret
rectangle at the username's location, skip the rest.
