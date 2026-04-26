# `Button` widget — variants

**Source:** `clients/silencer/src/button.h`,
`clients/silencer/src/button.cpp`,
render dispatch in `clients/silencer/src/renderer.cpp:699..705`.

The engine has 7 button variants. This doc covers the ones the
hydrated screens actually use. New variants will be added below
as more screens come online.

| Variant | Used by | Has sprite chrome? |
| ------- | ------- | ------------------ |
| `B196x33` | main menu, options menu (default), Save/Cancel on every options sub-screen | yes |
| `B112x33` | options-controls c1/c2 key buttons | yes |
| `B220x33` | options-display "Fullscreen"/"Smooth Scaling" rows; options-audio "Music" row | yes |
| `B52x21` | lobby-connect Login/Cancel buttons | no (text-only, fixed dimensions) |
| `B156x21` | lobby Go Back / Create Game / Join Game buttons | yes (sprite-backed; brightness-only animation) |
| `BNONE` | options-controls "OR/AND" op buttons (text-only, custom width/height) | no |
| `B236x27`, `BCHECKBOX` | not yet exercised | TBD when a screen needs them |

## `B196x33` constants

| Field | Value |
| ----- | ----- |
| Width × height | 196 × 33 |
| Sprite bank | 6 |
| Base `res_index` | 7 |
| Hover frames | `res_index` 7..11 (`base + state_i`) |
| Text font bank | 135 |
| Text advance | 11 |
| Text `yoff` | 8 |
| Hover sound | `whoom.wav` (engine plays at `state_i == 0` of `ACTIVATING`) |

Sprite header (from `shared/assets/bin_spr/SPR_006.BIN` idx 7):
`offset_x = -310, offset_y = -288`. The button's logical
`(x, y)` is the **anchor** — the rendered pill spans
`(x + 310, y + 288)..(x + 506, y + 321)` after the offset is
applied (`top_left = object - sprite_offset`). This is how a
`B196x33` placed at `object.x = 40, object.y = -134` ends up at
screen `(350, 154)..(546, 187)` — the right side of the menu — with
no explicit right-aligning logic anywhere.

## Lifecycle / state machine

```
States:  INACTIVE  →  ACTIVATING  →  ACTIVE  →  DEACTIVATING  →  INACTIVE
```

Transitions:

| Trigger | New state |
| ------- | --------- |
| Mouse enters bbox or focus given | `ACTIVATING` (`state_i = 0`) |
| Mouse leaves bbox or focus lost | `DEACTIVATING` (`state_i = 0`) |
| `state_i` reaches 4 in `ACTIVATING` | `ACTIVE` |
| `state_i` reaches 4 in `DEACTIVATING` | `INACTIVE` |
| Mouse pressed inside bbox | `clicked = true` (one-shot edge) |

Per-tick effect (set inside `Button::Tick`):

```
INACTIVE:
    res_index = 7
    effectbrightness = 128

ACTIVATING:
    if state_i == 0: play "whoom.wav"
    res_index = 7 + state_i        # 7, 8, 9, 10, (11 on transition)
    effectbrightness = 128 + state_i * 2     # 128, 130, 132, 134, (136)

ACTIVE:
    res_index = 11                  # base + 4
    effectbrightness = 136

DEACTIVATING:
    res_index = 7 + (4 - state_i)  # reverse ramp
    effectbrightness = 128 + (4 - state_i) * 2
```

`state_i` increments at the end of every `Tick`. The animation thus
takes **4 ticks ≈ 168 ms** in either direction.

## Hit-testing

```
top_left_x = object.x - sprite.offset_x       # = object.x + 310 for B196x33
top_left_y = object.y - sprite.offset_y       # = object.y + 288 for B196x33
inside = top_left_x < mx < top_left_x + 196
       AND top_left_y < my < top_left_y + 33
```

`button.cpp:166`. Note that `mouseInside` reads sprite offsets
from the **current** `res_index` — but for `B196x33` all hover
frames share the same offsets (`-310, -288`), so the hit-rect is
stable across the animation.

## Rendering pipeline

