---
name: image-comparison-review
description: Use when Codex must compare screenshots, video grabs, UI captures, sprite references, design mocks, or visual regression outputs. Before judging visual correctness, stitch reference and current images into one side-by-side composite, inspect that single composite, and report concrete mismatches in layout, scale, crop, text, color, focus state, sprites, and missing or extra elements.
---

# Image Comparison Review

## Rule

Do not rely on memory across separate image views when visual accuracy matters. Build one stitched comparison image first, then inspect that composite with `view_image` at original detail.

## Workflow

1. Put reference images and current images in two directories with matching filenames, or choose one explicit reference/current pair.
2. Match the viewport before capture whenever possible. For Silencer UI, use the CLI `resize --w W --h H` before `screenshot` when the reference came from a known video or screenshot size.
3. Run `scripts/stitch_compare.sh` to create a side-by-side sheet.
4. Open the sheet, not only the individual images.
5. Review row by row: viewport/crop, overall scale, panel/chrome shape, element positions, selected/hover/focus states, text content, wrapping, colors/brightness, sprite identity, and missing or extra UI.
6. Report the composite path and the highest-impact mismatches first.

## Script

```bash
# Directory mode: pairs files by identical basename.
bash shared/skills/image-comparison-review/scripts/stitch_compare.sh \
  --left docs/reference/character-creation-flow \
  --right docs/reference/character-creation-flow/proof/current-1280x800 \
  --out /tmp/character-create-compare.png \
  --left-label reference \
  --right-label current

# Single-pair mode.
bash shared/skills/image-comparison-review/scripts/stitch_compare.sh \
  --left ref.png \
  --right current.png \
  --out /tmp/ref-vs-current.png
```

The script uses ImageMagick `magick`, annotates each side, and appends matched pairs into one vertical comparison sheet.
