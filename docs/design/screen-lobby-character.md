# screen-lobby-character — Lobby CharacterInterface (left panel)

The left sub-interface in `screen-lobby` — player profile + 5 agency
toggle widgets. Bounding box `(x=10, y=64, width=217, height=120)`.

Reference: `/tmp/real_lobby_dump.ppm` (the same full-LOBBY dump used
by `screen-lobby.md`'s chrome gate). This Ralph gates only on the
CharacterInterface region of that dump.

## Object inventory (literal screen coords for overlays/toggles)

| z | Object | Type | Bank | Index | x | y | Notes |
| - | --- | --- | --- | --- | --- | --- | --- |
| 0 | Username text | overlay (font 134, w=8, color=200) | — | — | 20 | 71 | text=`<localusername>` (config-derived; on canonical dump shows the local user's saved name) |
| 1 | Level text  | overlay (font 133, w=7, color=129, brightness=128+32, ramp) | — | — | 17 | 130 | uid=2; runtime stat |
| 2 | Wins text   | overlay (font 133, w=7, color=129, brightness=128+32, ramp) | — | — | 17 | 143 | uid=3; runtime stat |
| 3 | Losses text | overlay (font 133, w=7, color=129, brightness=128+32, ramp) | — | — | 17 | 156 | uid=4; runtime stat |
| 4 | Etc text    | overlay (font 133, w=7, color=129, brightness=128+32, ramp) | — | — | 17 | 169 | uid=5; runtime stat |
| 5 | NOXIS toggle    | toggle | 181 | 0 | 20 + 0×42 = 20  | 90 | uid=1, set=1 |
| 6 | LAZARUS toggle  | toggle | 181 | 1 | 20 + 1×42 = 62  | 90 | uid=2, set=1 |
| 7 | CALIBER toggle  | toggle | 181 | 2 | 20 + 2×42 = 104 | 90 | uid=3, set=1 |
| 8 | STATIC toggle   | toggle | 181 | 3 | 20 + 3×42 = 146 | 90 | uid=4, set=1 |
| 9 | BLACKROSE toggle | toggle | 181 | 4 | 20 + 4×42 = 188 | 90 | uid=5, set=1 |

Stride between agency toggles: **+42 px x**.

## What's runtime / non-structural

- Username text content (varies per local config). Render *whatever* the
  candidate's local username is or use a placeholder; the structural
  gate is "username overlay rendered in the right place", not the
  exact text.
- Level / Wins / Losses / Etc text content (server-driven; empty without
  a running lobby). Render empty or with placeholder; gate is overlay
  position only.
- Toggle "selected" state (one toggle is highlighted to indicate the
  user's `defaultagency` from `Config`). The reference dump may show
  any of NOXIS / LAZARUS / CALIBER / STATIC / BLACKROSE selected; the
  candidate may pick any default — visual difference between selected
  and unselected toggles is non-gated.

## Toggle widget (new — bank 181)

A small icon-style toggle button. Sprite-bank 181 contains 5 sprites
(idx 0–4) for the 5 agencies. Each sprite is the agency emblem.
`set=1` means the toggle is enabled (normal-colored); `set=0` would
mean disabled (greyed out — not present on this screen).

When `selected=true`, the toggle is rendered with a brighter or
distinct treatment (probably a second sprite frame or a brightness
shift). For the structural gate, render the base sprite without
worrying about the selected highlight — that's a visual nit.

**Spec gap:** `widget-toggle.md` does not exist. Author one when this
sub-interface is filled in.

## Cross-references

- [`screen-lobby.md`](screen-lobby.md) — parent
- [`palette.md`](palette.md), [`sprite-banks.md`](sprite-banks.md), [`tick.md`](tick.md)
