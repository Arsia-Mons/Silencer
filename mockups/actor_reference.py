"""Static game-object sprites and explicit source markers for artist references."""

import struct

from PIL import Image, ImageDraw, ImageFont

from sil_reference import ASSETS, decode_pixels

STATIC_SPRITES = {
    0: (59, 0), 1: (121, 0), 2: (59, 0), 3: (59, 0), 6: (47, 0),
    56: (89, 0), 57: (172, 0), 58: (152, 0), 61: (85, 0),
    64: (179, 0), 67: (112, 0), 69: (171, 0), 70: (80, 0),
}
MARKER_ONLY = {36: "SPAWN", 37: "CAMERA", 60: "UNKNOWN", 65: "EXIT", 71: "LIGHT"}


def sprite_bank(bank, palette):
    count = (ASSETS / "BIN_SPR.DAT").read_bytes()[bank * 64 + 2]
    if not count:
        raise ValueError(f"Missing sprite bank {bank}")
    data = (ASSETS / "bin_spr" / f"SPR_{bank:03d}.BIN").read_bytes()
    offset = count * 344 + 4
    frames = []
    for index in range(count):
        header = index * 344
        width, height, ox, oy = struct.unpack_from("<HHhh", data, header)
        size = struct.unpack_from("<I", data, header + 12)[0]
        tiled = data[header + 20] != 0
        start = offset
        pixels, offset = decode_pixels(data, offset, width * height)
        if tiled:
            # Tile-mode sizes in sprite headers are unreliable. Advance by the
            # decoded pixel count, then scatter 64x64 blocks into image rows.
            linear = bytearray(width * height)
            cursor = 0
            for top in range(0, height, 64):
                for left in range(0, width, 64):
                    span = min(64, width - left)
                    for y in range(top, min(top + 64, height)):
                        linear[y * width + left:y * width + left + span] = pixels[cursor:cursor + span]
                        cursor += span
            pixels = bytes(linear)
        elif offset != start + size:
            raise ValueError(f"Sprite {bank}:{index} compressed size mismatch")
        image = Image.frombytes("P", (width, height), pixels)
        image.putpalette(palette)
        image.info["transparency"] = 0
        frames.append((image.convert("RGBA"), ox, oy))
    return frames


def actor_parts(actor):
    kind, variant = actor["id"], actor["type"]
    if kind in MARKER_ONLY:
        return []
    if kind in STATIC_SPRITES:
        return [STATIC_SPRITES[kind]]
    if kind == 47 and 0 <= variant <= 9:
        return [(49 + variant, 0)]
    if kind == 50:
        return [(65, {4: 0, 5: 0, 6: 1, 7: 2}[variant])]
    if kind == 54:
        return [(184 if variant else 183, 0)]
    if kind == 63 and 0 <= variant <= 6:
        # Available-state pickup artwork; respawn timers are not simulated.
        return [(200 if variant == 0 else 201 if variant == 2 else 205, 0)]
    if kind == 66 and 0 <= variant <= 2:
        return [(106, 0), (106, 1), (106, 17 + variant * 4)]
    if kind == 68:
        # Neutral billboard: background, two animated parts and frame. The SIL
        # does not choose an agency, so no faction name or live feed is invented.
        return [(151, 1), (151, 7), (151, 14), (151, 0)]
    raise ValueError(f"Unmapped source actor {kind}, variant {variant}")


def draw_actor_sprites(image, manifest, palette):
    banks, entries = {}, []
    for actor in manifest["actors"]:
        parts = []
        for bank, frame in actor_parts(actor):
            if bank not in banks:
                banks[bank] = sprite_bank(bank, palette)
            sprite, ox, oy = banks[bank][frame]
            mirrored = actor["id"] in (0, 2, 3) and actor["direction"] != 0
            x = actor["x"] - (sprite.width - ox if mirrored else ox)
            y = actor["y"] - oy
            if mirrored:
                sprite = sprite.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            image.alpha_composite(sprite, (x, y))
            parts.append({"bank": bank, "frame": frame, "x": x, "y": y,
                          "width": sprite.width, "height": sprite.height, "mirrored": mirrored})
        entry = {"index": actor["index"], "parts": parts}
        if parts:
            entry["bounds"] = [min(p["x"] for p in parts), min(p["y"] for p in parts),
                               max(p["x"] + p["width"] for p in parts),
                               max(p["y"] + p["height"] for p in parts)]
        else:
            entry["marker"] = MARKER_ONLY[actor["id"]]
        entries.append(entry)
    manifest["actor_sprites"] = {
        "state": "Representative idle object frames; powerups shown available; neutral palette 0.",
        "compositing": "After background tiles and before foreground tiles, at original signed sprite anchors.",
        "limitations": "No animation, live surveillance feeds, runtime shadows or team/weapon palette effects.",
        "rendered_actors": sum(bool(entry["parts"]) for entry in entries),
        "entries": entries,
    }


def annotate_actors(board, manifest, origin):
    draw = ImageDraw.Draw(board)
    font = ImageFont.load_default(size=19)
    entries = {entry["index"]: entry for entry in manifest["actor_sprites"]["entries"]}
    placed, markers = [], []
    ox, oy = origin
    map_bottom = oy + manifest["height_tiles"] * 64
    for actor in manifest["actors"]:
        entry = entries[actor["index"]]
        x, y = actor["x"] + ox, actor["y"] + oy
        tag = MARKER_ONLY.get(actor["id"], "POWERUP" if actor["id"] == 63 else "")
        label = f"{tag} {actor['index']}" if tag else str(actor["index"])
        color = "#79e8ff" if actor["id"] == 36 else "#ffcf6c"
        half_width = max(20, int(draw.textlength(label, font=font) / 2) + 10)
        above = min(-32, entry["bounds"][1] - actor["y"] - 24) if entry["parts"] else -32
        below = max(38, entry["bounds"][3] - actor["y"] + 24) if entry["parts"] else 38
        candidates = [(dx, dy) for dy in (above, below, above - 52, below + 52)
                      for dx in (0, half_width * 2 + 8, -half_width * 2 - 8, 2 * (half_width * 2 + 8))]
        for dx, dy in candidates:
            mx, my = x + dx, y + dy
            bounds = (mx - half_width, my - 20, mx + half_width, my + 20)
            if not (bounds[0] >= 2 and bounds[2] < board.width - 2
                    and bounds[1] >= max(0, oy) + 2 and bounds[3] < min(board.height, map_bottom) - 2):
                continue
            if all(bounds[2] + 4 <= p[0] or bounds[0] >= p[2] + 4
                   or bounds[3] + 4 <= p[1] or bounds[1] >= p[3] + 4 for p in placed):
                break
        else:
            raise ValueError(f"{manifest['source']}: no readable label position for actor {actor['index']}")
        placed.append(bounds)
        markers.append({"index": actor["index"], "label": label, "label_center_px": [mx, my]})
        draw.line((x, y, mx, my), fill=color, width=2)
        if actor["id"] == 36:
            draw.polygon(((x, y), (x - 9, y - 13), (x + 9, y - 13)), fill=color)
            draw.line((x - 12, y + 3, x + 12, y + 3), fill=color, width=3)
        else:
            draw.ellipse((x - 3, y - 3, x + 3, y + 3), fill=color)
        draw.rounded_rectangle(bounds, radius=10 if tag else 20,
                               fill="#101820", outline=color, width=2)
        draw.text((mx, my), label, anchor="mm", font=font, fill=color)
    return markers
