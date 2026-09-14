"""Compose the rendered cutaway and original map into a single artist sheet."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
board = Image.new("RGB", (2400, 2180), "#0c1320")
draw = ImageDraw.Draw(board)
title = ImageFont.load_default(size=49)
heading = ImageFont.load_default(size=29)
body = ImageFont.load_default(size=25)
small = ImageFont.load_default(size=21)

draw.text((65, 40), "SILENCER / INTERDIMENSIONAL BASE", font=title, fill="#e8f4ff")
draw.text((68, 104), "01  /  ARTIST CUTAWAY STUDY     |     XBASE15A.SIL     |     NOT FINAL GAME ART",
          font=body, fill="#82b7ce")
hero = Image.open(HERE / "01-cutaway.png").convert("RGB")
hero = hero.resize((2280, 1244), Image.Resampling.LANCZOS)
board.paste(hero, (60, 158))

draw.text((65, 1440), "ORIGINAL 2D TILE REFERENCE", font=heading, fill="#e8f4ff")
draw.text((65, 1482), "Unlit tiles; see source-reference.png for numbered equipment anchors.",
          font=small, fill="#82b7ce")
reference = Image.open(HERE / "source-tiles.png").crop((384, 256, 2624, 1152))
reference = reference.resize((1390, 556), Image.Resampling.LANCZOS)
board.paste(reference.convert("RGB"), (60, 1530))

draw.line((1495, 1450, 1495, 2100), fill="#2d4155", width=2)
draw.text((1540, 1440), "PRESERVE", font=heading, fill="#80def2")
for row, line in enumerate([
    "Room silhouette and platform heights.",
    "17 collision records; 20 actor anchors.",
    "Lower tech room, ladder and upper shelf.",
    "Support / inventory / return positions.",
]):
    draw.text((1540, 1490 + row * 36), line, font=body, fill="#d7e4ed")
draw.text((1540, 1680), "DESIGN INTERPRETATION", font=heading, fill="#d2b3ff")
for row, line in enumerate([
    "4m depth; open-front presentation.",
    "Graphite metal, cyan light, violet energy.",
    "All equipment shapes are placeholders.",
    "64px = 1 design metre; 1.8m scale figure.",
]):
    draw.text((1540, 1730 + row * 36), line, font=body, fill="#d7e4ed")
draw.text((1540, 1920), "ARTIST FILES", font=heading, fill="#e8f4ff")
draw.text((1540, 1970), ".blend  /  editable scene + cameras", font=body, fill="#d7e4ed")
draw.text((1540, 2010), ".glb  /  portable model + materials", font=body, fill="#d7e4ed")
draw.text((1540, 2050), "README.md  /  scale and fidelity notes", font=body, fill="#82b7ce")

draw.line((60, 2115, 2340, 2115), fill="#2d4155", width=2)
draw.text((65, 2138), "LAYOUT FIRST.  Preserve the side-on gameplay read; refine the dimensional architecture.",
          font=small, fill="#82b7ce")
board.save(HERE / "START-HERE.png")
print("Artist handout saved: START-HERE.png")
