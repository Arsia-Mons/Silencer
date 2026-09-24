"""Export both terminal sprite banks as transparent, anchor-aligned sheets."""

import hashlib
import json
from pathlib import Path

from PIL import Image

from actor_reference import sprite_bank
from sil_reference import ASSETS

OUTPUT = Path(__file__).resolve().parent / "terminals"
GUTTER = 4
TERMINALS = (
    ("small-terminal", 183, (("inactive", 0, 5), ("ready-hacking", 5, 10))),
    ("large-terminal", 184, (("inactive", 0, 5), ("beaming", 5, 14), ("ready-hacking", 14, 23))),
)


def main():
    palette_file = ASSETS / "PALETTE.BIN"
    palette = bytes((value << 2) & 255 for value in palette_file.read_bytes()[4:772])
    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = {
        "palette": 0,
        "palette_sha256": hashlib.sha256(palette_file.read_bytes()).hexdigest(),
        "gutter_px": GUTTER,
        "scale": 1,
        "sheets": [],
    }
    for name, bank, sequences in TERMINALS:
        frames = sprite_bank(bank, palette)
        expected = [index for _, start, end in sequences for index in range(start, end)]
        if expected != list(range(len(frames))):
            raise ValueError(f"{name}: animation ranges do not cover the complete sprite bank")
        left = min(-ox for _, ox, _ in frames)
        top = min(-oy for _, _, oy in frames)
        right = max(image.width - ox for image, ox, _ in frames)
        bottom = max(image.height - oy for image, _, oy in frames)
        cell_width, cell_height = right - left, bottom - top
        columns = max(end - start for _, start, end in sequences)
        sheet = Image.new("RGBA", (GUTTER + columns * (cell_width + GUTTER),
                                  GUTTER + len(sequences) * (cell_height + GUTTER)))
        entries = []
        for row, (state, start, end) in enumerate(sequences):
            for column, index in enumerate(range(start, end)):
                image, ox, oy = frames[index]
                x = GUTTER + column * (cell_width + GUTTER) - ox - left
                y = GUTTER + row * (cell_height + GUTTER) - oy - top
                sheet.paste(image, (x, y))
                entries.append({"index": index, "state": state, "row": row, "column": column,
                                "rect": [x, y, image.width, image.height], "origin": [ox, oy]})
        filename = f"{name}.png"
        sheet.save(OUTPUT / filename)
        source = ASSETS / "bin_spr" / f"SPR_{bank:03d}.BIN"
        manifest["sheets"].append({
            "file": filename,
            "source": f"shared/assets/bin_spr/{source.name}",
            "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "bank": bank,
            "size_px": list(sheet.size),
            "cell_size_px": [cell_width, cell_height],
            "cell_origin_px": [-left, -top],
            "columns": columns,
            "rows": len(sequences),
            "frames": entries,
        })
        print(f"{filename}: {len(frames)} frames, {sheet.width} x {sheet.height}px")
    (OUTPUT / "sheets.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
