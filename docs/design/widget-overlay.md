# `Overlay` widget

**Source:** `clients/silencer/src/overlay.h`,
`clients/silencer/src/overlay.cpp`,
render dispatch in `clients/silencer/src/renderer.cpp` lines
565..581 (sprite path) and 909..925 (text path).

A general-purpose drawable that renders **either** a sprite (from
some bank) **or** a text string, but never both at once. The main
menu uses three Overlays — background, logo, version — to cover all
three modes the menu needs.

## Properties

| Field | Type | Default | Notes |
| ----- | ---- | ------- | ----- |
| `x`, `y` | i16 | `0`, `0` | Position. **Sprite mode** uses world coords (camera offset added at draw); **text mode** uses raw screen coords (no camera). |
| `res_bank` | u8 | `0xFF` | Sprite bank. `0xFF` = no sprite. |
| `res_index` | u8 | `0` | Sprite frame within the bank (overwritten by `Tick` for animated banks — see below). |
| `state_i` | u8 | `0` | Animation cursor; bumped each tick. |
| `text` | string | `""` | Empty = sprite mode; non-empty = text mode. |
| `textbank` | u8 | `135` | Font bank for text mode (see [font.md](font.md)). |
| `textwidth` | u8 | `8` | Glyph advance for text mode. |
| `drawalpha` | bool | `false` | Use alpha blit for text glyphs. |
| `effectcolor` | u8 | `0` | `EffectColor` tint (sprite or text). `0` = none. |
| `effectbrightness` | u8 | `128` | `EffectBrightness` (sprite or text). `128` = neutral. |
| `textcolorramp` | bool | `false` | If `effectcolor != 0`, route through `EffectRampColor` instead of `EffectColor`. |
| `textallownewline` | bool | `false` | If true, split `text` on `\n` and stack lines `textlineheight` apart. |
| `textlineheight` | i32 | `10` | Line spacing for multi-line text. |

Constructor (`overlay.cpp:3`) initializes `res_bank = 0xFF`,
`textbank = 135`, `textwidth = 8`. Callers reassign the fields they
care about.

## Tick — per-bank animation

`Overlay::Tick` runs only if `customsprite.empty()`; otherwise
`res_bank/res_index` are overwritten based on bank:

| `res_bank` | Behavior |
| ---------- | -------- |
| `54`  | `res_index = state_i`; loops at `state_i >= 9` |
| `56`  | `res_index = 0` (static) |
| `57`, `58`  | `res_index = state_i / 4`; holds at frame 16 with random escape |
| `171` | `res_index = (state_i / 2) % 4` |
| **`208`** (main-menu logo) | Three-phase loop. `state_i < 60`: `res_index = state_i/2 + 29` (fade in 29→60). `60 ≤ state_i < 120`: `res_index = 60` (hold). `state_i ≥ 120`: `res_index = (120 - state_i/2) + 29` (fade out). When the fade-out formula produces `res_index ≤ 29`, **reset `state_i` to `-1`** so the unconditional `state_i++` at the end of `Tick` lands it back at `0` for the next cycle, restarting the fade-in. As a safety clamp, every Tick that hits the `state_i ≥ 120` branch should also cap `res_index` so it never exceeds 60 or drops below 29 even if `state_i` overflows. |
| `222` | One-shot animation; destroys self at `state_i >= 3` |
| anything else | `res_index` left as caller assigned |

`state_i++` runs unconditionally at the end of `Tick`.

For the main menu the only animated overlay is bank 208 (the logo).
The background (bank 6 idx 0) and version (text mode) hit the
"anything else" / text branches and do not animate.

## Render — sprite mode

When `text` is empty, the renderer treats the Overlay like any
other sprite-bearing object:

```
src = sprite_banks[res_bank][res_index]
dst.x = x - sprite.offset_x + camera.GetXOffset()
dst.y = y - sprite.offset_y + camera.GetYOffset()
if effectbrightness != 128: copy → EffectBrightness
blit src onto framebuffer at dst
```

For the main-menu camera position `(320, 240)` on a 640×480
screen, `GetXOffset = GetYOffset = 0` (see
[widget-interface.md](widget-interface.md)). So `dst = (x - off_x,
y - off_y)` — exactly the anchor convention from
[sprite-banks.md](sprite-banks.md).

Bank 222 has special-case handling (re-routed into the lighting
pass) — not relevant on the menu.

## Render — text mode

When `text` is non-empty, the renderer takes a separate path
(`renderer.cpp:909..925`) that draws text at **raw `(x, y)`** with
no camera offset and no sprite:

```
if textallownewline:
    yoffset = 0
    for line in split(text, '\n'):
        DrawText(surface, x, y + yoffset, line, textbank, textwidth,
                 drawalpha, effectcolor, effectbrightness, textcolorramp)
        yoffset += textlineheight
else:
    DrawText(surface, x, y, text, textbank, textwidth, drawalpha,
             effectcolor, effectbrightness, textcolorramp)
```

See [font.md](font.md) for `DrawText` semantics.

## On/off toggle pattern (used by options-display and options-audio)

These two screens express boolean settings as a *pair* of small
overlays placed side-by-side, with the active state shown by which
overlay renders the "bright" sprite vs the "dim" one. There's no
dedicated `Toggle` widget for this — it's two `Overlay` objects
that the screen's Tick code re-points at different sprite indices
each frame.

The shared sprite layout is bank 6, indices 12..15 (each `20 × 33`,
offset `(0, 0)`):

| Index | Meaning |
| ----- | ------- |
| `12`  | "Off" label, dim (option is currently ON) |
| `13`  | "Off" label, bright (option is currently OFF — the OFF state is selected) |
| `14`  | "On" label, dim (option is currently OFF) |
| `15`  | "On" label, bright (option is currently ON — the ON state is selected) |

A row hosts two overlays: one tagged with `uid` in `[20..39]` (the
"Off" half) sitting at `(420, 137 + i*53)` next to its row, and a
sibling with `uid` in `[40..59]` (the "On" half) at `(450, 137 + i*53)`.
The screen's Tick code reads the corresponding `Config` flag and
sets each overlay's `res_index` per the table above.

Default config has every covered toggle in the **ON** state
(`fullscreen = true`, `scalefilter = true`, `music = true`), so
default-config dumps render every off-overlay at idx 12 and every
on-overlay at idx 15.

A static-frame hydration that hardcodes default config can just set
the `res_index`es at construction time (`12` and `15` for ON; or
`13` and `14` for OFF) and skip the dynamic Tick step entirely.

## How the main menu uses it

| Instance  | Mode    | Bank | Index/Text | Position | Notes |
| --------- | ------- | ---- | ---------- | -------- | ----- |
| Background | sprite | 6    | 0 (640×480 plate) | `(0, 0)` | Drawn first; covers the whole framebuffer. `effectbrightness = 128`. |
| Logo       | sprite | 208  | 29..60 (animated) | `(0, 0)` | `Tick` walks `res_index`; the sprite's `offset_x = -7, offset_y = -222` (idx 60) places it on the upper-left half. |
| Version    | text   | —    | `"Silencer v" + version_string` | `(10, 463)` | `textbank = 133`, `textwidth = 11`, no tint, `effectbrightness = 128`. `y = 480 - 10 - 7`. |

These are constructed in `Game::CreateMainMenuInterface`
(`game.cpp:2266..2278`).

## Hit-testing

`Overlay::MouseInside` exists but the main menu's three overlays
are decorative — none of them are wired to clicks. A hydration
that's only rendering the menu can skip the hit-test path entirely.