```
1. src = sprite_banks[6][res_index]                 # 7..11
2. dst.x = x - src.offset_x + camera.GetXOffset()
   dst.y = y - src.offset_y + camera.GetYOffset()   # GetXOffset/GetYOffset = 0 in MAINMENU
3. if effectbrightness != 128:
       work = copy_of(src)
       EffectBrightness(work, effectbrightness)
   else:
       work = src
4. blit work onto framebuffer at dst (transparency = palette idx 0)
5. compute label position via GetTextOffset:
       xoff = (196 - strlen(text) * 11) / 2
       yoff = 8
       textX = dst.x + xoff
       textY = dst.y + yoff
6. DrawText(surface, textX, textY, button.text, 135, 11,
            alpha=true, color=0, brightness=effectbrightness)
```

Effect-brightness on the label is critical: button labels
*also* ramp from `128 → 136` during hover, brightening the glyph
pixels in lockstep with the chrome. A hydration that ignores
brightness on text will see a flat-tone label sitting on a
brightening pill.

## Tab order and Enter / Escape

`Interface` (see [widget-interface.md](widget-interface.md)) tracks
which button is `buttonenter` (Enter triggers it) and `buttonescape`
(Escape triggers it). On the main menu these are wired to the
"Tutorial" and "Exit" buttons respectively (see
[screen-main-menu.md](screen-main-menu.md)).

## `B112x33` constants

| Field | Value |
| ----- | ----- |
| Width × height | 112 × 33 |
| Sprite bank | 6 |
| Base `res_index` | 28 |
| Hover frames | `res_index` 28..32 (`base + state_i`) |
| Text font bank | 135 |
| Text advance | 11 |
| Text `yoff` | 8 |
| Hover sound | `whoom.wav` |

Sprite header (`shared/assets/bin_spr/SPR_006.BIN` idx 28):
`offset_x = -302, offset_y = -86`. So a `B112x33` placed at
`object.x = -30, object.y = 0` renders with its top-left at
`(-30 + 302, 0 + 86) = (272, 86)` and footprint `112 × 33`.

State machine, hit-testing, rendering, and label centering are
identical to `B196x33` above; only the dimensions, base index, and
anchor offset change. The label-position math becomes:

```
xoff = (112 - strlen(text) * 11) / 2
yoff = 8
```

## `B220x33` constants

| Field | Value |
| ----- | ----- |
| Width × height | 220 × 33 |
| Sprite bank | 6 |
| Base `res_index` | 23 |
| Hover frames | `res_index` 23..27 (`base + state_i`) |
| Text font bank | 135 |
| Text advance | 11 |
| Text `yoff` | 8 |
| Hover sound | `whoom.wav` |

Sprite header (`shared/assets/bin_spr/SPR_006.BIN` idx 23):
`offset_x = -76, offset_y = -86`. So a `B220x33` placed at
`object.x = 100, object.y = 50` renders with its top-left at
`(100 + 76, 50 + 86) = (176, 136)` and footprint `220 × 33`.

State machine, hit-testing, rendering, and label centering are
identical to `B196x33` and `B112x33`; only dimensions, base index,
and anchor offset change. Label-position math:

```
xoff = (220 - strlen(text) * 11) / 2
yoff = 8
```

## `B52x21` constants

| Field | Value |
| ----- | ----- |
| Width × height | **fixed** 52 × 21 |
| Sprite bank | `0xFF` (none — text-only) |
| Text font bank | 133 |
| Text advance | 7 |
| Text `yoff` | 8 |
| Text `xoff` extra | **+1 px after centering** |

Text-only button with fixed dimensions. Like `BNONE`, there's no
sprite chrome to render and no anchor offset to subtract; unlike
`BNONE`, the dimensions are baked into the variant and `yoff = 8`
gives proper vertical centering inside the 21-px tall hit-rect.

Label-position math (from `Button::GetTextOffset`, `button.cpp:188`):

```
xoff = (52 - strlen(text) * 7) / 2 + 1     // +1 px nudge after centering
yoff = 8
textX = button.x + xoff                    # no anchor offset to subtract
textY = button.y + 8                       # ditto
```

