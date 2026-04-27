# `Button` widget — `B196x33` (the only variant the main menu uses)

**Source:** `clients/silencer/src/button.h`,
`clients/silencer/src/button.cpp`,
render dispatch in `clients/silencer/src/renderer.cpp:699..705`.

The real engine has 7 button variants. The main menu only uses
**`B196x33`**. This doc covers that variant fully and notes where
the other variants will plug in when we extend the spec.

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

## Other variants — placeholder

`B112x33`, `B220x33`, `B236x27`, `B52x21`, `B156x21`, `BCHECKBOX`
are out of scope until a screen we hydrate uses them. See
`docs/design-system.md.archive` (`### Button` section) for the
legacy table; we will re-derive each row against `button.cpp` when
we add it.
