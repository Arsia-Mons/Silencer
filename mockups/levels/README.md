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

`source-reference.png` adds the **actual game-object sprites**, map description,
numbered actor anchors and an actor-type legend. **Player spawn points have
cyan `SPAWN` labels and arrows**, not fake player sprites. Camera targets,
base exits, light positions and unknown actors have explicit source-only
markers where there is no drawable object.

The map remains at **1:1 scale**, inset 40px from the left and 150px from the
top. Zoom to 100% to read the smaller annotations. Leader lines point to the
original actor positions; labels are placed above/below sprites and shifted
to avoid overlapping each other.

Each folder also includes `source-map.json`, recording the source SHA-256,
dimensions, complete actor/platform records, sprite bank/frame selections,
signed sprite placement bounds, label locations, omitted light-tile counts
per layer and any missing tile references.
Numbers on the reference sheet are the **zero-based actor
indices** in this file, not actor type IDs.

As with the base, these are **lighting-off artist references**, not gameplay
screenshots. Every tile with a nonzero luminance byte is a light-pass tile
in the engine and is omitted, exposing the artwork beneath the grey gradients.
Non-light tiles on the same layers and in the same banks remain visible.
Parallax backgrounds and runtime lighting are omitted.
No source `.SIL` or runtime asset is modified.

## Object presentation

Object artwork is decoded from `BIN_SPR.DAT` and `bin_spr/SPR_*.BIN`, including
the tiled sprite format and signed origin offsets. Objects are composited
after background tiles and before foreground tiles, preserving foreground
occlusion. Spawn and source markers remain visible above the finished image.

NPCs use a representative standing/idle frame; stations and terminals use
their static artwork, with terminal/monitor sizes and doodad variants matched
to the map. Tech stations include their A/B/C panel variants. Pickups show
their **available-state** art and a `POWERUP` label even though the runtime
may initially hide them behind a respawn timer. The base billboard uses its
neutral backing, not an invented team affiliation.

These are static reference views, not a running simulation: there are no
animation frames advancing, live surveillance feeds, dynamic shadows or
runtime team/weapon palette effects. Lights keep their location markers but
do not add glow. `source-tiles.png` intentionally contains **no objects or
spawn annotations**, providing a separate clean architectural reference.

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
