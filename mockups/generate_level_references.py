"""Generate reference pairs for original levels, explicitly excluding community."""

import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from sil_reference import ASSETS, read_map, render_tiles

OUTPUT = Path(__file__).resolve().parent / "levels"
HEADER = 150
MARGIN = 40


def render_reference(manifest, tiles):
    actors = manifest["actors"]
    types = sorted({actor["id"] for actor in actors})
    width, height = tiles.size
    footer_height = 100 + math.ceil(len(types) / 3) * 90
    board = Image.new("RGB", (width + 2 * MARGIN, height + HEADER + footer_height), "#0c1320")
    board.paste(tiles.convert("RGB"), (MARGIN, HEADER))
    draw = ImageDraw.Draw(board)
    title = ImageFont.load_default(size=36)
    body = ImageFont.load_default(size=23)
    small = ImageFont.load_default(size=20)
    source_name = Path(manifest["source"]).name
    draw.text((MARGIN, 24), f"SILENCER / {source_name}", font=title, fill="#e4f3ff")
    draw.text((MARGIN, 76), manifest["description"], font=body, fill="#c5dbea")
    draw.text((MARGIN, 112),
              f"ORIGINAL TILES + ACTOR ANCHORS  /  {width} x {height}px  /  "
              f"{len(actors)} ACTORS  /  UNLIT REFERENCE, NOT A GAME SCREENSHOT",
              font=small, fill="#88adc1")

    placed = []
    marker_positions = []
    for actor in actors:
        x, y = actor["x"] + MARGIN, actor["y"] + HEADER
        if not (MARGIN <= x < MARGIN + width and HEADER <= y < HEADER + height):
            raise ValueError(f"{source_name}: actor {actor['index']} lies outside the map")
        candidates = [(0, -32), (45, -32), (-45, -32), (0, 38), (45, 38),
                      (-45, 38), (85, -32), (-85, -32), (0, -80), (0, 85)]
        for dx, dy in candidates:
            mx, my = x + dx, y + dy
            if not (22 <= mx < board.width - 22 and HEADER + 22 <= my < HEADER + height - 22):
                continue
            if all(abs(mx - px) >= 44 or abs(my - py) >= 44 for px, py in placed):
                break
        else:
            raise ValueError(f"{source_name}: cannot place a readable label for actor {actor['index']}")
        placed.append((mx, my))
        marker_positions.append({"index": actor["index"], "label_center_px": [mx, my]})
        draw.line((x, y, mx, my), fill="#ffcf6c", width=2)
        draw.ellipse((x - 3, y - 3, x + 3, y + 3), fill="#ffcf6c")
        draw.ellipse((mx - 20, my - 20, mx + 20, my + 20),
                     fill="#101820", outline="#ffcf6c", width=2)
        draw.text((mx, my), str(actor["index"]), anchor="mm", font=small, fill="#fff0cf")

    top = HEADER + height + 24
    draw.text((MARGIN, top), "ACTOR INDEX / labels point to the encoded actor positions; numbers are zero-based.",
              font=small, fill="#88adc1")
    column_width = width // 3
    for row, actor_id in enumerate(types):
        column, line = row % 3, row // 3
        matches = [actor for actor in actors if actor["id"] == actor_id]
        x, y = MARGIN + column * column_width, top + 48 + line * 90
        draw.text((x, y), f"{matches[0]['name']}  (ID {actor_id})", font=body, fill="#e4f3ff")
        indices = ", ".join(str(actor["index"]) for actor in matches)
        if draw.textlength(indices, font=small) > column_width - 30:
            raise ValueError(f"{source_name}: actor legend exceeds column width")
        draw.text((x, y + 33), indices, font=small, fill="#ffcf6c")
    manifest["reference_image"] = {
        "map_origin_px": [MARGIN, HEADER],
        "scale": 1,
        "markers": marker_positions,
        "note": "Original tile layers only; actor sprites, parallax and runtime luminosity omitted.",
    }
    return board


def main():
    sources = sorted((ASSETS / "level").glob("*.SIL"))
    if not sources:
        raise FileNotFoundError("No top-level .SIL files found in shared/assets/level")
    for source in sources:
        manifest, raw = read_map(source)
        tiles = render_tiles(manifest, raw)
        reference = render_reference(manifest, tiles)
        destination = OUTPUT / source.stem
        destination.mkdir(parents=True, exist_ok=True)
        tiles.convert("RGB").save(destination / "source-tiles.png")
        reference.save(destination / "source-reference.png")
        (destination / "source-map.json").write_text(json.dumps(manifest, indent=2) + "\n")
        print(f"{source.name}: {tiles.width} x {tiles.height}px, {len(manifest['actors'])} actor anchors")
    print(f"Saved {len(sources)} reference pairs under {OUTPUT}; community excluded.")


if __name__ == "__main__":
    main()