Hit-rect (since `res_bank == 0xFF`, `spriteoffsetx[0xFF][...]` reads
as zero in a faithful port — match this in the hydration):

```
inside =
    button.x < mx < button.x + 52
    AND
    button.y < my < button.y + 21
```

State machine still runs (so brightness ramps 128 → 136 on hover);
the ramp brightens the label glyphs but no chrome.

## `B156x21` constants

| Field | Value |
| ----- | ----- |
| Width × height | 156 × 21 |
| Sprite bank | 7 |
| `res_index` | 24 (fixed — does **not** advance per state) |
| Hover animation | **brightness-only** — sprite frame stays at idx 24, only `effectbrightness` ramps |
| Text font bank | 134 |
| Text advance | 8 |
| Text `yoff` | 4 (different from the 8 used by the larger sprite-backed variants) |
| Hover sound | `whoom.wav` |

Sprite header (`shared/assets/bin_spr/SPR_007.BIN` idx 24):
`offset_x = 0, offset_y = 0`. So a `B156x21` placed at
`object.x = 473, object.y = 29` renders with its top-left at
`(473, 29)` and footprint `156 × 21`. Anchor offsets are zero, so
the logical position **is** the rendered top-left for this variant.

Different from the bigger sprite-backed variants in two ways:

1. The hover ramp does not advance `res_index` — `Button::Tick`
   special-cases `B156x21` and only mutates `effectbrightness`
   (128 → 136). The chrome stays at idx 24 throughout.
2. Text vertical-offset is `yoff = 4` (vs `8` for the bigger
   variants), which centers the smaller label inside the 21-px
   tall button.

Label-position math:

```
xoff = (156 - strlen(text) * 8) / 2
yoff = 4
textX = button.x - 0 + xoff   = button.x + xoff
textY = button.y - 0 + 4      = button.y + 4
```

## `BNONE` constants

| Field | Value |
| ----- | ----- |
| Sprite bank | `0xFF` (none) |
| Base `res_index` | n/a |
| Width × height | **caller-set** (e.g. `40 × 30` for the option-controls "OR/AND" buttons) |
| Text font bank | **caller-set** (e.g. `134`) |
| Text advance | **caller-set** (e.g. `9`) |
| Text `yoff` | not in the table — see below |

`BNONE` is a text-only button: there's no sprite chrome at all, just
a hot-rect (defined by the caller-set `width × height` at
`object.x, object.y` raw — no anchor offset because there's no
sprite to derive offsets from) and centered text inside it.

In `Button::SetType(BNONE)` only `res_bank = 0xFF` is set; the
caller assigns `width`, `height`, `textbank`, `textwidth`, and
`text` directly on the object after construction. Hover animation
(state machine, brightness ramp) still runs but has no visual
effect because there's no sprite frame to advance.

Label position (from `Button::GetTextOffset`, `button.cpp:178`):

```
xoff = (width - strlen(text) * textwidth) / 2
yoff = 0    # the BNONE branch in GetTextOffset doesn't set yoff
textX = button.x + xoff           # no sprite offset to subtract
textY = button.y + 0              # ditto
```

So the text top-left lands at `(button.x + xoff, button.y)` —
**vertically anchored at the top** of the hot-rect, not centered.
Callers compensate by choosing a `button.y` that already places the
text where they want it (e.g. the options-controls OR/AND buttons
are placed at `y = 95 + i*53`, which happens to align text with the
neighboring `B112x33` row).

Hit-testing uses the raw bounds:

```
inside =
    button.x < mx < button.x + width
    AND
    button.y < my < button.y + height
```

(no anchor-offset shift, since `res_bank == 0xFF` means there's no
sprite to read offsets from).

## Other variants

`B220x33`, `B236x27`, `B52x21`, `B156x21`, `BCHECKBOX` are still
out of scope until a screen we hydrate uses them. See
`docs/design-system.md.archive` (`### Button` section) for the
legacy table; we will re-derive each row against `button.cpp` when
we add it.
