# Original level source references

Each level directly in `shared/assets/level/` has its own folder here.
**The `community/` subfolder is excluded.** These are source-reference
images, not new 3D mockups.

| Level | Original tiles | Annotated source reference | Tile image size |
| --- | --- | --- | --- |
| CRAN01h | [source-tiles.png](CRAN01h/source-tiles.png) | [source-reference.png](CRAN01h/source-reference.png) | 6336 x 2944 |
| EASY05c | [source-tiles.png](EASY05c/source-tiles.png) | [source-reference.png](EASY05c/source-reference.png) | 2176 x 1536 |
| PIT16d | [source-tiles.png](PIT16d/source-tiles.png) | [source-reference.png](PIT16d/source-reference.png) | 4480 x 3136 |
| STAR72 | [source-tiles.png](STAR72/source-tiles.png) | [source-reference.png](STAR72/source-reference.png) | 7744 x 2624 |
| THET06e | [source-tiles.png](THET06e/source-tiles.png) | [source-reference.png](THET06e/source-reference.png) | 2944 x 1920 |

`source-tiles.png` is the complete, uncropped map at its original resolution,
with **lighting off**. All non-light tiles from the four background and four
foreground layers are composited using palette 0. It uses the same tile
renderer as the interdimensional base.

`source-reference.png` adds the map description, numbered actor anchors and
an actor-type legend. The map remains at **1:1 scale**, inset 40px from the
left and 150px from the top. Zoom to 100% to read the smaller annotations.
Leader lines point to the original actor positions; labels are shifted to
avoid overlapping each other.

Each folder also includes `source-map.json`, recording the source SHA-256,
dimensions, complete actor/platform records, label locations, omitted
light-tile counts per layer and any missing tile references.
Numbers on the reference sheet are the **zero-based actor
indices** in this file, not actor type IDs.

As with the base, these are **lighting-off tile references**, not gameplay
screenshots. Every tile with a nonzero luminance byte is a light-pass tile
in the engine and is omitted, exposing the artwork beneath the grey gradients.
Non-light tiles on the same layers and in the same banks remain visible.
Actor sprites, parallax backgrounds and runtime lighting are also omitted.
No source `.SIL` or runtime asset is modified.

## Original-data notes

Actor ID 60 occurs in CRAN01h, PIT16d and STAR72 but has no definition in the
current engine map loader or designer actor catalogue. Its anchors and raw
records are preserved and explicitly labeled `Unknown actor ID 60`.

STAR72 references tile bank 98 three times (`0x6214` twice and `0x6215` once),
but `BIN_TIL.DAT` marks that bank empty. These three references also carry
the luminance flag and are excluded with the other light tiles.
STAR72 also contains one unrecognized `(3, 2)` platform record, preserved
without guessing its meaning. Platform geometry is not painted over the tiles.

## Regenerate

From the repository root, using Python with Pillow:

```bash
python3 mockups/generate_level_references.py
```

This overwrites the generated PNG pairs and source manifests for the
non-recursive `shared/assets/level/*.SIL` selection. It does not regenerate
the base's 3D scene or traverse `community/`.
