# shared/assets/ — runtime assets

Binary, little-endian, 8-bit indexed. Don't try to read these as text.

- `BIN_SPR.DAT` + `bin_spr/SPR_NNN.BIN` — sprite banks (incl. font banks 132–136)
- `BIN_TIL.DAT` + `bin_til/TIL_NNN.BIN` — tile banks (level rendering)
- `PALETTE.BIN` — 11 sub-palettes, 256 colors × 6-bit RGB
- `actordefs/*.json` — actor definitions: per-animation sequences, per-frame
  hurtboxes, and per-frame sounds for guards, robots, civilians, and the
  player. Loaded by the C++ client on each map start (fetched from the admin
  API). Edit in the admin actor editor or directly as JSON — changes take
  effect without a client rebuild. See `clients/silencer/CLAUDE.md` §
  Actor definition system.
- `behaviortrees/*.json` — behavior tree definitions for NPCs (guard, robot,
  civilian). Loaded by the C++ BT interpreter at startup. Edit in the admin
  BT editor. See `clients/silencer/CLAUDE.md` § Behavior tree system.
- `level/` — built-in maps (`.SIL` format, 64 KiB max); `level/community/`
  holds community-uploaded maps.
- `sound.bin` — IMA ADPCM sound archive (98 usable sounds). Parsed by the
  C++ audio system; also pre-extracted to `web/admin/public/sounds/*.wav`
  for browser preview in the actor editor.

Format spec, RLE codec, and per-bank usage: `docs/design-system.md`
(§ Asset Formats, Appendix A — Sprite Bank Manifest). Loader: `src/resources.cpp`.
