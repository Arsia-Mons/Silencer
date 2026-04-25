# data/ — runtime assets

Binary, little-endian, 8-bit indexed. Don't try to read these as text.

- `BIN_SPR.DAT` + `bin_spr/SPR_NNN.BIN` — sprite banks (incl. font banks 132–136)
- `BIN_TIL.DAT` + `bin_til/TIL_NNN.BIN` — tile banks (level rendering)
- `PALETTE.BIN` — 11 sub-palettes, 256 colors × 6-bit RGB

Format spec, RLE codec, and per-bank usage: `docs/design-system.md`
(§ Asset Formats, Appendix A — Sprite Bank Manifest). Loader: `src/resources.cpp`.
