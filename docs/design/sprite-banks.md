# Sprite banks

**Source:** `clients/silencer/src/resources.cpp` (loader),
`clients/silencer/src/sprite.cpp` (RLE codec),
`shared/assets/BIN_SPR.DAT` (index), `shared/assets/bin_spr/SPR_NNN.BIN`
(per-bank pixel data).

## Index file: `BIN_SPR.DAT`

A single byte per bank, 256 banks total → file is exactly **256
bytes**. Byte `N` is the number of sprites (frames) in bank `N`.
A zero byte means the bank is unused; no `SPR_NNN.BIN` exists for it.

```
sprite_count[bank]  =  BIN_SPR.DAT[bank]
```

## Per-bank file: `bin_spr/SPR_NNN.BIN`

`NNN` is the bank number padded to 3 digits (`SPR_006.BIN`,
`SPR_208.BIN`, etc.). File layout:

```
[ header[0]            ]  ──┐
[ header[1]            ]    │   count = sprite_count[bank] entries,
[ ...                  ]    │   each header is exactly 344 bytes
[ header[c-1]          ]  ──┘
[ 4 bytes filler       ]              ← read past but unused
[ pixel data for sprite 0  ]
[ pixel data for sprite 1  ]
[ ...                      ]
```

The real client reads `(344 * count) + 4` bytes for the header
section in one shot (`clients/silencer/src/resources.cpp:56`). The
trailing 4 bytes are silent padding, and pixel data for sprite 0
starts at file offset `(344 * count) + 4` — *not* `344 * count`.
Forgetting the `+4` shifts every subsequent sprite's read by 4
bytes and produces near-correct but corrupted decodes (especially
visible on the menu's 640×480 background plate).

### Header (344 bytes, little-endian)

| Offset | Type   | Field        | Meaning |
| ------ | ------ | ------------ | ------- |
| 0      | u16    | `w`          | Sprite width in pixels |
| 2      | u16    | `h`          | Sprite height in pixels |
| 4      | i16    | `offset_x`   | Anchor offset (signed; see below) |
| 6      | i16    | `offset_y`   | Anchor offset (signed; see below) |
| 8..11  | —      | (reserved)   | Currently unused by the menu codepath |
| 12     | u32    | `comp_size`  | Bytes of pixel data **for mode-0 sprites only**; tile-mode sprites overstate or understate this value (see RLE codec below) |
| 16..19 | —      | (reserved)   | |
| 20     | u8     | `mode`       | RLE encoding mode. `0` = linear; any non-zero value = tile mode |
| 21..343 | —     | (reserved)   | Padding / fields not used by the menu |

The trailing ~320 bytes carry per-sprite metadata used elsewhere
(palette tint, ramp tables, surveillance offsets); for the menu
subset they can be ignored.

## Anchor convention (the part that surprises everyone)

`offset_x` and `offset_y` are **subtracted** from the object's
position when blitting. The sprite's top-left ends up at
`(object.x - offset_x, object.y - offset_y)`.

```
top_left_x = object.x - sprite.offset_x
top_left_y = object.y - sprite.offset_y
```

Because the offsets are signed, they typically encode **anchor
points to the right or below the sprite's top-left**, expressed as
*negative* numbers. Example:

| Bank | Idx | w   | h  | offset_x | offset_y | Notes |
| ---- | --- | --- | -- | -------- | -------- | ----- |
| 6    | 0   | 640 | 480 | 0       | 0        | Full-screen menu background, top-left anchored |
| 6    | 7   | 196 | 33  | -310    | -288     | `B196x33` button base frame |
| 208  | 60  | 348 | 31  | -7      | -222     | Main-menu logo, steady-state frame |

For the `B196x33` button at `object.x = 40, object.y = -134`, the
sprite's top-left lands at `(40 - (-310), -134 - (-288)) = (350, 154)`,
i.e. right side of the screen, well above center. This is how the
menu's right-aligned buttons are positioned without any explicit
"right-align" math — the offset is baked into the sprite header.

## Blit (the only blit the menu needs)

```
for sy in 0..sprite.h:
    for sx in 0..sprite.w:
        p = sprite.pixels[sy * sprite.w + sx]
        if p == 0: continue           # 0 is transparent
        out = tint_lut ? tint_lut[p] : p
        framebuffer[top_left_y + sy][top_left_x + sx] = out
```

`tint_lut` is a 256-byte mapping built from the active sub-palette
(see [palette.md](palette.md)). The button's hover effect uses a
brightness-only LUT; backgrounds and the logo blit with the
identity LUT (`tint_lut[i] = i`).

## RLE codec

Each sprite's pixel data is a stream of 32-bit dwords (little-endian
on disk, but logically native after read). The decoder branches on
the high byte:

```
read u32 dword
if (dword & 0xFF000000) == 0xFF000000:
    # run marker
    run_bytes = dword & 0x0000FFFF                # always a multiple of 4
    pixel = (dword >> 16) & 0xFF                  # 1-byte palette index
    emit `pixel` `run_bytes` times
else:
    # literal: 4 raw pixels
    emit dword[0], dword[1], dword[2], dword[3]
```

The `mode` byte (header offset 20) selects how those emitted bytes
are arranged into the sprite's final w×h buffer:

| `mode` | Output arrangement |
| ------ | ------------------ |
| `0`    | Linear: emitted bytes are written row-major directly. |
| any non-zero (4, 5, 6, 80, …) | Tile: bytes flow in 64×64 tile order — outer iteration over tile rows then tile columns; inner iteration over rows within a tile, then 4-pixel-wide chunks across the tile. Partial edge tiles use the actual `w` and `h` remainder. |

The real client reads dwords inline during the tile traversal
(`clients/silencer/src/resources.cpp:74..106`); a pre-pass that
decompresses the whole stream into a linear buffer and then
re-distributes by tile order produces the same output and is
easier to test.

### `comp_size` is unreliable for tile mode

The header's `comp_size` is reliable only for `mode == 0` sprites.
For tile-mode sprites the field exists but does not always match
the bytes the RLE+tile traversal actually consumes. Two strategies
work:

1. **Match the real client.** Read dwords inline during tile
   iteration; advance the file pointer naturally. `comp_size` is
   never consulted.
2. **Pre-pass with output-size termination.** When pre-decoding into
   a linear buffer, stop as soon as the buffer reaches `w * h`
   bytes (rather than after `comp_size` bytes). Track how many
   source bytes were consumed and use *that* to advance to the
   next sprite.

A naive "consume `comp_size` bytes per sprite" approach
miscalculates the start of subsequent sprites in the same bank
once a tile-mode sprite is encountered.

## Banks the main menu touches

| Bank | Why |
| ---- | --- |
| 6    | Menu background plate (`idx 0`) and `B196x33` button frames (`idx 7..11`) |
| 7    | (Not used by main menu, but commonly co-loaded; safe to skip for menu-only hydrations) |
| 132–136 | Font banks — see [font.md](font.md). The version text overlay uses bank 133 |
| 208  | Animated game-title logo (`idx 29..60`) |

A hydration can stop after loading these four (132+133 plus 6 and
208) and still render the entire main menu.
