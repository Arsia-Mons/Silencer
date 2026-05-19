# Character Creation Flow Reference

Source video:
`/Users/hv/.codex/discord-inbox/2026-05-18T17-05-59-440000+00-00-1505979780573692147/Screen_Recording_2026-05-17_185506.mp4`

## Step Screens

- `01_select-agent-existing-character.png`
- `02_select-agent-create-new.png`
- `03_alias-empty.png`
- `04_alias-typed.png`
- `05_select-agency-noxis.png`
- `06_select-agency-lazarus.png`
- `07_select-agency-static.png`
- `08_select-agency-blackrose.png`
- `character-creation-flow-contact.png`

## Background Sprite References

- Starfield/planet background: `shared/assets/bin_spr/SPR_006.BIN`, frame `0` (`640x480`).
- Character creation split-panel chrome: `shared/assets/bin_spr/SPR_007.BIN`, frame `5` (`628x441`).
- Options controls reference chrome: `shared/assets/bin_spr/SPR_007.BIN`, frame `7` (`628x454`), used only as a similar-UI reference.
- Bank 7 has `28` frames; see `sprites/bank007_28_frames_contact.png`.
- Rendered background pair: `sprites/background_sprite_refs.png`.
- Sprite index and palette inputs: `shared/assets/BIN_SPR.DAT`, `shared/assets/PALETTE.BIN`.

## Proof Capture

- Contiguous framebuffer walkthrough encoded with ffmpeg: `proof/character-creation-e2e-walkthrough.mp4`.
- Contact sheet for quick review: `proof/character-creation-e2e-contact-sheet.png`.
