# Lobby chrome rectangles — canonical coordinates and halo params

Reference table for the structural rectangles baked into the legacy
lobby BG sprite (bank 7, idx 1, decoded via palette block 2). Each row
is the **outer stroke rect** as visible on-screen, sampled from
`/tmp/lobby_bg.png` via the decoder at `/tmp/dump_lobby_bg.py`.

Halo palette indices are the **dominant** non-stroke neighbors at
offset ±1 from the primary stroke on the rectangle's "clean" edges —
edges that don't visibly abut another panel's chrome. Anomalies on
unclean edges are noted per-row. C2 will extend this table to all
remaining rectangles; C0d seeded it with the chat-box row after the
side-by-side parity DM landed.

## Conventions

- Coordinates are **outer stroke rect** in the 640×480 lobby BG image
  (top-left origin). They are NOT the panel-interface rect from the
  legacy `*_panel.cpp` files — that rect lives inside the stroke and
  doesn't include the chrome itself.
- Halo widths are 1 px each unless otherwise noted. Default canonical
  palette: `primary=216, outerHalo=75, innerHalo=77`. Per-rectangle
  variants override.

## Rectangles

| Name | x | y | w | h | primary | outer halo | inner halo | notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Chat-box outer frame | 10 | 195 | 378 | 260 | 216 | 75 (1 px) | 77 (1 px) | Right edge halo reads as outer=90/inner=66 instead of 75/77 — believed to be a sprite-adjacency artifact where the character panel's chrome touches the chat-box. The canonical pick is left+bottom edges (clean). |

## Sampling notes for future rows

The sampling script `/tmp/sample_chat_box_halo.py` is the template:
walk the column at the rect's left/right axis to find runs of idx
216, walk the row at the rect's top/bottom axis the same way, then
inspect offsets ±1..±4 from each edge's primary stroke. Aggregate
across all four edges. Cross-panel adjacency creates per-edge
variants — when this happens, prefer the edges that face open chrome
or background (palette idx 114 in the empty regions of the chat-box
context).
