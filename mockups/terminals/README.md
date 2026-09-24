# Terminal sprite sheets

Original terminal artwork at native resolution, using palette 0 and transparent
backgrounds. Frames are anchor-aligned, with 4px gutters and no labels baked
into the image. Read each row from left to right; frame numbers are zero-based.

| Sheet | Bank | Frames | Frame size | Rows |
| --- | --- | --- | --- | --- |
| [Small terminal](small-terminal.png) | 183 | 10 | 56 x 72px | Inactive 0-4; ready/hacking 5-9 |
| [Large terminal](large-terminal.png) | 184 | 23 | 120 x 90px | Inactive 0-4; beaming 5-13; ready/hacking 14-22 |

The small sheet is 5 columns by 2 rows; the large sheet is 9 columns by
3 rows, with four empty cells at the end of the first row. All original
bank frames are included. `sheets.json` records each frame's exact rectangle,
original sprite origin and source checksum.

The row groups follow `Terminal::SetSize` and `Terminal::Tick` in
`clients/silencer/src/stations/terminal.cpp`. They are frame groups, not
animation timing instructions. The small terminal reuses inactive frames
while beaming. Additional runtime effects, such as the hacking overlay
from bank 180, are not part of these terminal banks.

Regenerate from the **Arsia-Mons/Silencer** repository root using Python
with Pillow and the original `shared/assets/` binaries:

```bash
python3 mockups/generate_terminal_sheets.py
```
