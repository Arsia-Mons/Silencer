"""Extract XBASE15A's original geometry and an unlit tile reference."""

import hashlib
import json
import struct
import zlib
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
ASSETS = HERE.parents[1] / "shared" / "assets"
SOURCE = ASSETS / "XBASE15A.SIL"
ACTOR_NAMES = {
    50: "Surveillance monitor",
    56: "Inventory station",
    57: "Heal machine",
    58: "Secret return",
    65: "Base exit",
    66: "Tech station",
    67: "Wall defense",
    68: "Team billboard",
    70: "Credit machine",
}


def read_map():
    data = SOURCE.read_bytes()
    width, height = struct.unpack_from(">HH", data, 4)
    minimap_size = struct.unpack_from("<I", data, 145)[0]
    level_size = struct.unpack_from("<I", data, 149 + minimap_size)[0]
    start = 153 + minimap_size
    if start + level_size != len(data):
        raise ValueError("Unexpected XBASE15A file length")
    raw = zlib.decompress(data[start:])
    offset = width * height * 36
    actor_count = struct.unpack_from("<I", raw, offset)[0]
    offset += 8
    actors = []
    fields = ("id", "x", "y", "direction", "type", "matchid",
              "subplane", "unknown", "securityid")
    for index in range(actor_count):
        actor = dict(zip(fields, struct.unpack_from("<4Ii4I", raw, offset)))
        actor.update(index=index, name=ACTOR_NAMES[actor["id"]])
        actors.append(actor)
        offset += 36
    platform_count = struct.unpack_from("<I", raw, offset)[0]
    offset += 8
    platforms = []
    for index in range(platform_count):
        values = struct.unpack_from("<6i", raw, offset)
        platform = dict(zip(("x1", "y1", "x2", "y2", "type1", "type2"), values))
        kind = {(0, 0): "rectangle", (1, 0): "ladder"}[values[4:]]
        platform.update(index=index, kind=kind)
        platforms.append(platform)
        offset += 24
    if offset != len(raw):
        raise ValueError("Unexpected trailing sections in XBASE15A")
    manifest = {
        "source": "shared/assets/XBASE15A.SIL",
        "sha256": hashlib.sha256(data).hexdigest(),
        "width_tiles": width,
        "height_tiles": height,
        "tile_size_px": 64,
        "coordinate_transform": {
            "x_m": "(source_x - 448) / 64",
            "z_m": "(1094 - source_y) / 64",
            "y_m": "invented depth; front -2, rear +2",
        },
        "scale_note": "One 64px tile = one design metre; not an original physical scale.",
        "tile_reference_note": "Unlit tile composite, without sprites or parallax. "
                               "Tile 0x0701 refers to empty bank 7 and is not drawn, "
                               "matching the engine's missing-surface behavior.",
        "actors": actors,
        "platforms": platforms,
    }
    return manifest, raw


def tile_images(bank, palette):
    from PIL import Image

    count = (ASSETS / "BIN_TIL.DAT").read_bytes()[bank * 64 + 2]
    data = (ASSETS / "bin_til" / f"TIL_{bank:03d}.BIN").read_bytes()
    pixels = bytearray()
    for (word,) in struct.iter_unpack("<I", data[12 * count + 4:]):
        if word >= 0xFF000000:
            pixels.extend(bytes([(word >> 16) & 255]) * (word & 65535))
        else:
            pixels.extend(struct.pack("<I", word))
    if len(pixels) != count * 4096:
        raise ValueError(f"Invalid decoded size for tile bank {bank}")
    images = []
    for i in range(count):
        indices = bytes(pixels[i * 4096:(i + 1) * 4096])
        image = Image.frombytes("P", (64, 64), indices)
        image.putpalette(palette)
        image.info["transparency"] = 0
        images.append(image.convert("RGBA"))
    return images


def render_reference(manifest, raw):
    from PIL import Image, ImageDraw, ImageFont

    width, height = manifest["width_tiles"], manifest["height_tiles"]
    palette = bytes((value << 2) & 255
                    for value in (ASSETS / "PALETTE.BIN").read_bytes()[4:772])
    banks = {}
    reference = Image.new("RGBA", (width * 64, height * 64), "#070b14")
    # Background layers, then foreground layers; actors are numbered anchors,
    # not sprite reconstructions. Luminosity and parallax are deliberately omitted.
    for layer_offset in (0, 4, 8, 12, 20, 24, 28, 32):
        for i in range(width * height):
            tile, flip, _lum = struct.unpack_from("<HBB", raw, i * 36 + layer_offset)
            if not tile or tile == 0x0701:
                continue
            bank, frame = tile >> 8, tile & 255
            if bank not in banks:
                banks[bank] = tile_images(bank, palette)
            image = banks[bank][frame]
            if flip:
                image = image.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            reference.alpha_composite(image, (i % width * 64, i // width * 64))
    reference.convert("RGB").save(HERE / "source-tiles.png")
    cropped = reference.crop((384, 256, 2624, 1152)).convert("RGB")
    board = Image.new("RGB", (2240, 1190), "#0c1320")
    board.paste(cropped, (0, 120))
    draw = ImageDraw.Draw(board)
    title = ImageFont.load_default(size=35)
    body = ImageFont.load_default(size=21)
    small = ImageFont.load_default(size=17)
    draw.text((40, 25), "SILENCER / INTERDIMENSIONAL BASE", font=title, fill="#e4f3ff")
    draw.text((40, 75), "XBASE15A.SIL  /  ORIGINAL TILES + ACTOR ANCHORS  /  NOT A GAME SCREENSHOT",
              font=body, fill="#88adc1")
    for actor in manifest["actors"]:
        x, y = actor["x"] - 384, actor["y"] - 256 + 120
        # Credit and healing intentionally share an anchor in the source.
        shift = -23 if actor["id"] == 70 else (23 if actor["id"] == 57 else 0)
        draw.line((x, y, x + shift, y - 26), fill="#ffcf6c", width=2)
        x, y = x + shift, y - 26
        draw.ellipse((x - 14, y - 14, x + 14, y + 14), fill="#101820", outline="#ffcf6c", width=2)
        draw.text((x, y), str(actor["index"]), anchor="mm", font=small, fill="#fff0cf")
    for row, (actor_id, name) in enumerate(ACTOR_NAMES.items()):
        column, line = row % 3, row // 3
        indices = ", ".join(str(a["index"]) for a in manifest["actors"] if a["id"] == actor_id)
        draw.text((40 + column * 735, 1040 + line * 38),
                  f"{indices}  {name}", font=body, fill="#c5dbea")
    board.save(HERE / "source-reference.png")


if __name__ == "__main__":
    manifest, raw = read_map()
    render_reference(manifest, raw)
    (HERE / "source-map.json").write_text(json.dumps(manifest, indent=2) + "\n")
    counts = Counter(actor["name"] for actor in manifest["actors"])
    print(f"Decoded {len(manifest['platforms'])} platforms, {sum(counts.values())} actors: {dict(counts)}")
