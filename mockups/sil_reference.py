"""Source map decoding and unlit tile compositing shared by artist references."""

import hashlib
import struct
import warnings
import zlib
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "shared" / "assets"
ACTOR_NAMES = {
    0: "Guard blaster",
    1: "Civilian",
    2: "Captain laser",
    3: "Trooper rocket",
    6: "Robot",
    36: "Player start",
    37: "Camera",
    47: "Doodad",
    50: "Surveillance monitor",
    54: "Terminal",
    56: "Inventory station",
    57: "Heal machine",
    58: "Secret return",
    61: "Warper",
    63: "Powerup",
    64: "Vent",
    65: "Base exit",
    66: "Tech station",
    67: "Wall defense",
    68: "Team billboard",
    69: "Computer",
    70: "Credit machine",
    71: "Light",
    72: "Magistrate",
    73: "Vanta",
}
PLATFORM_NAMES = {
    (0, 0): "rectangle",
    (1, 0): "ladder",
    (0, 1): "stairs up",
    (0, 2): "stairs down",
    (2, 0): "track",
    (3, 0): "outside room",
    (3, 1): "specific room",
}


def read_map(source):
    data = source.read_bytes()
    width, height = struct.unpack_from(">HH", data, 4)
    if not (0 < width <= 256 and 0 < height <= 256):
        raise ValueError(f"{source.name}: invalid map dimensions {width} x {height}")
    minimap_size = struct.unpack_from("<I", data, 145)[0]
    level_size = struct.unpack_from("<I", data, 149 + minimap_size)[0]
    start = 153 + minimap_size
    if start + level_size != len(data):
        raise ValueError(f"{source.name}: unexpected file length")
    raw = zlib.decompress(data[start:])
    offset = width * height * 36
    actor_count = struct.unpack_from("<I", raw, offset)[0]
    offset += 8
    actors = []
    fields = ("id", "x", "y", "direction", "type", "matchid",
              "subplane", "unknown", "securityid")
    unknown_actors = set()
    for index in range(actor_count):
        actor = dict(zip(fields, struct.unpack_from("<4Ii4I", raw, offset)))
        actor_id = actor["id"]
        name = ACTOR_NAMES.get(actor_id, f"Unknown actor ID {actor_id}")
        if actor_id not in ACTOR_NAMES:
            unknown_actors.add(actor_id)
        actor.update(index=index, name=name)
        actors.append(actor)
        offset += 36
    if unknown_actors:
        warnings.warn(f"{source.name}: retaining unrecognized actor IDs {sorted(unknown_actors)}")
    platform_count = struct.unpack_from("<I", raw, offset)[0]
    offset += 8
    platforms = []
    for index in range(platform_count):
        values = struct.unpack_from("<6i", raw, offset)
        platform = dict(zip(("x1", "y1", "x2", "y2", "type1", "type2"), values))
        kind = PLATFORM_NAMES.get(values[4:])
        if kind is None:
            kind = f"unrecognized ({values[4]}, {values[5]})"
            warnings.warn(f"{source.name}: retaining {kind} platform {index}")
        platform.update(index=index, kind=kind)
        platforms.append(platform)
        offset += 24
    if offset != len(raw):
        raise ValueError(f"{source.name}: trailing sections unsupported by this reference decoder")
    manifest = {
        "source": source.relative_to(ROOT).as_posix(),
        "sha256": hashlib.sha256(data).hexdigest(),
        "description": data[17:145].split(b"\0")[0].decode("utf-8"),
        "width_tiles": width,
        "height_tiles": height,
        "tile_size_px": 64,
        "actors": actors,
        "platforms": platforms,
    }
    return manifest, raw


def tile_images(bank, palette, count):
    from PIL import Image

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


def render_tiles(manifest, raw):
    from PIL import Image

    width, height = manifest["width_tiles"], manifest["height_tiles"]
    palette = bytes((value << 2) & 255
                    for value in (ASSETS / "PALETTE.BIN").read_bytes()[4:772])
    counts = (ASSETS / "BIN_TIL.DAT").read_bytes()
    banks, missing = {}, Counter()
    reference = Image.new("RGBA", (width * 64, height * 64), "#070b14")
    # Actors are annotated separately. Parallax and runtime luminosity are
    # deliberately omitted so the reference exposes the original tile artwork.
    for layer_offset in (0, 4, 8, 12, 20, 24, 28, 32):
        for i in range(width * height):
            tile, flip, _lum = struct.unpack_from("<HBB", raw, i * 36 + layer_offset)
            if not tile:
                continue
            bank, frame = tile >> 8, tile & 255
            count = counts[bank * 64 + 2]
            if count == 0:
                missing[tile] += 1
                continue
            if frame >= count:
                raise ValueError(f"{manifest['source']}: tile 0x{tile:04X} exceeds bank frame count")
            if bank not in banks:
                banks[bank] = tile_images(bank, palette, count)
            image = banks[bank][frame]
            if flip:
                image = image.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            reference.alpha_composite(image, (i % width * 64, i // width * 64))
    manifest["undrawn_tiles"] = [
        {"tile_id": f"0x{tile:04X}", "count": count, "reason": "empty bank in BIN_TIL.DAT"}
        for tile, count in sorted(missing.items())
    ]
    if missing:
        warnings.warn(f"{manifest['source']}: not drawing tiles in empty banks: "
                      + ", ".join(f"0x{tile:04X} ({count} references)" for tile, count in sorted(missing.items())))
    return reference
