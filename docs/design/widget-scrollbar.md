# `ScrollBar` widget

**Source:** `clients/silencer/src/scrollbar.h`,
`clients/silencer/src/scrollbar.cpp`,
render dispatch in `clients/silencer/src/renderer.cpp:431..436`
(track) and `709..733` (thumb).

A vertical scroll widget. **Optional rendering** — every screen
that uses one allocates a `ScrollBar` object and adds it to its
`Interface`, but the bar is only drawn when its `draw` field is
true. Screens that need scrolling logic without visible chrome
(like the options-controls keybinding list) leave `draw = false`.

## Properties

| Field | Type | Default | Notes |
| ----- | ---- | ------- | ----- |
| `res_bank` | u8 | `7` | Track sprite bank. |
| `res_index` | u8 | `9` | Track sprite frame. |
| `barres_index` | u8 | `10` | Thumb sprite frame within the same bank. |
| `draw` | bool | `false` | When false the widget renders nothing; only its scrollposition state is live. **The options-controls screen leaves this false.** |
| `scrollposition` | u16 | `0` | Current scroll offset (in items, not pixels). |
| `scrollmax` | u16 | `0` | Largest valid `scrollposition`. |
| `scrollpixels` | u8 | `0` | Pixels of vertical scroll motion per item — used by Interface to map mouse-wheel deltas onto `scrollposition`. |
| `x`, `y` | i16 | `0` | Position. Caller usually leaves these `0` and lets the Interface lay it out at the right edge. |

## What `scrollposition` means for the host screen

`ScrollBar::scrollposition` is a logical index, not a pixel offset.
Hosts read it to decide which subset of their full item list to
render. Example from options-controls
([screen-options-controls.md](screen-options-controls.md)): six
visible rows of the 20-item keybinding list, with row `i`
displaying the action at logical index `i + scrollbar.scrollposition`.

`scrollmax` should be set to `total_items - visible_items` so that
`scrollposition` can never push the visible window past the end.

## Hit-test regions (only relevant when `draw == true`)

The widget exposes three hit regions:

```
MouseInside(x, y):
    x1 = x - sprite.offset_x
    x2 = x1 + sprite.width
    y1 = y - sprite.offset_y
    y2 = y1 + sprite.height
    return mx in (x1, x2) and my in (y1, y2)

MouseInsideUp(x, y):  # the top 16-px arrow region
    same x bounds; y bounds = (y1, y1 + 16)

MouseInsideDown(x, y):  # the bottom 16-px arrow region
    same x bounds; y bounds = (y2 - 16, y2)
```

Clicking the up/down arrows calls `ScrollUp` / `ScrollDown`, which
just clamp `scrollposition` by ±1 against `0..scrollmax`. Mouse
wheel events (routed through the parent `Interface`) call the same
methods.

## Rendering — track (`draw == true`)

When the parent `Interface` iterates objects, the SCROLLBAR case at
`renderer.cpp:431..436` short-circuits if `!scrollbar.draw`,
otherwise the standard sprite-blit path renders the track sprite at
the object's anchor.

**For the screens currently covered by this spec, `draw` is always
false**, so the track-sprite path is never exercised. The minimal
faithful hydration of `ScrollBar` for those screens is just the
state fields (`scrollposition`, `scrollmax`, `scrollpixels`) and
the `ScrollUp`/`ScrollDown` methods.

## Rendering — thumb (`draw == true`)

Per `renderer.cpp:709..733`, the thumb is a clipped blit of the
sprite at `barres_index` (default 10): the renderer takes a
sub-rect of `srcrect.h = track.height - scrollmax` (clamped to a
minimum of 32 px), then offsets `dstrect.y` by
`(track.height - srcrect.h) * (scrollposition / scrollmax)` plus
a fixed `+16` (to clear the top arrow). The thumb's height
shrinks as `scrollmax` grows, suggesting "more items = smaller
thumb" — but the formula is per-track-pixel, not per-item.

When a screen using `draw = true` shows up (likely the lobby
chat or the file-list selectbox), this section will get a proper
worked example. For now the formula stands as documentation;
implementations don't need to exercise it for the menu subset.

## Wiring inside `Interface`

`Interface` keeps a single optional `scrollbar` field — the id of
the ScrollBar this Interface is managing. When set, mouse-wheel
events on the Interface route to the ScrollBar's
`ScrollUp`/`ScrollDown`. See
[widget-interface.md](widget-interface.md) for the dispatch path
and the related `objectupscroll` / `objectdownscroll` fields.
