"""Extract XBASE15A's original geometry and an unlit tile reference."""

import json
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from sil_reference import ASSETS, ACTOR_NAMES, read_map, render_tiles

SOURCE = ASSETS / "XBASE15A.SIL"


def render_reference(manifest, raw):
    from PIL import Image, ImageDraw, ImageFont

    reference = render_tiles(manifest, raw)
    reference.convert("RGB").save(HERE / "source-tiles.png")
    cropped = reference.crop((384, 256, 2624, 1152)).convert("RGB")
    board = Image.new("RGB", (2240, 1190), "#0c1320")
    board.paste(cropped, (0, 120))
    draw = ImageDraw.Draw(board)
    title = ImageFont.load_default(size=35)
    body = ImageFont.load_default(size=21)
    small = ImageFont.load_default(size=17)
    draw.text((40, 25), "SILENCER / INTERDIMENSIONAL BASE", font=title, fill="#e4f3ff")
    draw.text((40, 75), "XBASE15A.SIL  /  ORIGINAL TILES + ACTOR ANCHORS  /  LIGHTING OFF",
              font=body, fill="#88adc1")
    for actor in manifest["actors"]:
        x, y = actor["x"] - 384, actor["y"] - 256 + 120
        # Credit and healing intentionally share an anchor in the source.
        shift = -23 if actor["id"] == 70 else (23 if actor["id"] == 57 else 0)
        draw.line((x, y, x + shift, y - 26), fill="#ffcf6c", width=2)
        x, y = x + shift, y - 26
        draw.ellipse((x - 14, y - 14, x + 14, y + 14), fill="#101820", outline="#ffcf6c", width=2)
        draw.text((x, y), str(actor["index"]), anchor="mm", font=small, fill="#fff0cf")
    used_ids = {actor["id"] for actor in manifest["actors"]}
    for row, actor_id in enumerate(sorted(used_ids)):
        name = ACTOR_NAMES[actor_id]
        column, line = row % 3, row // 3
        indices = ", ".join(str(a["index"]) for a in manifest["actors"] if a["id"] == actor_id)
        draw.text((40 + column * 735, 1040 + line * 38),
                  f"{indices}  {name}", font=body, fill="#c5dbea")
    board.save(HERE / "source-reference.png")


if __name__ == "__main__":
    manifest, raw = read_map(SOURCE)
    manifest.update(
        coordinate_transform={
            "x_m": "(source_x - 448) / 64",
            "z_m": "(1094 - source_y) / 64",
            "y_m": "invented depth; front -2, rear +2",
        },
        scale_note="One 64px tile = one design metre; not an original physical scale.",
        tile_reference_note="Lighting off: luminance-marked tiles, actor sprites, parallax "
                            "and runtime lighting are omitted. "
                            "Tile 0x0701 refers to empty bank 7 and is not drawn, "
                            "matching the engine's missing-surface behavior.",
    )
    render_reference(manifest, raw)
    (HERE / "source-map.json").write_text(json.dumps(manifest, indent=2) + "\n")
    counts = Counter(actor["name"] for actor in manifest["actors"])
    print(f"Decoded {len(manifest['platforms'])} platforms, {sum(counts.values())} actors: {dict(counts)}")
