# Lobby chrome rectangles — canonical coordinates and halo params

Reference table for the structural rectangles baked into the legacy
lobby BG sprite (bank 7, idx 1, decoded via palette block 2). Each row
is the **outer stroke rect** as visible on-screen, sampled from
`/tmp/lobby_bg.png` via the decoder at `/tmp/dump_lobby_bg.py`.

Halo palette indices are the **dominant** non-stroke neighbors at
offset ±1 from the primary stroke on the rectangle's "clean" edges —
edges that don't visibly abut another panel's chrome. Anomalies on
unclean edges are noted per-row.

## Conventions

- Coordinates are **outer stroke rect** in the 640×480 lobby BG image
  (top-left origin). They are NOT the panel-interface rect from the
  legacy `*_panel.cpp` files — that rect lives inside the stroke and
  doesn't include the chrome itself.
- Halo widths are 1 px each unless otherwise noted. Default canonical
  palette: `primary=216, outerHalo=75, innerHalo=77`. Per-rectangle
  variants override.
- Detection: `/tmp/find_all_rects.py` scans the decoded BG sprite for
  horizontal + vertical runs of idx 216 (`MIN_RUN=20`) and assembles
  closed rectangles by corner-matching the runs. Halo sampling:
  `/tmp/sample_all_halos.py` (template for future panels).

## Rectangles

| Name | x | y | w | h | primary | outer halo | inner halo | notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Top frame strip | 10 | 25 | 620 | 29 | 216 | 75 (1 px) | 75 (1 px) | Thin wide strip between the top film-strip sphere band and the panel row. Inner-halo distribution is 75=509, 77=344, 74=286 — 75 dominates both sides. Functionally a "title-bar" / status strip across the top of the lobby. |
| Top-left title bar / status panel | 10 | 64 | 218 | 121 | 216 | 75 (1 px) | 77 (1 px) | Clean closed rectangle. The legacy decorative texture inside (circuit-board art) is dropped in the Clay chrome — interior becomes flat color. |
| Chat-box outer frame | 10 | 195 | 378 | 260 | 216 | 75 (1 px) | 77 (1 px) | Right edge halo reads as outer=90/inner=66 instead of 75/77 — believed to be a sprite-adjacency artifact where the right-pane chrome touches the chat-box. Canonical pick is left+bottom edges (clean). |
| Right pane — upper extension | 238 | 64 | 160 | 121 | 216 | 75 (1 px) | 77 (1 px) | Half of the upside-down-L. See "Right pane topology" below. |
| Right pane — tall right section | 398 | 64 | 232 | 391 | 216 | 75 (1 px) | 77 (1 px) | The other half of the upside-down-L. Bottom edge halo at 75=381/74=214 — left/bottom edges clean, top abuts the upper extension (internal joint). |

### Right pane topology (the upside-down-L)

The right pane is an upside-down-L (or "step") shape. Its perimeter
in the BG sprite traces:

```
(238, 64) ────────────────────────────────── (629, 64)
   │                                              │
   │     (upper extension)                        │
   │                                              │
(238, 184)──(398, 184)                            │
                │                                 │
                │       (lower / tall section)    │
                │                                 │
            (398, 454) ────────────────────── (629, 454)
```

Detected run evidence (from `/tmp/find_all_rects.py`):

- Top edge: y=64, x=[238..629] (one continuous run)
- Right edge: x=629, y=[64..454] (one continuous run)
- Bottom edge: y=454, x=[398..629]
- Bottom of upper extension: y=184, x=[238..397]
- Left of lower section: x=398, y=[185..454]
- Left of upper extension: x=238, y=[64..184]

Decomposes naturally into two rectangles abutting at column x=398:

1. **Upper extension**: `(238, 64, 160, 121)` — covers the wide upper
   area to the right of the title bar, ending above the chat-box's
   right shoulder.
2. **Tall right section**: `(398, 64, 232, 391)` — the main
   game-info / player-list pane, full height.

### Recommendation for the L-shape (C5/C6 reference)

**Compose two `Box`es side-by-side.** The shared edge (x=398, y=64..184)
is internal — both Boxes own their own stroke at that column, so the
joint visually reads as a single thicker stroke band along (397..398),
which **matches the legacy sprite** (which similarly carries stroke
pixels on both sides of the L-bend joint).

Concretely: emit `Box{x=238, y=64, w=160, h=121}` for the upper
extension AND `Box{x=398, y=64, w=232, h=391}` for the main right
pane. Both with canonical halo params (`primary=216, outer=75,
inner=77`).

This is preferable to:

- **Polygon primitive.** Would need a new primitive that can stroke
  arbitrary closed paths. Overkill for one L-shape; the Box primitive
  is sufficient via composition.
- **Reworking the right pane to a single rectangle.** Changes the
  lobby layout's visual identity; outside the chrome milestone's
  scope.

The two-Box composition lets each sub-region be its own flex
container, which actually helps C6 (the right pane has variant
subtrees — game list, player list, etc. — that naturally inhabit
the lower section while the upper extension is fixed chrome).

## Notes on what's NOT a rectangle

Items visible in `/tmp/lobby_bg.png` that are NOT chrome rectangles:

- **Film-strip sphere lights** at the top and bottom of the BG.
  Deferred per RALPH.md milestone rules (not in scope).
- **Decorative interior textures** (circuit boards, planet monitor,
  photo collage). These live inside the rectangles in the legacy BG;
  the Clay chrome intentionally drops them in favor of flat fills
  with optional opacity.
- **Character panel area**. The area bounded by the title bar
  bottom (y≈184), the right-pane left edge (x≈398), the chat-box
  top (y≈195), and the outer left (x≈10) has **no green stroke
  in the BG sprite** — there is no character-panel rectangle to
  replicate. The character widgets render as overlay UI on top of
  the BG, not framed by chrome.

## Sampling notes for future rows

The detection script `/tmp/find_all_rects.py` plus halo sampler
`/tmp/sample_all_halos.py` are the template. Walk runs of idx 216,
match corners, then sample ±1 offsets. Cross-panel adjacency
creates per-edge variants — when this happens, prefer the edges
that face open chrome or background (palette idx 114 in the empty
regions of the chat-box context). The Box primitive's defaults
already encode `(216, 75, 77)`; per-rectangle variants need only
override the deltas.
