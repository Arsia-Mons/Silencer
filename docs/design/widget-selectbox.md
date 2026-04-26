# `SelectBox` widget

**Source:** `clients/silencer/src/selectbox.h`,
`clients/silencer/src/selectbox.cpp`,
render dispatch in `clients/silencer/src/renderer.cpp:735..776`.

A scrollable single-selection list. Items are flush-left text
strings stored as a deque; the renderer draws as many as fit in
`height / lineheight` rows starting from `scrolled`. The selected
item gets a palette-180 highlight rectangle behind it.

The lobby uses one for the "Active Games" panel (`uid = 10`); the
options-controls map-picker would use one too if we hydrated that
sub-screen. On the lobby, the SelectBox is **empty** in the
default-config dump (no games returned by the lobby) — only the
chrome (the bank-7 idx-8 right-border panel that frames it) is
visible.

## Properties

| Field | Type | Default | Notes |
| ----- | ---- | ------- | ----- |
| `x`, `y` | i16 | `0`, `0` | Top-left of the scrolling area. |
| `width`, `height` | u16 | caller-set | Pixel bounds. |
| `lineheight` | u8 | caller-set (commonly 14) | Pixels per row. Visible rows = `height / lineheight`. |
| `scrolled` | u16 | `0` | First-visible item index. Wired to a `ScrollBar` via the parent Interface. |
| `selecteditem` | i32 | `-1` | Index of the highlighted item; `-1` means nothing selected. |
| `items` | `deque<char *>` | empty | Per-row text. `AddItem(text, id)` appends. |
| `itemids` | `deque<u32>` | empty | Optional opaque id per row. |
| `maxlines` | u16 | (caller-set) | Cap on retained items. |
| `uid` | u8 | caller-set | Discriminator for screen Tick code. |
| `enterpressed` | bool | `false` | Set by Interface key dispatch when the SelectBox has focus and Enter was hit. |
| `downloadprogress`, `downloaditem` | i32, char[256] | `-1`, empty | Map-picker–specific async download state; not used on the lobby. |

## Adding items

`AddItem(text, id = 0)` allocates a new char buffer, copies the
string in, and pushes it onto `items` (with `id` onto `itemids`).
There's no width-clipping at the data layer — the renderer truncates
visually if a row's pixels exceed `width`.

## Rendering — selection highlight + per-row text

Per `renderer.cpp:735..776`:

```
i = 0
for each item in items:
    if i < scrolled: skip
    row = i - scrolled
    y = selectbox.y + row * lineheight
    if y + lineheight > selectbox.y + selectbox.height: stop  # row would clip
    if i == selecteditem:
        DrawFilledRectangle(surface,
            selectbox.x, y,
            selectbox.x + selectbox.width, y + lineheight,
            180)                          # palette index 180 = mid-gray
    DrawText(surface, selectbox.x, y, item, 133, 6, false)  # main lobby uses bank 133, advance 6
    # selectbox uid 4 (map picker) does extra per-row drawing for the
    # download-status indicator — out of scope for the lobby SelectBox
    i += 1
```

So the selected row gets a `180` background under its text — a flat
mid-gray rectangle filling the row's pixel-width. The unselected
rows have no background; the underlying panel sprite shows through.

For the lobby SelectBox in its empty state, **no rows render at
all** — the `items` deque is empty.

## Hit-test

`MouseInside(world, mousex, mousey)` returns the row index under
the cursor (or `-1` if outside):

```
return mx in (x, x + width) and my in (y, y + height)
        ? scrolled + (my - y) / lineheight
        : -1
```

The screen's Tick code is responsible for translating that into
`selecteditem` updates.

## ScrollBar wiring

When the parent Interface has its `scrollbar` field set, the
ScrollBar's `scrollposition` mirrors `selectbox.scrolled` (and
vice versa via the per-tick logic in `Game::Tick`). For the
lobby SelectBox the ScrollBar's `draw` flag flips to `true` when
items exceed `height / lineheight` (per `game.cpp:4310`). Empty
SelectBox → `scrollbar.draw = false` → no scrollbar chrome
visible.

## How the lobby uses it

```
SelectBox * gameselect = ...
gameselect->x = 407;
gameselect->y = 89;
gameselect->width = 214;
gameselect->height = 265;
gameselect->lineheight = 14;
gameselect->uid = 10;
```

(From `Game::CreateGameSelectInterface`, `game.cpp:2950..2956`.)
Wired to a sibling `ScrollBar` (`res_index = 9`,
`scrollpixels = 14`). The screen's Tick adds rows when the lobby
sends MSG_NEWGAME messages; for an isolated dump (or a lobby with
no active games) the `items` deque stays empty.

## Spec gap noticed while authoring

The map-picker SelectBox (`uid = 4`) on the create-game screen
does extra per-row rendering (download progress, `[DL]` prefix
handling) — not covered here because the lobby's `uid = 10`
SelectBox doesn't use those code paths. When create-game lands
in scope this doc will gain a "map-picker variant" subsection.
