#!/usr/bin/env python3
"""Extract bitmap fonts from Silencer sprite banks into OpenType fonts.

Reads:
  shared/assets/BIN_SPR.DAT          bank header (sprite count per bank)
  shared/assets/bin_spr/SPR_NNN.BIN  sprite pixel data (RLE-encoded)
  shared/assets/PALETTE.BIN          palette 0 (used for AA opacity calc)

Writes to shared/fonts/:
  silencer-tiny.otf       bank 132, 4x5 monochrome (HUD digits)
  silencer-ui.otf         bank 133, 6x11 anti-aliased (main UI)
  silencer-ui-large.otf   bank 134, 9x13 anti-aliased (toggles/headers)
  silencer-title.otf      bank 136, 16x24 anti-aliased (titles)

The decoder mirrors Resources::LoadSprites in
clients/silencer/src/resources/resources.cpp. Output fonts are real
OpenType: monochrome glyf outlines + an SVG-in-OpenType table that
preserves the original anti-aliasing as fill-opacity. Glyphs use
currentColor, so CSS `color:` recolors text the same way the game's
Renderer::EffectRampColor re-tints it at draw time.

Opacity model
-------------
The Silencer palette is structured as 16-brightness ramps (see
docs/design/palette.md). For palette index `i >= 2`:

    ramp       = (i - 2) // 16
    brightness = (i - 2) %  16    (0..15)

Renderer::EffectRampColor preserves brightness while substituting a
new ramp, so a pixel's brightness IS its alpha-equivalent. We map

    opacity = brightness / max_brightness_used_in_bank

so the brightest pixel renders at 100% currentColor and edge pixels
render at proportional opacity. This is the property that lets HTML
`color:` re-tint glyphs the same way the in-game ramp tint does.
"""
import struct
import sys
from pathlib import Path

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib.tables.S_V_G_ import table_S_V_G_

FONTS_DIR = Path(__file__).resolve().parent.parent  # shared/fonts/
SHARED_DIR = FONTS_DIR.parent                        # shared/
ASSETS = SHARED_DIR / "assets"
OUT_DIR = FONTS_DIR

# bank: SPR_NNN.BIN bank index.
# ioffset: sprite index 0 = ASCII codepoint `ioffset` (matches Renderer::DrawText).
# advance: nominal advance width in font units (matches `width` arg to DrawText).
BANKS = [
    {"bank": 132, "filename": "silencer-tiny.otf",     "family": "Silencer Tiny",     "ioffset": 34, "advance": 4},
    {"bank": 133, "filename": "silencer-ui.otf",       "family": "Silencer UI",       "ioffset": 33, "advance": 6},
    {"bank": 134, "filename": "silencer-ui-large.otf", "family": "Silencer UI Large", "ioffset": 33, "advance": 9},
    {"bank": 136, "filename": "silencer-title.otf",    "family": "Silencer Title",    "ioffset": 33, "advance": 16},
]


def read_palette(palidx=0):
    with open(ASSETS / "PALETTE.BIN", "rb") as f:
        f.seek(palidx * (768 + 4) + 4)
        data = f.read(768)
    out = []
    for i in range(256):
        r, g, b = data[i*3], data[i*3+1], data[i*3+2]
        out.append((r << 2, g << 2, b << 2))
    return out


def get_sprite_counts():
    with open(ASSETS / "BIN_SPR.DAT", "rb") as f:
        data = f.read()
    return [data[i*64 + 2] for i in range(256)]


