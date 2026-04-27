# screen-lobby-gameselect — Lobby GameSelectInterface (right panel)

The right sub-interface in `screen-lobby` — Active Games list,
selected-game info display, Create Game / Join Game buttons. Bounding
box `(x=403, y=87, width=222, height=267)`.

Reference: `/tmp/real_lobby_dump.ppm`. This Ralph gates on the
GameSelectInterface region.

## Object inventory

| z | Object | Type | Bank | Index | x | y | Notes |
| - | --- | --- | --- | --- | --- | --- | --- |
| 0 | Right border    | overlay | 7 | 8 | 0 | 0 | The bank-7 idx-8 sprite — additional chrome for the game-list area on top of the LOBBY panel |
| 1 | "Active Games" label | overlay (font 134, w=8) | — | — | 405 | 70 | text=`Active Games` |
| 2 | SelectBox (game list) | selectbox | — | — | 407 | 89 | width=214, height=265, lineheight=14, uid=10. Empty when no Go lobby is running. |
| 3 | Scrollbar            | scrollbar | (engine 7?) | 9 | (engine-positioned, right edge of select box) | — | scrollpixels=14, scrollposition=0 |
| 4 | Selected-game name  | overlay (font 133, w=6) | — | — | 405 | 358 | uid=1, runtime |
| 5 | Selected-game map   | overlay (font 133, w=6) | — | — | 405 | 370 | uid=2, runtime |
| 6 | Selected-game players | overlay (font 133, w=6) | — | — | 405 | 382 | uid=3, runtime |
| 7 | Selected-game creator | overlay (font 133, w=6) | — | — | 405 | 394 | uid=4, runtime |
| 8 | Selected-game info  | overlay (font 133, w=6) | — | — | 405 | 406 | uid=5, runtime |
| 9 | Join Game button   | B156x21 | — | — | 436 | 430 | text=`Join Game`, uid=20 |
| 10| Create Game button | B156x21 | — | — | 242 | 68  | text=`Create Game`, uid=30 |

Note that **Create Game** sits at x=242 — that places it OUTSIDE the
GameSelectInterface bounding box (which starts at x=403). The button
visually appears at the top of the canonical dump centered above the
SelectBox. This is the engine's actual layout — the bbox is the data
region; the action buttons live above/below.

## What's runtime / non-structural

- The five selected-game info overlays are empty when no game is
  selected (always the case without a running lobby). Render empty.
- The SelectBox is empty without server data. Render the box outline
  / scrollable area; do not gate on rows of game text.
- The Scrollbar's thumb position: `scrollposition=0` so thumb is at
  the top of the track (same as the OPTIONSCONTROLS scrollbar — but
  in this case the scrollbar widget is a separate object, not baked
  into the panel chrome).

## SelectBox widget (new)

Multi-row selectable list. Engine fields: `x, y, width, height,
lineheight, uid, scrolled`. For an empty SelectBox the candidate just
renders the bounding-box outline (likely supplied by the bank-7 idx-8
sprite chrome).

**Spec gap:** `widget-selectbox.md` does not exist.

## Bank 7 idx 8 (right-border chrome)

A different sprite from the LOBBY panel idx 1. It supplies the
game-list area's inner chrome (left/right edges of the selectbox,
divider between selectbox and selected-game-info area, etc.). Renders
at `(0, 0)` like the panel — same anchor convention.

## Cross-references

- [`screen-lobby.md`](screen-lobby.md) — parent
- [`screen-options-controls.md`](screen-options-controls.md) — also uses ScrollBar (different bank but same widget concept)
- [`palette.md`](palette.md), [`sprite-banks.md`](sprite-banks.md), [`tick.md`](tick.md)