def decode_bank(bank_idx):
    counts = get_sprite_counts()
    n = counts[bank_idx]
    if n == 0:
        return []
    with open(ASSETS / f"bin_spr/SPR_{bank_idx:03d}.BIN", "rb") as f:
        raw = f.read()
    header_size = 344 * n + 4
    header = raw[:header_size]
    body = raw[header_size:]
    bpos = 0
    sprites = []
    for j in range(n):
        base = j * 344
        width  = struct.unpack_from("<H", header, base + 0)[0]
        height = struct.unpack_from("<H", header, base + 2)[0]
        offx   = struct.unpack_from("<h", header, base + 4)[0]
        offy   = struct.unpack_from("<h", header, base + 6)[0]
        size   = struct.unpack_from("<I", header, base + 12)[0]
        offsets_flag = header[base + 20]
        if offsets_flag:
            decompressed = bytearray(width * height)
            tempvalue = 0
            count = 0
            for y2 in range((height + 63) // 64):
                for x2 in range((width + 63) // 64):
                    ymax = min((y2 + 1) * 64, height)
                    xmax = min((x2 + 1) * 64, width)
                    for y in range(y2 * 64, ymax):
                        for x in range(x2 * 64, xmax, 4):
                            if count:
                                struct.pack_into("<I", decompressed, y*width + x, tempvalue)
                                count -= 4
                            else:
                                tempvalue = struct.unpack_from("<I", body, bpos)[0]
                                bpos += 4
                                if tempvalue >= 0xFF000000:
                                    count = tempvalue & 0xFFFF
                                    tempvalue &= 0x00FF0000
                                    tempvalue |= tempvalue << 8
                                    tempvalue |= tempvalue >> 16
                                    tempvalue &= 0xFFFFFFFF
                                    count -= 4
                                struct.pack_into("<I", decompressed, y*width + x, tempvalue)
        else:
            data = body[bpos:bpos + size]
            bpos += size
            decompressed = bytearray(width * height)
            k = 0
            j2 = 0
            n_words = size // 4
            while j2 < n_words:
                tempvalue = struct.unpack_from("<I", data, j2 * 4)[0]
                j2 += 1
                if tempvalue >= 0xFF000000:
                    count = tempvalue & 0xFFFF
                    tempvalue &= 0x00FF0000
                    tempvalue |= tempvalue << 8
                    tempvalue |= tempvalue >> 16
                    tempvalue &= 0xFFFFFFFF
                    while count > 0:
                        struct.pack_into("<I", decompressed, k * 4, tempvalue)
                        count -= 4
                        k += 1
                    k -= 1
                else:
                    struct.pack_into("<I", decompressed, k * 4, tempvalue)
                k += 1
        sprites.append((width, height, offx, offy, bytes(decompressed)))
    return sprites


def brightness_of(palette_index):
    """Brightness level (0..15) within a 16-color palette ramp.

    Indices 0 and 1 are special (transparent / pre-mapped black) and
    fall outside the ramp scheme; they're handled by callers.
    """
    return (palette_index - 2) % 16


def crop_glyph(width, height, pixels):
    """Tight bounding box. Returns (cropped_pixels, x_off, y_off, w, h)."""
    if not pixels or all(p == 0 for p in pixels):
        return (b"", 0, 0, 0, 0)
    min_x, min_y, max_x, max_y = width, height, -1, -1
    for y in range(height):
        for x in range(width):
            if pixels[y*width + x] != 0:
                if x < min_x: min_x = x
                if x > max_x: max_x = x
                if y < min_y: min_y = y
                if y > max_y: max_y = y
    new_w = max_x - min_x + 1
    new_h = max_y - min_y + 1
    out = bytearray(new_w * new_h)
    for y in range(new_h):
        for x in range(new_w):
            out[y*new_w + x] = pixels[(y + min_y) * width + (x + min_x)]
    return (bytes(out), min_x, min_y, new_w, new_h)


def make_svg(glyph_id, ox, oy, w, h, pixels, upem, pix, opacity_map):
    """Build the SVG-in-OpenType document for one glyph.

    The outer <g> uses currentColor for fill and a Y-flip transform so the
    inner content can use natural top-left = (0, 0) coordinates while the
    OpenType SVG viewport (Y-up, baseline at 0, top at upem) gets the
    bitmap rendered right-side up. Each bitmap pixel renders as a
    pix-by-pix square in font units (upem = native_em * pix).
    """
    rects = []
    for y in range(h):
        for x in range(w):
            p = pixels[y*w + x]
            if p == 0:
                continue
            op = opacity_map.get(p, 1.0)
            xx = (ox + x) * pix
            yy = (oy + y) * pix
            if op >= 0.999:
                rects.append(f'<rect x="{xx}" y="{yy}" width="{pix}" height="{pix}"/>')
            else:
                rects.append(f'<rect x="{xx}" y="{yy}" width="{pix}" height="{pix}" fill-opacity="{op:.3f}"/>')
    if not rects:
        return None
    body = "".join(rects)
    return (
        '<?xml version="1.0" encoding="UTF-8" standalone="no"?>'
        '<svg xmlns="http://www.w3.org/2000/svg" version="1.1">'
        f'<g id="glyph{glyph_id}" fill="currentColor" '
        f'transform="matrix(1 0 0 -1 0 {upem})">'
        f'{body}</g></svg>'
    )


def make_outline_glyph(ox, oy, w, h, pixels, upem, pix):
    """Monochrome outline fallback (one square per non-zero pixel).

    Used when the renderer doesn't support the SVG-in-OpenType table.
    Coordinates are in font units; each bitmap pixel = pix × pix.
    """
    pen = TTGlyphPen(None)
    for y in range(h):
        for x in range(w):
            if pixels[y*w + x] == 0:
                continue
            xl = (ox + x) * pix
            xr = xl + pix
            # OT Y-up: pixel row y from top -> top edge at upem - (oy+y)*pix.
            yt = upem - (oy + y) * pix
            yb = yt - pix
            pen.moveTo((xl, yb))
            pen.lineTo((xr, yb))
            pen.lineTo((xr, yt))
            pen.lineTo((xl, yt))
            pen.closePath()
    return pen.glyph()


def build_font(cfg):
    bank = cfg["bank"]
    sprites = decode_bank(bank)
    if not sprites:
        print(f"  bank {bank}: no sprites", file=sys.stderr)
        return

    # native_em: tallest bitmap row count in the bank (= the glyph cell height).
    # pix: how many font units per bitmap pixel. The OpenType spec / browser
    # OTS sanitizer requires unitsPerEm in [16, 16384], so we scale the
    # native pixel grid up by `pix` and set upem = native_em * pix.
    # CSS font-size: <native_em>px still renders 1 bitmap pixel per CSS pixel.
    native_em = max(h for (_, h, _, _, _) in sprites)
    pix = 64
    upem = native_em * pix

    used = set()
    for (_, _, _, _, px) in sprites:
        used.update(b for b in px if b != 0)
    # Brightness-per-ramp is the alpha-equivalent (see module docstring).
    if used:
        brightnesses = {i: brightness_of(i) for i in used}
        max_b = max(brightnesses.values()) or 1
        opacity_map = {i: brightnesses[i] / max_b for i in used}
    else:
        opacity_map = {}

    ioffset = cfg["ioffset"]
    advance_units = cfg["advance"] * pix

    glyph_order = [".notdef"]
    cmap = {}
    glyfs = {}
    metrics = {}
    svg_docs = []

    notdef_pen = TTGlyphPen(None)
    notdef_pen.moveTo((0, 0))
    notdef_pen.lineTo((advance_units, 0))
    notdef_pen.lineTo((advance_units, upem))
    notdef_pen.lineTo((0, upem))
    notdef_pen.closePath()
    glyfs[".notdef"] = notdef_pen.glyph()
    metrics[".notdef"] = (advance_units, 0)

    glyfs["space"] = TTGlyphPen(None).glyph()
    metrics["space"] = (advance_units, 0)
    glyph_order.append("space")
    cmap[0x20] = "space"
    cmap[0xA0] = "space"

    for sprite_idx, (w, h, _, _, pixels) in enumerate(sprites):
        codepoint = ioffset + sprite_idx
        if codepoint in (0x20, 0xA0):
            continue
        cropped, ox, oy, cw, ch = crop_glyph(w, h, pixels)
        gname = f"u{codepoint:04X}"
        glyph_order.append(gname)
        cmap[codepoint] = gname
        if cw == 0:
            glyfs[gname] = TTGlyphPen(None).glyph()
        else:
            glyfs[gname] = make_outline_glyph(ox, oy, cw, ch, cropped, upem, pix)
        metrics[gname] = (advance_units, 0)
        gid = len(glyph_order) - 1
        svg = make_svg(gid, ox, oy, cw, ch, cropped, upem, pix, opacity_map)
        if svg:
            svg_docs.append((svg, gid, gid))

    fb = FontBuilder(upem, isTTF=True)
    fb.setupGlyphOrder(glyph_order)
    fb.setupCharacterMap(cmap)
    fb.setupGlyf(glyfs)
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=upem, descent=0)
    fb.setupOS2(sTypoAscender=upem, sTypoDescender=0, usWinAscent=upem, usWinDescent=0)
    family = cfg["family"]
    fb.setupNameTable({
        "familyName": family,
        "styleName": "Regular",
        "psName": family.replace(" ", "") + "-Regular",
    })
    fb.setupPost()

    svg_table = table_S_V_G_()
    svg_table.docList = svg_docs
    fb.font["SVG "] = svg_table

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    out_path = OUT_DIR / cfg["filename"]
    fb.save(out_path)
    shades = len(set(round(o, 3) for o in opacity_map.values())) if opacity_map else 0
    print(f"  bank {bank}: wrote {out_path.name} "
          f"upem={upem} (native {native_em}px x{pix}) advance={advance_units} "
          f"glyphs={len(glyph_order)-1} svg={len(svg_docs)} shades={shades}")


def main():
    # Sub-palettes 0 (in-game), 1 (menu), and 2 (lobby) all share the same
    # RGB for the font ramp indices, so any of them gives the canonical
    # in-game color.
    palette = read_palette(0)
    r, g, b = palette[220]
    print(f"In-game default tint (palette idx 220): rgb({r},{g},{b}) #{r:02X}{g:02X}{b:02X}")
    for cfg in BANKS:
        build_font(cfg)
    print("Done.")


if __name__ == "__main__":
    main()
