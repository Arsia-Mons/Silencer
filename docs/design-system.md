# zSILENCER Design System

A reference of the existing UI design specifications extracted from the C++/SDL2 source code.
This document serves as the foundation for building a component library and evaluating
future rendering migrations.

> **Note:** zSILENCER uses an 8-bit indexed-color palette (256 entries) rendered to a
> fixed **640 × 480** internal surface. The window is resizable and supports fullscreen,
> but all UI coordinates and measurements below are in logical pixels at 640 × 480.

---

## Table of Contents

1. [Typography](#typography)
2. [Color System](#color-system)
3. [Asset Formats](#asset-formats) — BIN_SPR.DAT, SPR_NNN.BIN, RLE codec, font banks, transparency
4. [Animation Timing & Tick Rate](#animation-timing--tick-rate) — 1 tick = 42 ms, ~24 Hz simulation
5. [Components](#components)
   - [Reusable Widgets vs. Screen Compositions](#reusable-widgets-vs-screen-compositions)
   - [Shared Base: Sprite Properties](#shared-base-sprite-properties)
   - [Button](#button) — 7 variants, state machine, animation, hit-testing
   - [Toggle](#toggle) — agency icon & checkbox modes, radio groups, label
   - [TextInput](#textinput) — caret, scrolling, password, intentionally limited editing
   - [TextBox](#textbox) — multi-line text, auto-scroll, word wrap
   - [SelectBox](#selectbox) — single-selection list, palette-180 highlight
   - [ScrollBar](#scrollbar) — up/down arrows, scroll track
   - [Overlay](#overlay) — sprite/text label, animations, custom pixel buffer
   - [Interface](#interface-container--focus-manager) — container, focus/tab, keyboard/mouse routing, focus visuals
   - [9-Slice Panel](#9-slice-panel-chat-message-background) — horizontal-stretch chat header/footer
   - [Modal Dialog](#modal-dialog) — centered alert/confirm with optional OK button
   - [Loading Bar](#loading-bar) — progress indicator
   - [In-Game Chat Overlay](#in-game-chat-overlay) — messages + input
   - [Announcement Message](#announcement-message-drawmessage) — per-character animation
   - [Status Messages](#status-messages-drawstatus) — kill/event stack
   - [Top Message](#top-message-drawtopmessage) — scrolling objective text
   - [Player List](#player-list-drawplayerlist) — scoreboard overlay
   - [Buy Menu](#buy-menu) — purchasable items interface
   - [HUD Bars](#hud-bars) — health, shield, fuel, files, ammo, inventory
   - [Team HUD](#team-hud) — team icons and player status
   - [Minimap](#minimap) — 172 × 62 map overview
   - [Drawing Primitives](#drawing-primitives) — rectangle, line, circle, checkered
6. [Layout & Spacing](#layout--spacing)
7. [Visual Effects](#visual-effects)
8. [UI Sounds](#ui-sounds)
9. [Effect-Color Reference](#effect-color-reference)

---

## Typography

All text is rendered from bitmap glyph sprite banks. There are no TrueType or vector
fonts — each "font" is just a regular [sprite bank](#asset-formats) containing one
glyph per printable ASCII character (starting at ASCII 33 `!`, or 34 `"` for bank 132).
Glyph N in font bank B is sprite index `(asciiCode - asciiOffset)`. Space (`0x20`) is
not stored; the renderer just advances the cursor by `fontwidth`. See
[Asset Formats → Font Banks](#font-banks-132136) for the full encoding.

### Font Banks

| Bank | Name | Glyph Height\* | ASCII Offset | Typical Use |
| ---- | ----------- | -------------- | ------------ | --------------------------------- |
| 132 | Tiny | ~5 px | 34 | HUD counters (ammo, health nums) |
| 133 | Small | ~11 px | 33 | Labels, metadata, chat, debug |
| 134 | Medium | ~15 px | 33 | Form labels, item names, prices |
| 135 | Large | ~19 px | 33 | Button text, headings, titles |
| 136 | Extra-Large | ~23 px | 33 | XP overlays, win/loss headlines |

\*Glyph heights are the hit-test heights used in `Overlay::MouseInside()` (`overlay.cpp`).
Actual pixel heights depend on the sprite data.

### Font Width (Advance)

The `width` parameter on `DrawText()` is a **fixed advance** — every character occupies
exactly `width` pixels horizontally, regardless of glyph shape. This is a monospaced grid.

| Bank | Width | Context |
| ---- | ----- | ------------------------------------------ |
| 132 | 4 | `DrawTinyText()` — HUD numbers |
| 133 | 6 | Chat, player names, small labels |
| 133 | 7 | Debug text, status messages, map labels |
| 133 | 11 | Version number display |
| 134 | 8 | Standard form labels, option text |
| 134 | 9 | Buy-menu item names, wider labels |
| 134 | 10 | Player name input, deploy messages |
| 135 | 11 | Button labels (all button types) |
| 135 | 12 | Config-screen titles |
| 135 | 13 | Win/loss second-line text |
| 136 | 15 | XP gain overlay |
| 136 | 25 | Win/loss first-line headline |

### Rendering Functions

| Function | Source | Notes |
| ---------------------- | --------------------- | ----------------------------------------------- |
| `DrawText()` | `renderer.cpp:1443` | Core glyph renderer (bank, width, color, alpha). **Quirk:** if both `color == 0` *and* `brightness == 0`, the renderer skips all effect processing — color tinting included. Default `brightness = 128` avoids this; only matters if a port explicitly passes `brightness = 0`. |
| `DrawTinyText()` | `renderer.cpp:1514` | Convenience — uses bank 132, width 4, centered |
| `DrawTextInput()` | `renderer.cpp:1490` | Renders input field text with caret |

### Text Centering

- **Horizontal center:** `x = (640 - (charCount * width)) / 2`
- **Vertical center on button:** `yoff = 8` (large buttons), `yoff = 4` (B156×21)
- **DrawTinyText:** auto-centers on the given x coordinate; special-cases `'1'` (-1 px)

---

## Color System

### Palette Architecture

- **File:** `data/PALETTE.BIN` (8,448 bytes)
- **Format:** 4-byte file-level header (reserved, all zeros) followed by 11 sub-palettes
  of 768 bytes each. Each sub-palette is 256 colors × 3 bytes (R, G, B). Total =
  `4 + 11 × 768 = 8,448 bytes`. Sub-palette N starts at offset `4 + N × 768`.
- **Color depth:** 6 bits per channel. Raw file values are 0–63; loader shifts each
  byte left by 2 (`v << 2`) to expand to 8-bit (0–252 in steps of 4).
- **Lookup tables** (`PALETTECALC{n}.BIN`): pre-computed 256 × 256 tables for brightness,
  color-tint, and alpha-blend transformations; auto-calculated and cached if missing

### Palette Index Ranges (Palette 0)

| Range | Purpose |
| --------- | ------------------------------------------------------------------- |
| 0–1 | Transparent / black — protected, never transformed |
| 2–113 | Main color ramps: 7 groups × 16 brightness levels each |
| 114–225 | Upper palette — mirrors ramp structure for effect/tint rendering |
| 226–255 | Parallax sky colors — dynamic, swapped from palettes 5–9 per map |

### Color Ramp Groups (indices 2–113)

Each group is 16 consecutive palette entries forming a brightness ramp from
darkest (level 0) to brightest (level 15).

```
index = (colorGroup * 16) + brightnessLevel + 2
```

- `colorGroup` = `(index - 2) / 16` (0–6)
- `brightnessLevel` = `(index - 2) % 16` (0 = darkest, 15 = brightest)

**Group 0 — Gray (indices 2–17)**

| Index | Level | R | G | B | Hex |
| ----- | ----- | --- | --- | --- | ------- |
| 2 | 0 | 0 | 0 | 0 | `#000000` |
| 5 | 3 | 48 | 48 | 48 | `#303030` |
| 10 | 8 | 132 | 132 | 132 | `#848484` |
| 14 | 12 | 200 | 200 | 200 | `#C8C8C8` |
| 17 | 15 | 252 | 252 | 252 | `#FCFCFC` |

**Group 1 — Yellow / Fire (indices 18–33)**

Note: Group 1 starts at saturated red (`#FC0000`) and ramps up through orange
to pure yellow (`#FCFC00`) and finally pale yellow (`#FCFCD4`). Group 4 below
is a separate, darker orange ramp. Code referring to "fire" colors usually
means the lower (red-orange) end of group 1; code referring to "yellow" means
the upper end.

| Index | Level | R | G | B | Hex |
| ----- | ----- | --- | --- | --- | ------- |
| 18 | 0 | 252 | 0 | 0 | `#FC0000` |
| 22 | 4 | 252 | 96 | 0 | `#FC6000` |
| 26 | 8 | 252 | 200 | 0 | `#FCC800` |
| 28 | 10 | 252 | 252 | 0 | `#FCFC00` |
| 33 | 15 | 252 | 252 | 212 | `#FCFCD4` |

**Group 2 — Red (indices 34–49)**

| Index | Level | R | G | B | Hex |
| ----- | ----- | --- | --- | --- | ------- |
| 34 | 0 | 12 | 4 | 4 | `#0C0404` |
| 38 | 4 | 92 | 28 | 28 | `#5C1C1C` |
| 42 | 8 | 172 | 24 | 24 | `#AC1818` |
| 46 | 12 | 252 | 0 | 0 | `#FC0000` |
| 49 | 15 | 252 | 80 | 80 | `#FC5050` |

**Group 3 — Brown/Tan (indices 50–65)**

| Index | Level | R | G | B | Hex |
| ----- | ----- | --- | --- | --- | ------- |
| 50 | 0 | 40 | 12 | 0 | `#280C00` |
| 54 | 4 | 96 | 48 | 16 | `#603010` |
| 58 | 8 | 152 | 100 | 60 | `#98643C` |
| 62 | 12 | 208 | 164 | 132 | `#D0A484` |
| 65 | 15 | 252 | 224 | 200 | `#FCE0C8` |

**Group 4 — Orange (indices 66–81)**

| Index | Level | R | G | B | Hex |
| ----- | ----- | --- | --- | --- | ------- |
| 66 | 0 | 40 | 4 | 0 | `#280400` |
| 70 | 4 | 104 | 36 | 4 | `#682404` |
| 74 | 8 | 168 | 84 | 28 | `#A8541C` |
| 77 | 11 | 216 | 136 | 52 | `#D88834` |
| 81 | 15 | 252 | 220 | 180 | `#FCDCB4` |

**Group 5 — Blue (indices 82–97)**

| Index | Level | R | G | B | Hex |
| ----- | ----- | --- | --- | --- | ------- |
| 82 | 0 | 0 | 0 | 24 | `#000018` |
| 86 | 4 | 0 | 12 | 88 | `#000C58` |
| 90 | 8 | 0 | 52 | 152 | `#003498` |
| 94 | 12 | 44 | 112 | 184 | `#2C70B8` |
| 97 | 15 | 92 | 164 | 212 | `#5CA4D4` |

**Group 6 — Green (indices 98–113)**

| Index | Level | R | G | B | Hex |
| ----- | ----- | --- | --- | --- | ------- |
| 98 | 0 | 0 | 24 | 0 | `#001800` |
| 102 | 4 | 4 | 64 | 0 | `#044000` |
| 106 | 8 | 20 | 108 | 0 | `#146C00` |
| 110 | 12 | 48 | 140 | 60 | `#308C3C` |
| 113 | 15 | 104 | 164 | 128 | `#68A480` |

### Upper Palette (indices 114–225)

The upper palette mirrors the same 7-group × 16-level structure, offset by 112.
`upper[i] = lower[i - 112]` for groups 0–6. This duplicated range is used for
tinting effects where the EffectColor transform needs separate upper/lower lookups.

### Semantic UI Colors

These `effectcolor` values are used on text and overlays to tint sprites via the
palette's color-lookup table. RGB values are from Palette 0.

| Index | Semantic Name | R | G | B | Hex | Used For |
| ----- | ---------------------- | --- | --- | --- | ------- | ------------------------------------------------ |
| 112 | Toggle Active | 84 | 156 | 104 | `#549C68` | Agency toggles (selected/deselected via brightness) |
| 114 | Hack Incomplete | 0 | 0 | 0 | `#000000` | Hacking progress lines, secret-carrier indicator |
| 123 | Loading Bar | 148 | 148 | 148 | `#949494` | Loading progress bar fill |
| 126 | Neutral Light | 200 | 200 | 200 | `#C8C8C8` | Object labels (ramp-color mode) |
| 128 | Deploy Message | 232 | 232 | 232 | `#E8E8E8` | Deploy/spawn announcement text |
| 129 | Info Tint | 252 | 252 | 252 | `#FCFCFC` | Map name, level, wins, losses, stats labels |
| 140 | Caret | 252 | 252 | 0 | `#FCFC00` | Text-input cursor — yellow |
| 146 | Health Damage | 12 | 4 | 4 | `#0C0404` | Damage-flash on health-only hits |
| 150 | Minimap Tint | 92 | 28 | 28 | `#5C1C1C` | Minimap icon brightness |
| 152 | Title Text | 132 | 28 | 28 | `#841C1C` | "zSilencer" title in lobby — dark red |
| 153 | Red Alert | 152 | 28 | 28 | `#981C1C` | Neutron activated, game lost, connection lost |
| 161 | Health Value | 252 | 80 | 80 | `#FC5050` | Health number on HUD — bright red |
| 189 | Version Label | 216 | 136 | 52 | `#D88834` | Version string in lobby — orange |
| 192 | Secret Dropped | 252 | 196 | 128 | `#FCC480` | Secret-dropped message — light orange |
| 194 | Shield Damage | 0 | 0 | 24 | `#000018` | Damage-flash on shield-only hits |
| 200 | User Info | 0 | 28 | 120 | `#001C78` | User info text — dark blue |
| 202 | Warm / Orange | 0 | 52 | 152 | `#003498` | Credits display, shield value text — blue |
| 204 | Team Color Base | 16 | 80 | 168 | `#1050A8` | Base index for team color decoding |
| 205 | Shield Stencil | 28 | 96 | 176 | `#1C60B0` | Shield-damage visual overlay |
| 208 | Standard Message | 72 | 148 | 200 | `#4894C8` | Default in-game announcement — sky blue |
| 210 | Poison / Base Entry | 0 | 24 | 0 | `#001800` | Poison-damage flash, player-in-base indicator |
| 224 | Highlight / Beacon | 84 | 156 | 104 | `#549C68` | Win message, secret-beacon indicator — green |

### Brightness Transform

Brightness is an 8-bit value (0–255) passed to `EffectBrightness()`. The transform
is a linear interpolation per channel (`palette.cpp:399`):

```
if brightness > 128:           // lighten toward white
    percent   = (brightness - 127) / 128
    output.ch = (input.ch * (1 - percent)) + (255 * percent)

if brightness < 128:           // darken toward black
    percent   = brightness / 128
    output.ch = input.ch * percent

if brightness == 128:          // no change (neutral)
    output = input
```

**Common brightness values used in the UI:**

| Value | Calculated Effect | Where Used |
| ----- | -------------------------------- | --------------------------------------------- |
| 0 | `ch * 0.0` → all black | — |
| 8 | `ch * 0.0625` → near-black | Text shadow minimum floor |
| 32 | `ch * 0.25` → very dark | Inactive/deselected toggle |
| 64 | `ch * 0.5` → half-dark | Inactive text-input text; shadow offset |
| 96 | `ch * 0.75` → dimmed | Incomplete hack-progress text |
| 128 | No change (neutral) | Default for all text and sprites |
| 136 | `ch*0.9375 + 255*0.0625` → slight boost | Chat text, HUD labels, button hover start |
| 144 | `ch*0.875 + 255*0.125` → bright | Tech description text |
| 160 | `ch*0.75 + 255*0.25` → brighter | Info labels, stat displays |
| 192 | `ch*0.5 + 255*0.5` → very bright | Highly emphasized elements |
| 255 | `ch*0.0 + 255*1.0` → all white | Full white |

### Color Tint Transform

`EffectColor()` (`palette.cpp:427`) performs a luminance-preserving tint:

```
luma_a = 0.30*a.r + 0.59*a.g + 0.11*a.b    // luma of source pixel
luma_b = 0.30*b.r + 0.59*b.g + 0.11*b.b    // luma of tint color
diff   = luma_a - luma_b
output = clamp(tint + diff, 0, 255)          // per channel
```

The result is then mapped to the nearest palette index via Euclidean-distance matching.

### Alpha Blend Transform

`Alpha()` (`palette.cpp:442`) performs standard linear alpha blending:

```
alpha  = ((pixelIndex - 2) % 16) / 16.0     // derived from ramp position
if alpha > 0.5: alpha = 1.0                  // binary threshold
output.ch = (a.ch * alpha) + (b.ch * (1 - alpha))
```

### Team Colors

Encoded in a single byte: upper 4 bits = brightness, lower 4 bits = hue.
Decoded via `TeamColorToIndex()` using the palette color + brightness lookups against
a base index of 204 (`#1050A8`, a mid-blue).

---

## Asset Formats

A reimplementation must read the same binary asset bundles, since every UI
component references sprites by `(bank, index)` pairs and every glyph is a
sprite in a font bank. Implementations in `src/resources.cpp`.

### Sprite Banks — `data/BIN_SPR.DAT` + `data/bin_spr/SPR_NNN.BIN`

**`BIN_SPR.DAT`** is the metadata index. Fixed size **16,384 bytes** = 256 banks ×
64 bytes per bank. Only one byte per bank is consulted:

| Offset | Size | Type | Meaning |
| ------ | ---- | ----- | ----------------------------------------------- |
| `+2`   | 1    | uint8 | Sprite count in this bank (0 = bank unused, skip) |

The other 63 bytes per entry are unused. If `header[bank][2] > 0`, load the
matching `data/bin_spr/SPR_NNN.BIN` (NNN = bank, zero-padded to 3 digits, e.g.
`SPR_007.BIN`, `SPR_132.BIN`).

**`SPR_NNN.BIN`** layout:

```
[ per-sprite header[0] (344 bytes) ]
[ per-sprite header[1] (344 bytes) ]
...
[ per-sprite header[N-1] (344 bytes) ]
[ compressed pixel data — concatenated, in sprite order ]
```

**Per-sprite header (344 bytes, little-endian):**

| Offset | Size | Type   | Field                                                  |
| ------ | ---- | ------ | ------------------------------------------------------ |
| `+0`   | 2    | uint16 | Width (pixels)                                         |
| `+2`   | 2    | uint16 | Height (pixels)                                        |
| `+4`   | 2    | int16  | OffsetX (signed; subtracted from `x` when blitting)    |
| `+6`   | 2    | int16  | OffsetY (signed; subtracted from `y` when blitting)    |
| `+12`  | 4    | uint32 | Compressed pixel-data size (bytes)                     |
| `+20`  | 1    | uint8  | Compression mode: `0` = linear RLE, non-zero = tile-ordered RLE |

(Other byte ranges within the 344-byte header are reserved/unused.)

The renderer reads `width`, `height`, `offsetX`, `offsetY` into the
`spriteWidth[bank][index]`, `spriteHeight[bank][index]`, `spriteOffsetX[bank][index]`,
`spriteOffsetY[bank][index]` arrays referenced throughout this document.

### RLE Codec (Sprites + Tiles)

Pixel data is decoded as a stream of 32-bit little-endian dwords. For each dword
read:

```
read dword D (uint32 LE)
if (D & 0xFF000000) == 0xFF000000:
    // Run-length sequence
    runBytes = D & 0x0000FFFF             // total bytes to emit (always a multiple of 4)
    pixel    = (D >> 16) & 0xFF           // the byte value to repeat
    word     = pixel | (pixel << 8)
    fill     = word  | (word  << 16)      // dword filled with 4 copies of pixel
    emit `runBytes / 4` copies of `fill`
else:
    emit D as a literal dword
```

Decoding stops when `compressedSize` bytes (from header `+12`) have been consumed.

**Linear mode** (`compression flag = 0`): emitted dwords fill the destination
buffer left-to-right, top-to-bottom in row-major order over the sprite's
`width × height` grid.

**Tile-ordered mode** (`compression flag != 0`): the destination is divided into
**64 × 64-pixel tiles** in row-major order. Within each tile, pixels are stored
row-major. Tiles at the right/bottom edges are clipped to the sprite's
dimensions. Use this layout for any sprite whose flag byte is non-zero.

### Transparency

**Palette index 0 is the transparent color key.** The runtime calls
`SDL_SetColorKey(surface, SDL_TRUE, 0)` on every loaded sprite. A reimplementation
should treat any pixel with palette index 0 as transparent during blit.

### Font Banks (132–136)

Font banks are **ordinary sprite banks** — same `BIN_SPR.DAT` entry, same
`SPR_NNN.BIN` format, same RLE codec. Each glyph occupies one sprite slot.

**Character → sprite-index mapping:**

```
asciiOffset = 34 if bank == 132 else 33
spriteIndex = charCode - asciiOffset
```

So the glyph for `'A'` (ASCII 65) in bank 135 is sprite index `65 - 33 = 32`.

- **Space (`0x20`) is never indexed.** The renderer skips it and just advances
  the cursor by `fontwidth`.
- **Glyphs above 0x80 are not supported.** Banks store glyphs for ASCII roughly
  33–127 only. There is no extended-ASCII or Unicode coverage.
- **Per-glyph `offsetY` is honored** — descenders (`g`, `p`, `y`) drop below the
  baseline via the sprite's stored OffsetY. There is no separate baseline metric;
  the sprite's offset *is* the baseline adjustment.
- **`fontwidth` is a render-time advance**, not stored in the font. The same
  font bank can be drawn with different advances at different call sites
  (e.g., bank 133 used at width 6 for chat and width 7 for status messages).
- **`'1'` special case:** `DrawTinyText()` (bank 132) shifts `'1'` by -1 px to
  visually center it; no other character has special handling.

### Tile Banks — `data/BIN_TIL.DAT` + `data/bin_til/TIL_NNN.BIN`

(Used for level/map rendering, not UI; documented for completeness.)

Identical metadata structure to sprites (256 banks × 64 bytes), but per-tile
headers in `TIL_NNN.BIN` are only **12 bytes** each (no W/H/offset fields — all
tiles are fixed at 64 × 64 pixels). The RLE codec is the same; the
compression-mode flag is at offset `+8` of the 12-byte header.

### Endianness

All multi-byte integers in `BIN_SPR.DAT`, `SPR_NNN.BIN`, `BIN_TIL.DAT`,
`TIL_NNN.BIN`, and `PALETTE.BIN` are **little-endian**. Big-endian ports must
byte-swap on read.

### Loading Strategy

- **Eager:** all sprite banks declared in `BIN_SPR.DAT` are decoded into 8-bit
  indexed `Surface` buffers at startup and cached in
  `spritebank[bank][index]` (`shared_ptr<Surface>`). No memory mapping.
- **Dedicated server (`-s`)** skips pixel decompression entirely; only the
  width/height/offsets are loaded so server-side hit-testing still works.
- **Lookup tables** (`PALETTECALC{n}.BIN`): if missing, the engine computes
  them from the palette and caches to disk. Ports can compute on-demand.

---

## Animation Timing & Tick Rate

Every animation timing in this document — caret blink (`% 32`), button activation
(4 ticks), low-HP flash (every 4 ticks), agency intro (120+ ticks), status fade
(16 ticks) — is measured in **simulation ticks**, not render frames.

### One tick = 42 ms (~24 Hz simulation)

`game.cpp:529` sets `wait = 42` ms in the main loop. The game uses a
fixed-timestep simulation with variable-rate rendering:

```
loop:
    now = SDL_GetTicks()
    while now - lastTick > 42:
        world.Tick()           // advances simulation, increments every state_i
        renderer.Tick()        // advances UI animation state (button anims, blink, etc.)
        lastTick += 42
    alpha = 1 - (now - lastTick) / 42
    renderer.Draw(alpha)       // interpolates between sim states for smooth rendering
```

So the **render frame rate is uncapped** (whatever the GPU/display delivers),
while **simulation and UI animation tick at ~23.8 Hz** (1000/42).

### Common timings in real time

| Animation                           | Ticks | Real time |
| ----------------------------------- | ----- | --------- |
| Button activation/deactivation      | 4     | 168 ms    |
| Caret blink full cycle (16 on / 16 off) | 32    | 1,344 ms  |
| Caret on or off phase               | 16    | 672 ms    |
| Low-HP / low-shield flash period    | 8     | 336 ms    |
| Status message fade-out             | 16    | 672 ms    |
| Buy-menu highlight pulse cycle      | 16    | 672 ms    |
| Bank-171 sprite anim cycle          | 8     | 336 ms    |
| Bank-54 10-frame sprite loop        | 10    | 420 ms    |
| Agency intro animation              | 120+  | 5+ s      |
| FPS counter / window-title refresh  | —     | 1,000 ms (uses real wall-clock timer, not ticks) |

### Replay scaling

When playing back a recorded match, `wait` is multiplied by `replay.speed`
(`game.cpp:546`), so animations slow down or speed up proportionally. UI
animations on top of an active replay inherit this scaling.

### Implication for ports

Any reimplementation that wants visual parity must drive UI animation state
from a fixed-rate timer at 23.8 Hz (or 24 Hz with negligible drift), not from
the render loop. Driving `state_i` from render frames would make a button
animation finish in ~67 ms on a 60 Hz display instead of the intended 168 ms.

---

## Components

> Every component below is documented with enough detail to recreate it in any
> language or framework. All coordinates are in the 640 × 480 logical pixel space.
> All colors are 8-bit palette indices (see [Color System](#color-system) for
> RGB/hex values). Sprite banks refer to pre-loaded bitmap sprite sheets — each
> bank contains numbered sprites accessed by index (see
> [Asset Formats](#asset-formats) for the file layout).

### Reusable Widgets vs. Screen Compositions

The component sections below mix two categories. A component library should
treat them differently:

**Reusable widgets** (build a library of these — they appear on multiple screens
and accept arbitrary configuration): Button, Toggle, TextInput, TextBox,
SelectBox, ScrollBar, Overlay, 9-slice / horizontal-stretch panel, Modal Dialog,
Loading Bar, Drawing Primitives.

**Screen-specific compositions** (build these *out of* the widgets above; their
positions and sprite banks are hardcoded for one screen): In-Game Chat Overlay,
Announcement Message, Status Messages, Top Message, Player List, Buy Menu, HUD
Bars, Team HUD, Minimap, Lobby Screen Panels.

A reimplementation should expose the reusable widgets as a parameterized API
and treat screen compositions as concrete layouts that wire those widgets
together at fixed coordinates.

### Shared Base: Sprite Properties

Every component is implemented as a subclass of the engine's `Object` /
`Sprite` base, and so internally carries these rendering properties
(`sprite.h` / `sprite.cpp`). A component library porting these to a different
language should expose the visually relevant ones on every widget:

| Property | Type | Default | Description |
| ------------------- | ----- | ------- | ------------------------------------------------- |
| `x`, `y` | int16 | 0 | Position in 640 × 480 logical coords |
| `res_bank` | uint8 | 0 | Sprite bank (sheet) for the visual |
| `res_index` | uint8 | 0 | Sprite index within the bank |
| `effectcolor` | uint8 | 0 | Color tint (palette index for `EffectColor()`) — 0 = no tint |
| `effectbrightness` | uint8 | 128 | Brightness (`EffectBrightness()`) — 128 = neutral |
| `draw` | bool | true | Whether to render the component |
| `drawcheckered` | bool | false | Every-other-pixel transparency mode |
| `drawalpha` | bool | false | Alpha-blend mode |
| `mirrored` | bool | false | Horizontally flip the sprite |
| `renderpass` | uint8 | 0 | Draw order layer (0–3, lower draws first) |

**Bounding box formula** (sprite-based hit-testing — used by Button, Toggle,
SelectBox, Overlay-in-sprite-mode, ScrollBar):

```
x1 = x - spriteOffsetX[res_bank][res_index]
y1 = y - spriteOffsetY[res_bank][res_index]
x2 = x1 + spriteWidth[res_bank][res_index]
y2 = y1 + spriteHeight[res_bank][res_index]
```

> **Exceptions:** `TextInput` and `TextBox` use a **rectangular** hit area
> defined by their explicit `x`, `y`, `width`, `height` properties (no sprite
> offset adjustment). `Overlay` in text mode uses a text-bounds box (see Overlay
> section). `Interface` containers use their own rectangular bounds for
> click-containment.

### Button

**Source:** `button.h` / `button.cpp` — `renderer.cpp` (DrawWorld render pass)

A sprite-backed button with text label, hover animation, and click detection.

#### Variants

| Type | Size (W × H) | Sprite Bank | Base `res_index` | Text Font Bank | Text Advance (px) | Text Y-Offset | Notes |
| -------------- | ------------- | ----------- | ---------------- | -------------- | ------------------ | ------------- | ------------------------------ |
| `B112x33` | 112 × 33 | 6 | 28 | 135 (~19 px) | 11 | 8 | Join, Create |
| `B196x33` | 196 × 33 | 6 | 7 | 135 (~19 px) | 11 | 8 | Default; lobby main actions |
| `B220x33` | 220 × 33 | 6 | 23 | 135 (~19 px) | 11 | 8 | Wide variant |
| `B236x27` | 236 × 27 | 6 | 2 | 135 (~19 px) | 11 | 8 | Extra-wide (config) |
| `B52x21` | 52 × 21 | — (none) | — | 133 (~11 px) | 7 | 8 (+1 px X) | Text-only, no sprite bg |
| `B156x21` | 156 × 21 | 7 | 24 | 134 (~15 px) | 8 | 4 | Brightness-only animation |
| `BCHECKBOX` | 13 × 13 | 7 | 19 (unchecked) | — | — | — | Binary toggle; no text, no hover anim |

#### Text Positioning

```
xoff = (width - strlen(text) * textwidth) / 2    // horizontal center
yoff = (see table above)                          // vertical offset from top

textX = button.x - spriteOffsetX[res_bank][res_index] + xoff
textY = button.y - spriteOffsetY[res_bank][res_index] + yoff
```

For `B52x21`, add +1 to `xoff` after centering.

#### State Machine

```
States: INACTIVE(0)  →  ACTIVATING(1)  →  ACTIVE(3)  →  DEACTIVATING(2)  →  INACTIVE(0)

Transitions:
  Mouse enters bounding box (or keyboard focus) → ACTIVATING, state_i = 0
  Mouse leaves bounding box (or loses focus)    → DEACTIVATING, state_i = 0
  state_i reaches 4 during ACTIVATING           → ACTIVE
  state_i reaches 4 during DEACTIVATING         → INACTIVE

Per-tick (each state_i increment):
  ACTIVATING:
    effectbrightness = 128 + (state_i * 2)          // 128, 130, 132, 134, 136
    res_index = base_index + state_i                 // sprite frame advances (except B156x21)
    if state_i == 0: play "whoom.wav"

  ACTIVE:
    effectbrightness = 136                           // stays at max hover brightness
    res_index = base_index + 4                       // final hover frame (except B156x21)

  DEACTIVATING:
    effectbrightness = 128 + ((4 - state_i) * 2)    // 136, 134, 132, 130, 128
    res_index = base_index + (4 - state_i)           // reverse animation (except B156x21)

  INACTIVE:
    effectbrightness = 128 (neutral)
    res_index = base_index

  BCHECKBOX: no animation at all; stays at base sprite
  B156x21:   brightness animates, but res_index stays fixed at 24
```

#### Click Detection

```
function mouseInside(mousex, mousey):
    x1 = button.x - spriteOffsetX[res_bank][res_index]
    x2 = x1 + width
    y1 = button.y - spriteOffsetY[res_bank][res_index]
    y2 = y1 + height
    return mousex > x1 AND mousex < x2 AND mousey > y1 AND mousey < y2
```

When `mouseInside` is true AND mouse button is pressed → `clicked = true`.
Hidden buttons (`draw = false`) are excluded from hit-testing.

#### Rendering Order

1. Draw sprite at `(x - offsetX, y - offsetY)` with `effectbrightness` applied
2. Draw text label at computed `(textX, textY)` using `DrawText(surface, textX, textY, text, textbank, textwidth)`
3. If `effectbrightness != 128`: apply `EffectBrightness()` to the sprite before blit

#### Keyboard Navigation

- Buttons register in an Interface's `tabobjects` list for Tab/arrow key focus
- Enter key triggers `clicked = true` on the focused button or the `buttonenter` button
- Escape key triggers `clicked = true` on the `buttonescape` button

---

### Toggle

**Source:** `toggle.h` / `toggle.cpp`

A binary visual switch. Two rendering modes: **agency icon** (sprite bank 181)
and **checkbox** (sprite bank 7). Used for agency selection in the lobby where
only one toggle per `set` can be active.

#### Properties

| Property | Type | Default | Description |
| ---------- | ------ | ------- | ------------------------------------------------ |
| `selected` | bool | false | Current on/off state |
| `set` | uint8 | 0 | Mutual-exclusion group — if non-zero, selecting this deselects all other toggles with the same `set` in the same Interface |
| `width` | uint8 | 0 | Read from sprite dimensions at runtime |
| `height` | uint8 | 0 | Read from sprite dimensions at runtime |
| `text` | char[64] | "" | Optional label, rendered above/at the toggle's `(x, y)` |

#### Label Rendering

If `text[0] != '\0'`, the renderer draws the label at
`(x - (strlen(text) * 9) / 2, y)` using **font bank 134, advance 9 px**
(`renderer.cpp:790-792`). Centered horizontally on the toggle's `x`. No tint or
brightness override is applied — the label uses default white text.

#### Visual States

**Agency icon mode** (`res_bank = 181`):

| State | `effectcolor` | `effectbrightness` | Visual |
| ---------- | ------------- | ------------------- | ------------------------------- |
| Selected | 112 | 128 (neutral) | Full-brightness tinted icon |
| Deselected | 112 | 32 (`ch * 0.25`) | Very dark / dimmed icon |

**Checkbox mode** (`res_bank = 7`):

| State | `res_index` | Visual |
| ---------- | ----------- | --------------- |
| Selected | 18 | Checked sprite |
| Deselected | 19 | Unchecked sprite |

Dimensions are always read from the sprite: `width = spriteWidth[res_bank][res_index]`,
`height = spriteHeight[res_bank][res_index]`.

#### Mutual Exclusion (Radio Group)

When a toggle with `set > 0` becomes selected, the Interface iterates all child
objects and deselects every other Toggle with the same `set` value. This
creates radio-button behavior.

#### Hit-Testing

Same sprite-offset bounding box as Button (see above). Click sets
`selected = true` and triggers radio-group deselection.

---

### TextInput

**Source:** `textinput.h` / `textinput.cpp` — rendered by `Renderer::DrawTextInput()`

A single-line text field with caret, scrolling, optional password mask, and
number-only restriction.

#### Properties

| Property | Type | Default | Description |
| ------------- | ------- | ------- | --------------------------------------------- |
| `res_bank` | uint8 | 135 | Font bank for text rendering |
| `fontwidth` | uint8 | 9 | Fixed advance per character (px) |
| `maxchars` | uint | 256 | Maximum characters in buffer |
| `maxwidth` | uint | 10 | Visible character slots before scrolling begins |
| `width` | uint16 | 0 | Hit area width (set by parent) |
| `height` | uint16 | 0 | Hit area height (set by parent) |
| `caretcolor` | uint8 | 140 | Palette index for caret — yellow `#FCFC00` |
| `password` | bool | false | Replaces each character with `*` when rendering |
| `numbersonly` | bool | false | Restricts input to ASCII 0x30–0x39 (digits 0–9) |
| `inactive` | bool | false | Disables input; renders at brightness 64 (half-dark) |
| `showcaret` | bool | false | Whether the caret is visible (set by Interface focus logic) |
| `scrolled` | uint | 0 | Number of characters scrolled off the left edge |

#### Common Field Instances (set in `game.cpp`)

| Field | Width × Height | Font Bank | Font Width | Buffer Size | Password | Numbers Only |
| --------------- | -------------- | --------- | ---------- | ----------- | -------- | ------------ |
| Username | 180 × 14 | 135 | 6 | 256 (buffer) | no | no |
| Password | 180 × 14 | 135 | 6 | 256 (buffer) | yes | no |
| Chat (lobby) | 360 × 14 | 135 | 6 | 60 | no | no |
| Chat (in-game) | varies | 133 | 6 | 60 | no | no |
| Game Name | 210 × 14 | 135 | 6 | 256 (buffer) | no | no |
| Small (numeric) | 20 × 20 | 135 | 8 | 256 (buffer) | no | yes |

> **Note:** "Buffer Size" is the `maxchars` allocation — the raw character buffer limit.
> The effective visible length is constrained by `maxwidth` (visible character slots)
> and the field's pixel width. The network protocol may impose additional limits on
> transmitted string lengths.

#### Rendering Pipeline

```
function drawTextInput(surface, input):
    text = input.text[input.scrolled .. end]          // scroll offset
    if input.password:
        text = repeat('*', strlen(text))

    brightness = input.effectbrightness
    if input.inactive:
        brightness = 64                                // dimmed

    drawText(surface, input.x, input.y, text, input.res_bank, input.fontwidth,
             alpha=false, color=input.effectcolor, brightness=brightness)

    // Caret
    if NOT input.inactive AND input.showcaret AND (renderer.state_i % 32 < 16):
        caretX = input.x + strlen(text) * input.fontwidth
        caretY = input.y - 1
        caretW = 1
        caretH = floor(input.height * 0.8)
        drawFilledRectangle(surface, caretX, caretY, caretX + caretW, caretY + caretH, input.caretcolor)
```

**Caret blink cycle:** 32-tick period. Visible for ticks 0–15, hidden for ticks 16–31.

#### Input Handling

```
function processKeyPress(key):
    if inactive: return

    if key == BACKSPACE:
        if caret > 0:
            caret--; text[caret] = NUL; offset--
            if scrolled > 0: scrolled--

    else if key == ENTER:
        enterpressed = true

    else if key == TAB:
        tabpressed = true

    else if (NOT numbersonly AND key >= 0x20 AND key <= 0x7F) OR (numbersonly AND key >= '0' AND key <= '9'):
        if offset >= maxchars: return
        if offset >= maxwidth + scrolled: scrolled++
        text[caret] = key; offset++; caret++
```

#### Hit-Testing

Simple rectangular bounds (no sprite offset):

```
function mouseInside(mousex, mousey) → int:
    if mousex > x AND mousex < x + width AND mousey > y AND mousey < y + height:
        return (mousex - x) / fontwidth            // character index at click position
    return -1                                      // outside bounds
```

When clicked, the Interface sets this TextInput as the active object and
`showcaret = true`. All other TextInputs get `showcaret = false`.

#### Intentionally Unsupported Edges

A reimplementation should *not* add the following — they are absent by design,
and adding them would diverge from the original behavior:

- **Cursor movement keys** (Left, Right, Home, End): not handled. The caret is
  always at the end of the buffer. Editing means typing or backspacing.
- **Mouse-click caret placement:** `MouseInside()` *returns* a character index,
  but the Interface does not call `SetCaretPosition()` with it.
  `SetCaretPosition()` itself is annotated `// this doesnt work` in
  `textinput.cpp:85` and does not adjust scroll, so it is unsafe to wire up.
- **Clipboard paste / Ctrl+V:** no SDL `TEXTINPUT` event handling for paste; no
  `SDL_GetClipboardText()` call anywhere in the input path.
- **IME / composition input:** no `TEXTEDITING` handling. Input is restricted
  to ASCII `0x20..0x7F` (or `0x30..0x39` when `numbersonly`).
- **Selection / shift-click:** no concept of a selection range.

The hover-state has no rendering either (no border highlight on mouse-over).
The only visual states are: focused (caret blinks), unfocused (no caret),
inactive (rendered at brightness 64).

---

### TextBox

**Source:** `textbox.h` / `textbox.cpp`

A multi-line, auto-scrolling text display area. Used for lobby chat messages,
player presence lists, tech descriptions, and item descriptions. Read-only —
does not accept user input directly.

#### Properties

| Property | Type | Default | Description |
| -------------- | -------------- | ------- | ------------------------------------------------ |
| `res_bank` | uint8 | 133 | Font bank (~11 px glyph height) |
| `lineheight` | uint8 | 11 | Vertical spacing per line (px) |
| `fontwidth` | uint8 | 6 | Fixed character advance (px) |
| `width` | uint16 | 100 | Viewport width (px) |
| `height` | uint16 | 100 | Viewport height (px) |
| `maxlines` | uint16 | 256 | Maximum buffered lines (oldest are dropped) |
| `bottomtotop` | bool | false | If true, renders from bottom up |
| `scrolled` | uint16 | 0 | Number of lines scrolled from the top |

#### Line Storage Format

Each line is a `vector<char>` with metadata appended after the null terminator:

```
[ char0 | char1 | ... | charN | NUL | color_byte | brightness_byte ]
                                       ↑ offset size+1  ↑ offset size+2
```

- `color_byte` (default 0): palette index for `EffectColor()` tint
- `brightness_byte` (default 128): value for `EffectBrightness()`

This means every line can have its own independent color and brightness.

#### Adding Lines

```
function addLine(string, color=0, brightness=128, scroll=true):
    if lines.size() > maxlines:
        lines.removeFirst()                            // drop oldest

    if scroll:
        visibleLines = height / lineheight
        if lines.size() > visibleLines:
            scrolled = lines.size() - visibleLines     // auto-scroll to bottom
        else:
            scrolled = 0

    maxCharsPerLine = width / fontwidth
    size = min(strlen(string), maxCharsPerLine)        // truncate to viewport
    newLine = allocate(size + 1 + 2)
    copy string[0..size] into newLine
    newLine[size] = NUL
    newLine[size + 1] = color
    newLine[size + 2] = brightness
    lines.append(newLine)
```

#### Word Wrapping (AddText)

`AddText()` wraps long text using `Interface::WordWrap()`, which breaks at
spaces or force-breaks at `width / fontwidth` characters. Each wrapped segment
becomes a separate line via `AddLine()`. An optional indent prepends spaces
after each `\n`.

#### Rendering

The parent Interface's render pass iterates `lines[scrolled .. scrolled + visibleLines]`
and draws each line:

```
for i = scrolled to min(scrolled + visibleLines, lines.size()):
    line = lines[i]
    color = line[strlen(line) + 1]
    brightness = line[strlen(line) + 2]
    drawText(surface, textbox.x, textbox.y + (i - scrolled) * lineheight,
             line, res_bank, fontwidth, alpha=false, color, brightness)
```

---

### SelectBox

**Source:** `selectbox.h` / `selectbox.cpp`

A single-selection list box. Items are text strings with optional numeric IDs.
Paired with a ScrollBar for overflow.

#### Properties

| Property | Type | Default | Description |
| -------------- | ------- | ------- | ------------------------------------------------ |
| `lineheight` | uint8 | 13 | Row height per item (px) |
| `width` | uint16 | — | Total width including scrollbar area |
| `height` | uint16 | — | Total height |
| `maxlines` | uint16 | 256 | Maximum item count |
| `selecteditem` | int | -1 | Currently selected index (-1 = none) |
| `scrolled` | uint16 | 0 | Number of items scrolled off the top |
| `enterpressed` | bool | false | Set true when Enter is pressed while focused |

#### Item Management

```
function addItem(text, id=0):
    items.append(copy(text))
    itemids.append(id)
    visibleItems = height / lineheight
    if items.size() > visibleItems:
        scrolled = items.size() - visibleItems         // auto-scroll to show new item
    else:
        scrolled = 0

function deleteItem(index):
    free items[index]
    items.remove(index)
    itemids.remove(index)
```

#### Hit-Testing

The hit area excludes 16 px on the right (reserved for the scrollbar):

```
function mouseInside(mousex, mousey):
    x1 = x - spriteOffsetX[res_bank][res_index]
    x2 = x1 + width - 16                              // scrollbar reservation
    y1 = y - spriteOffsetY[res_bank][res_index]
    y2 = y1 + height
    if mousex > x1 AND mousex < x2 AND mousey > y1 AND mousey < y2:
        index = ((mousey - y1) / lineheight) + scrolled
        if index < items.size(): return index
    return -1
```

#### Rendering

`renderer.cpp:735-757`. The renderer iterates visible items (from `scrolled` to
`scrolled + visibleItems`), drawing each item's text at the corresponding Y offset:

```
for i = scrolled to min(scrolled + visibleItems, items.size()):
    line = i - scrolled
    if i == selecteditem:
        // Highlight rectangle: full width, 11 px tall, drawn behind text
        drawFilledRectangle(surface,
            x, y + line * lineheight,
            x + width, y + line * lineheight + 11,
            color = 180)        // mid-gray ramp color
    drawText(surface, x, y + line * lineheight, items[i],
             bank = 133, fontwidth = 6)
```

- Selected item: filled rectangle of palette index **180** (mid-gray) underneath,
  text drawn on top in font bank 133, width 6, default brightness.
- Unselected items: text only, no background.
- **Hover (mouse over but not selected) has no visual feedback.** Hover only
  routes clicks; it does not change rendering.

Keyboard up/down changes `selecteditem`; Enter sets `enterpressed = true`.
Crossing the visible-window boundary triggers a `scrollUp`/`scrollDown` on the
paired ScrollBar.

#### File Listing

`ListFiles(directory)` scans a directory and calls `AddItem()` for each
non-hidden file. Cross-platform: uses POSIX `opendir`/`readdir` or
Win32 `FindFirstFile`/`FindNextFile`.

---

### ScrollBar

**Source:** `scrollbar.h` / `scrollbar.cpp`

A vertical scrollbar with up/down arrow buttons and a thumb track. Always
paired with a SelectBox or TextBox.

#### Properties

| Property | Type | Default | Description |
| --------------- | ------ | ------- | ------------------------------------------------ |
| `res_bank` | uint8 | 7 | Sprite bank for the scrollbar track |
| `res_index` | uint8 | 9 | Sprite index for the track background |
| `barres_index` | uint8 | 10 | Sprite index for the thumb indicator |
| `scrollposition` | uint16 | 0 | Current scroll offset |
| `scrollmax` | uint16 | 0 | Maximum scroll value |
| `draw` | bool | false | Whether to render (hidden until content overflows) |

#### Hit Regions

The scrollbar divides into three vertical zones based on the track sprite
dimensions:

```
trackWidth  = spriteWidth[res_bank][res_index]
trackHeight = spriteHeight[res_bank][res_index]
x1 = x - spriteOffsetX[res_bank][res_index]
y1 = y - spriteOffsetY[res_bank][res_index]

Up arrow:   (x1, y1)                  to (x1 + trackWidth, y1 + 16)
Track:      (x1, y1 + 16)             to (x1 + trackWidth, y1 + trackHeight - 16)
Down arrow: (x1, y1 + trackHeight-16) to (x1 + trackWidth, y1 + trackHeight)
```

Each arrow button hit area is **16 px tall**.

#### Scroll Logic

```
function scrollUp():
    if scrollposition > 0: scrollposition--

function scrollDown():
    if scrollposition < scrollmax: scrollposition++
```

The Interface also forwards mouse wheel events to the scrollbar:
wheel up → `scrollUp()`, wheel down → `scrollDown()`. When navigating via
keyboard past the first/last visible item, the Interface calls
`scrollUp()`/`scrollDown()` on the paired scrollbar and plays `"whoom.wav"`.

---

### Overlay

**Source:** `overlay.h` / `overlay.cpp`

A generic sprite or text label component. Used for static images, animated
decorations, clickable text links, and custom pixel-buffer graphics.

#### Properties

| Property | Type | Default | Description |
| -------------------- | ----------- | ------- | ----------------------------------------------- |
| `text` | string | "" | Text content (if non-empty, renders as text label) |
| `textbank` | uint8 | 135 | Font bank for text rendering |
| `textwidth` | uint8 | 8 | Character advance (px) |
| `textlineheight` | int | 10 | Line spacing for multi-line text |
| `textcolorramp` | bool | false | Use ramp-color tint instead of standard color |
| `textallownewline` | bool | false | Allow `\n` line breaks in text |
| `drawalpha` | bool | false | Alpha-blend rendering mode |
| `clicked` | bool | false | Set true when clicked inside bounds |
| `customsprite` | byte[] | [] | Optional raw pixel buffer for custom graphics |
| `customspritew` | int | 0 | Width of custom pixel buffer |
| `customspriteh` | int | 0 | Height of custom pixel buffer |

#### Sprite Animations

When used as an animated sprite, the Overlay's `Tick()` advances `state_i`
each frame and maps it to a `res_index` based on the bank:

| Bank | Animation Pattern | Notes |
| ---- | -------------------------------------------------- | ----------------------------------- |
| 54 | `res_index = state_i`, wraps at 9 | 10-frame looping animation |
| 56 | `res_index = 0` always | Static sprite |
| 57 | `res_index = state_i / 4`, holds at 16+, 1% reset | Slow animation with random restart |
| 58 | Same as 57 | Different sprite set, same timing |
| 171 | `res_index = (state_i / 2) % 4` | 4-frame loop, half-speed |
| 208 | Complex ramp up/hold/ramp down over 120+ ticks | Agency intro animation |
| 222 | `res_index = state_i`, destroys at 3 | 4-frame one-shot, then self-destruct (frames 0–3 all render; destroy is deferred to end-of-tick) |

#### Hit-Testing

Two modes depending on whether `text` is populated:

**Text mode** (`text.length() > 0`):
```
x1 = x;  x2 = x + (text.length() * textwidth)
y1 = y;  y2 = y + glyphHeight[textbank]

// Glyph heights by bank:
//   133 → 11,  134 → 15,  135 → 19,  136 → 23
```

**Sprite mode** (`text` is empty):
```
x1 = x - spriteOffsetX[res_bank][res_index]
x2 = x1 + spriteWidth[res_bank][res_index]
y1 = y - spriteOffsetY[res_bank][res_index]
y2 = y1 + spriteHeight[res_bank][res_index]
```

---

### Interface (Container / Focus Manager)

**Source:** `interface.h` / `interface.cpp`

Not a visual component itself — it's the **container** that manages a group of
UI components, handles focus/tab order, keyboard routing, and mouse dispatch.
Every screen (lobby, game create, config, buy menu, chat) is built as an
Interface containing Buttons, TextInputs, SelectBoxes, etc.

#### Properties

| Property | Type | Default | Description |
| ------------------ | ---------- | ------- | --------------------------------------------- |
| `x`, `y` | uint16 | 0 | Bounding box origin for click containment |
| `width`, `height` | uint16 | 0 | Bounding box for focus area |
| `activeobject` | uint16 | 0 | ID of the currently focused child component |
| `buttonenter` | uint16 | 0 | Button ID triggered by Enter key |
| `buttonescape` | uint16 | 0 | Button ID triggered by Escape key |
| `scrollbar` | uint16 | 0 | ID of the paired ScrollBar (if any) |
| `disabled` | bool | false | Ignores all input when true |
| `modal` | bool | false | Prevents child Interfaces from receiving input |

#### Focus / Tab Order

Components are added to two lists:
- `objects[]` — all child components (for rendering and mouse hit-testing)
- `tabobjects[]` — focusable subset (for Tab/arrow key navigation)

Tab cycles forward through `tabobjects[]`; Shift+arrow cycles backward. The
first component added to `tabobjects[]` gets initial focus.

#### Keyboard Routing

| Key | Action |
| ------- | ------------------------------------------------------- |
| Tab | Focus next in `tabobjects[]` |
| Enter | Trigger `buttonenter`'s click; or `enterpressed` on focused SelectBox |
| Escape | Trigger `buttonescape`'s click |
| Left | Previous focusable (or no-op if focused on SelectBox) |
| Right | Next focusable (or no-op if focused on SelectBox) |
| Up | Previous focusable; if at scroll boundary, scroll up |
| Down | Next focusable; if at scroll boundary, scroll down |

Character keys (printable ASCII, backspace) are forwarded to the focused
TextInput.

#### Mouse Dispatch

On mouse move or click, the Interface iterates all `objects[]` and:
1. **ScrollBar** — click on up/down arrows or track; mouse wheel forwarded
2. **Child Interface** — if click is inside its bounding box, focus shifts to it
3. **SelectBox** — click selects the item at the mouse Y position
4. **Overlay** — click sets `overlay.clicked = true`
5. **Button** — mouse enter → Activate; mouse leave → Deactivate; click → `clicked = true`
6. **TextInput** — click focuses the field and shows the caret
7. **Toggle** — click selects it; radio-group exclusion runs if `set > 0`

Interfaces can be **nested**: a parent Interface contains child Interfaces as
objects. The parent delegates focus and events to the active child.

#### Focus Visual Indicator

There is **no dedicated focus border, glow, or ring** on any widget. Focus is
expressed entirely through each widget's existing visual states:

| Widget    | Focus visual                                                                 |
| --------- | ---------------------------------------------------------------------------- |
| Button    | Same as hover — enters `ACTIVATING` → `ACTIVE` (brightness 128 → 136, sprite frame advances). Mouse hover and keyboard focus produce identical visuals. |
| Toggle    | None separate from selected state. Click toggles `selected`; the visual changes only on the selection itself. |
| TextInput | The blinking caret is the focus indicator. Unfocused inputs render with `showcaret = false` (no caret). Inactive inputs render at `effectbrightness = 64` regardless of focus. |
| TextBox   | Read-only — never focusable. |
| SelectBox | Focus enables keyboard up/down/Enter routing but does not change rendering. The selected-item highlight (palette index 180 background) is independent of focus. |
| ScrollBar | Not directly focusable — it receives input via the focused SelectBox/TextBox or via mouse. |

A reimplementation should reproduce this minimalist style: no extra "this widget
is focused" chrome on top of the widget's existing states.

---

### 9-Slice Panel (Chat Message Background)

**Source:** `renderer.cpp:3013` — `DrawMessageBackground()`

A stretchable panel built from 9 sprite pieces in bank **188**. Used for the
in-game chat overlay background.

#### Sprite Map

| Index | Piece | Tiling |
| ----- | ------------- | ---------------------------------------- |
| 0 | Top-left | Fixed corner |
| 1 | Top edge | Tiled horizontally to fill width |
| 2 | Top-right | Fixed corner (positioned at `x + width - 36`) |
| 3 | Left edge | (unused in current code — sides rendered by top/bottom rows) |
| 4 | Center fill | (unused in current code) |
| 5 | Right edge | (unused in current code) |
| 6 | Bottom-left | Fixed corner |
| 7 | Bottom edge | Tiled horizontally to fill width |
| 8 | Bottom-right | Fixed corner (positioned at `x + width - 36`) |

#### Rendering Algorithm

```
function drawMessageBackground(surface, rect):
    // Top row
    blit(bank188[0], x=rect.x, y=rect.y)                           // top-left corner
    tileX = spriteWidth[188][0]
    while tileX < rect.w - spriteWidth[188][2]:
        clipW = min(rect.w - tileX - 36, spriteWidth[188][1])
        blit(bank188[1], x=rect.x + tileX, y=rect.y, srcW=clipW)  // tiled top edge
        tileX += clipW
    blit(bank188[2], x=rect.x + rect.w - 36, y=rect.y)            // top-right corner

    // Bottom row (at y = rect.y + rect.h)
    blit(bank188[6], x=rect.x, y=rect.y + rect.h)                 // bottom-left
    tileX = spriteWidth[188][6]
    while tileX < rect.w - spriteWidth[188][8]:
        clipW = min(rect.w - tileX - 36, spriteWidth[188][7])
        blit(bank188[7], x=rect.x + tileX, y=rect.y + rect.h, srcW=clipW)
        tileX += clipW
    blit(bank188[8], x=rect.x + rect.w - 36, y=rect.y + rect.h)  // bottom-right
```

The corner offset of **36 px** is hardcoded — the right-side corners are always
placed at `rect.x + rect.w - 36`.

> **Note on the "9-slice" name:** The sprite bank reserves indices 3–5 for left
> edge, center fill, and right edge, but the renderer never draws them. In
> practice this is a **two-row horizontal-stretch panel** — only the top and
> bottom edges (with corners) are drawn, and they tile horizontally only. The
> panel does not stretch vertically; its height is fixed by the spacing between
> the top and bottom rows.

---

### Modal Dialog

**Source:** `game.cpp:3776` — `Game::CreateModalDialog(message, ok)`

A centered dialog overlay used for connection errors, validation failures, and
async-operation status. Built from existing widgets (Overlay + optional Button +
Interface container).

#### Composition

| Piece | Component | Properties |
| ------------- | --------- | ----------------------------------------------------------- |
| Background    | Overlay   | `res_bank = 40`, `res_index = 4`, `renderpass = 3`. Sprite is pre-centered via its baked-in offsets — no `(x, y)` is set. |
| Message text  | Overlay   | `textbank = 134`, `textwidth = 8`, centered at `x = 320 - (len(text) * 8) / 2`, `y = 200` (with OK button) or `y = 218` (no button). |
| OK button     | Button    | Type `B156x21`, `(x, y) = (242, 230)`, `text = "OK"`, `uid = 50`. Bound to the parent Interface's `buttonenter` so Enter triggers it. Omitted when `ok = false`. |
| Container     | Interface | `modal = true`. Pushed onto the `currentinterface` stack so it receives all input. The previous interface ID is saved in `aftermodalinterface` and restored on `DestroyModalDialog()`. |

#### Behavior

- **Modal flag** prevents the underlying interface from receiving input. The
  background sprite is opaque; there is no separate dimming/scrim layer.
- **Dismissal:** click OK, press Enter (via `buttonenter`), or call
  `DestroyModalDialog()` programmatically when an async operation completes.
  **Escape is not handled** — there is no automatic cancel.
- **Status-only dialogs** (`ok = false`) have no button at all and stay open
  until code explicitly closes them. Used while waiting for the lobby server
  to respond (e.g., "Creating game…").

#### Use Sites

| Trigger                                  | Message                            | OK |
| ---------------------------------------- | ---------------------------------- | -- |
| Game create failed                       | "Could not create game"            | yes |
| Join failed (full / wrong password)      | "Unable to join game"              | yes |
| Network loss during play                 | "Disconnected from game"           | yes |
| Player level too low for game            | "Your player level is too low"     | yes |
| Player level too high for game           | "Your player level is too high"    | yes |
| Clicked Join with no game selected       | "No game selected"                 | yes |
| Clicked Create with no game name         | "No game name"                     | yes |
| Clicked Create with no map selected      | "No map selected"                  | yes |
| Async create in progress                 | "Creating game…"                   | no  |

(Source: `game.cpp:858, 868, 946, 4811, 4815, 4830, 4980, 4993, 4995`.)

---

### Loading Bar

**Source:** `game.cpp:484` — `LoadProgressCallback()`

A minimal progress indicator shown during resource loading.

#### Rendering

```
totalWidth = 500
filledWidth = (progress / totalItems) * totalWidth
color = 123 (palette index — gray #949494)

x1 = (640 - totalWidth) / 2        // = 70
y1 = (480 - 20) / 2                // = 230
x2 = (640 + filledWidth) / 2
y2 = (480 + 20) / 2                // = 250

drawFilledRectangle(surface, x1, y1, x2, y2, color)
```

- Centered at (320, 240) in the 640 × 480 buffer
- Total bar area: 500 × 20 px
- Fill color: palette index 123 (`#949494` — mid-gray)
- Update rate: throttled to every 100 ms (`SDL_GetTicks()` guard)
- No border, no text label, no background — just a filled rectangle that grows

---

### In-Game Chat Overlay

**Source:** `renderer.cpp:2926` — within `DrawHUD()`

The in-game chat box appears when the player presses the chat key or there are
recent messages.

#### Layout

```
background rect: x=400, y=280, w=231, h=30
9-slice panel rendered via DrawMessageBackground()
```

#### Rendering

```
function drawChatOverlay(surface):
    rect = {x: 400, y: 280, w: 231, h: 30}
    drawMessageBackground(surface, rect)

    yoffset = 10
    for each chatline (up to 5 lines):
        text = truncate(chatline, 36 chars)
        drawText(surface, rect.x + 10, rect.y + yoffset, text,
                 bank=133, width=6, color=0, brightness=136)
        yoffset += 10

    if chat input is active:
        prepend = "(ALL):" or "(TEAM):"
        drawText(surface, rect.x + 10, rect.y + yoffset, prepend,
                 bank=133, width=6, color=0, brightness=136)
        textInput.x = rect.x + strlen(prepend) * 6 + 10
        textInput.y = rect.y + yoffset
        drawTextInput(surface, textInput)
```

- Font: bank 133, width 6, brightness 136 (slightly bright)
- Max visible lines: 5 (if chat input is active and 5 lines exist, the first is dropped)
- Max characters per line: 36 (hard truncated)
- Line spacing: 10 px
- Left padding: 10 px from background edge
- Top padding: 10 px from background edge

---

### Announcement Message (DrawMessage)

**Source:** `renderer.cpp:1649`

Full-screen centered announcement text with per-character brightness animation,
drop shadow, and type-dependent coloring.

#### Message Types

| Type | Color Index | Color Hex | Y Position | Font | Width | Description |
| ---- | ----------- | --------- | ---------- | ---- | ----- | ---------------------- |
| 0 | 208 | `#4894C8` | 60 | 135 | 11 | Default announcement |
| 1 | 128 | `#E8E8E8` | 190 | 134 | 10 | Deploy/spawn message |
| 2 | 128 | `#E8E8E8` | 60 | 135 | 11 | Secret picked up |
| 3 | 192 | `#FCC480` | 60 | 135 | 11 | Secret dropped |
| 4 | 153 | `#981C1C` | 60 | 135 | 11 | Neutron activated |
| 10 | 224 | `#549C68` | 60 | 136/135 | 25/13 | Game won |
| 11 | 153 | `#981C1C` | 60 | 136/135 | 25/13 | Game lost |
| 20 | 153 | `#981C1C` | 200 | 135 | 11 | Connection lost |

#### Multi-Line Layout

- Line 1 centered: `x = (640 - lineLength * textwidth) / 2`
- Line break: `\n` in the message string
- Line height: 20 px (standard), 40 px gap after line 1 for win/loss (types 10, 11)
- Win/loss: line 1 uses bank 136 / width 25 (extra-large headline); line 2+ uses bank 135 / width 13

#### Per-Character Brightness Animation

```
for each character i in message:
    brightness = 128

    // Fade-out after display time expires (types < 10)
    // Note: brightness is a uint8 in the engine; underflow wraps.
    // A clean reimplementation should clamp: brightness = max(0, brightness - delta)
    if message_i - messagetime + 8 >= 0:
        brightness -= (message_i - messagetime + 8) * 8

    // Triangular wave pulse (types < 10)
    if message_i % 32 >= 16:
        brightness += (16 - (message_i % 16)) * 2
    else:
        brightness += (message_i % 16) * 2

    // Typewriter reveal boost for leading characters
    if message_i - i <= 5:
        brightness += 40 - ((message_i - i) * 8)
```

#### Drop Shadow

Every character is drawn twice:
1. Shadow: at `(x+1, y+1)` with `brightness2 = max(brightness - 64, 8)`
2. Foreground: at `(x, y)` with `brightness`

---

### Status Messages (DrawStatus)

**Source:** `renderer.cpp:1743`

A stack of short messages that appear near the bottom-center of the screen
(kills, pickups, events).

#### Layout & Rendering

```
baseY = 370
font: bank 133, width 7
each message centered: x = (640 - strlen(text) * 7) / 2

for each message (newest first):
    color  = stored per-message (byte after null terminator + 1)
    time   = stored per-message (byte after null terminator)
    brightness = 128
    if time <= 16:
        brightness -= (16 - time) * 8              // fade out over last 16 ticks

    // Drop shadow
    brightness2 = max(brightness - 64, 8)
    drawText(surface, x+1, baseY + yoffset + 1, text, 133, 7, color, brightness2)
    drawText(surface, x,   baseY + yoffset,     text, 133, 7, color, brightness)
    yoffset -= 10                                  // stack upward
```

Messages stack **upward** from y=370 (each subsequent message is 10 px higher).

---

### Top Message (DrawTopMessage)

**Source:** `renderer.cpp:1760`

A single-line scrolling message pinned to the top of the screen (game events,
objective updates).

```
position: x=200, y=10
font: bank 133, width 7
max visible: 35 characters

// Scroll logic: if message_i / 2 > 24, start showing from offset (message_i/2 - 24)
// This creates a typewriter-style reveal: characters appear left to right
// then the visible window scrolls if the message is longer than 35 chars

drawText(surface, 200, 10, text[0..35], 133, 7)
```

---

### Player List (DrawPlayerList)

**Source:** `renderer.cpp:2968`

Shown when the player holds the scoreboard key. Lists all teams with agency
icons and per-player stats.

#### Layout

```
bounds: x=50 to x=590, y=50 to y=50 + 10 + (numTeams * 58)
background: checkered pattern (every-other-pixel black) over the bounds area

Per team block (58 px tall):
    Agency icon: drawn at (60, 60 + yoffset + 10) using bank 181, scaled 2×, team-colored
    Player rows: 12 px per player, vertically centered in the 58 px block

        // Vertical centering: the team block reserves space for up to 4 players;
        // when the team has fewer, leftover space is split evenly above and below.
        vertCenter = ((4 - team.numpeers) * 12) / 2

        Name:  drawn at (x = 60 + 40, y = 60 + yoffset + vertCenter + i*12 + 1), bank 133, width 6
        Stats: right-aligned at (x = 580 - textWidth), same Y, bank 133, width 6
               format: "L:{level}    E:{endurance}  S:{shield}  J:{jetpack}  H:{hacking}  C:{contacts}"
```

**Checkered background:** Instead of a solid fill, the background draws black
pixels in a checkerboard pattern (every other pixel on alternating rows),
creating a 50% transparent dark overlay.

---

### Buy Menu

**Source:** `renderer.cpp:2785` — within `DrawHUD()`

The in-game buy/tech interface showing purchasable items.

#### Layout

```
Background: sprite bank 102, index 0 (full buy menu frame)
Highlight:  sprite bank 102, index 1 (selected row highlight)
Up arrow:   sprite bank 102, index 2
Down arrow: sprite bank 102, index 3

visible items: 5
row height: 25 px

Per item row:
    Item sprite:  at (169 + spriteOffset, 139 + yoffset + spriteOffset)
    Item name:    at (222, 145 + yoffset), bank 134, width 9
    Price:        at (440 - (strlen(price) * 9) / 2, 145 + yoffset), bank 134, width 9, centered

Available credits line:
    text at (320 - (strlen(text) * 9) / 2, 275), bank 134, width 9, centered
```

#### Selection Highlight Animation

```
brightness = 128
if state_i % 16 >= 8:
    brightness += (state_i % 8)        // +0..+7  (state_i 8-15)
else:
    brightness += 8 - (state_i % 8)    // +8..+1  (state_i 0-7)
// Triangular pulse: 136 → 128 → 135 → 136 over 16 ticks (starts HIGH at state_i=0,
// dips to neutral at state_i=8, climbs back; period 16 ticks ≈ 672 ms)
```

The selected item's sprite and text are drawn with this animated brightness.
The highlight sprite (bank 102 index 1) is drawn behind the selected row.

---

### HUD Bars

**Source:** `renderer.cpp:2351` — `DrawHUD()`

The main gameplay HUD uses sprite bank **95** for health, shield, fuel,
and file progress bars, and bank **94** for the frame/border.

#### Health Bar

```
sprite: bank 95, index 0
fill direction: bottom-up (crop from top based on HP percentage)

srcRect.y = spriteHeight - (float(health) / float(maxHealth)) * spriteHeight
srcRect.h = spriteHeight
dstRect.y = spriteOffsetY + srcRect.y

Low HP warning (health/maxHealth <= 0.5):
    flash sprite bank 95 index 3 every 4 ticks (visible for ticks 0-3, hidden 4-7)

Health number: DrawTinyText at (158, 463), tint=161 (#FC5050, bright red)
```

#### Shield Bar

```
sprite: bank 95, index 1
fill direction: bottom-up (same as health)

Overshield effect (shield > maxShield):
    brightness pulses: base 136, ±(state_i % time) * 2 triangular wave, time=6

Low shield warning (shield/maxShield <= 0.5):
    flash sprite bank 95 index 4 every 4 ticks

Shield number: DrawTinyText at (481, 463), tint=202 (#003498, blue)
```

#### Fuel Bar

```
sprite: bank 95, index 6
fill direction: left-to-right (crop width based on fuel percentage)

srcRect.w = (fuel / maxFuel) * spriteWidth
Fuel frame: bank 95, index 5
Low fuel warning: bank 95, index 8 (when player.fuellow is true)
```

#### File Progress Bar

```
sprite: bank 95, index 7
fill direction: left-to-right

srcRect.w = (files / maxFiles) * spriteWidth
```

#### Ammo Display

```
Current weapon ammo: DrawText at (117, 457), bank 135, width 12, alpha=true
Per-weapon ammo counts: DrawTinyText at (10, 414/428/442/456)
Credits: DrawText at (572, 456), bank 135, width 12, tint=202 (#003498)
```

#### Inventory Slots

```
4 slots at x offsets: [612, 584, 556, 528], y offsets: [13, 13, 11, 7]
Active slot: full brightness sprite + DrawTinyText label
Inactive slot: brightness=32 (very dark) + DrawTinyText with brightness=32
Item count > 1: DrawText at (x+20, y+20), bank 132, width 6
```

---

### Team HUD

**Source:** `renderer.cpp:2607` — within `DrawHUD()`

Displays team icons and player status indicators.

```
Per team row (20 px tall, starting at y=5):
    Agency icon: bank 181, team-colored, scaled 2×, at (5, teamY + 1)
    Player dots: bank 103, indices 4-7 (alive) or 8-11 (dead)
        positioned at (25 + 17*i, teamY) with sprite offsets
        In-base indicator: EffectRampColorPlus with color 210 (#001800, green), pulsing
        Has-secret indicator: EffectRampColorPlus with color 114 (#000000), slower pulse

    Secret indicators (3 slots):
        Collected: bank 103 index 2
        Empty: bank 103 index 3
        Being carried: flash between 2 and 3 every 6 ticks
        Beaming: ramp-colored with 224 (#549C68)
```

---

### Minimap

**Source:** `renderer.cpp:2423`, `minimap.h`

```
position: x=235, y=419
size: 172 × 62 px
frame: sprite bank 94, index 0

The minimap is a pre-rendered pixel buffer (172 × 62 bytes, palette-indexed)
generated from the map data. Each frame, the buffer is reset from the stored
map minimap, then entity markers are drawn on top via MiniMapBlit() and
MiniMapCircle().
```

---

### Drawing Primitives

These low-level functions are used by the components above. A component library
would need equivalent implementations.

#### FilledRectangle

```
function drawFilledRectangle(surface, x1, y1, x2, y2, color):
    for x = x1 to x2:
        for y = y1 to y2:
            setPixel(surface, x, y, color)
```

Single palette-index fill. No anti-aliasing.

#### Line

```
function drawLine(surface, x1, y1, x2, y2, color, thickness=1):
    // Bresenham's line algorithm
    // At each step: drawFilledRectangle(x, y, x+thickness, y+thickness, color)
    // Handles vertical, shallow (|slope| < 1), and steep (|slope| >= 1) cases
```

#### Circle

```
function drawCircle(surface, x, y, radius, color):
    // Midpoint circle algorithm (Bresenham variant)
    // Draws 8 symmetric points per step — outline only, not filled
```

#### Checkered Fill

```
function drawCheckered(surface, x1, y1, x2, y2, color):
    for y = y1 to y2:
        for x = x1 + (y % 2) to x2 step 2:
            setPixel(surface, x, y, color)
    // Every-other-pixel pattern — creates 50% visual transparency
```

---

## Layout & Spacing

### Screen & Display Scaling

The game renders everything to a fixed **640 × 480** internal surface (8-bit indexed
color). This surface is then scaled to fill the window or display at presentation time.

**Rendering pipeline:**

1. All game and UI rendering draws to `screenbuffer` (a `Surface` of 640 × 480 × 8bpp)
2. Each 8-bit palette index is expanded to the display's native pixel format using a
   pre-built `streamingtexturepalette[256]` lookup (maps palette index → RGB/RGBA)
3. The expanded pixels are written to an SDL streaming texture at 640 × 480
4. `SDL_RenderCopy()` scales that texture to fill the window (preserving nothing —
   the full window rect is used, so the aspect ratio stretches if the window is not 4:3)

**Scale filter** (`config.cfg: scalefilter`):

| Setting | SDL Hint | Effect |
| ------- | ----------------------------- | ---------------------------------------------- |
| `0` | `SDL_HINT_RENDER_SCALE_QUALITY = "nearest"` | Pixel-perfect / blocky — sharp edges at any size |
| `1` (default) | `SDL_HINT_RENDER_SCALE_QUALITY = "linear"` | Bilinear filter — smoothed/blurred at large sizes |

**Mouse input scaling** (`game.cpp:6155`):

All mouse coordinates are converted from window space to the 640 × 480 logical space
at the event handler level, before any game logic sees them:

```cpp
int w, h;
SDL_GetWindowSize(window, &w, &h);
logicalX = (float(event.button.x) / w) * 640;
logicalY = (float(event.button.y) / h) * 480;
```

This means all hit-testing, button bounds, and UI coordinates operate entirely in
640 × 480 logical pixels regardless of the actual window or display resolution.

**Font / UI scaling implications:**

- Fonts are bitmap sprites at fixed pixel sizes — they do **not** scale independently
  of the world. A glyph that is 11 px tall in the 640 × 480 buffer stays 11 logical
  px and is stretched along with everything else when presented to the window.
- On a 1920 × 1080 display, each logical pixel becomes roughly 3 × 2.25 physical pixels.
  With `scalefilter=0` (nearest), text appears blocky but sharp. With `scalefilter=1`
  (linear), text appears slightly blurred.
- On a 3840 × 2160 (4K) display at fullscreen, each logical pixel is ~6 × 4.5 physical
  pixels. The bitmap fonts are visibly pixelated at this scale.
- There is **no HiDPI/Retina awareness** — no `SDL_WINDOW_ALLOW_HIGHDPI` flag is set.
  The window size is in screen coordinates, not physical pixels, so on macOS Retina
  displays the game renders at the logical (point) resolution, not the backing-store
  resolution.
- There is **no aspect-ratio correction** — the 640 × 480 buffer stretches to fill the
  entire window rect. Non-4:3 windows will distort the image.

**Window modes:**

| Mode | Flag | Behavior |
| ---------- | ----------------------------------- | ------------------------------------------------ |
| Windowed | `0` | Opens at 640 × 480; user can resize freely |
| Fullscreen | `SDL_WINDOW_FULLSCREEN_DESKTOP` | Uses desktop resolution; 640×480 stretched to fit |
| Toggle | `RAlt + Enter` at runtime | Switches between the above two modes |

**Effective font sizes on common displays** (approximate, assuming fullscreen).
Each logical-pixel column shows `physicalW × physicalH` per logical pixel,
computed as `displayW / 640` and `displayH / 480`. The glyph columns multiply
the base glyph dimension (e.g., `11 × textwidth`) by those ratios; values are
rounded to the nearest integer. With `scalefilter = 0` (nearest), the actual
on-screen pixels match these to within ±1 because rounding happens in the GPU
at sample time; with `scalefilter = 1` (linear), the visible glyph is the same
size but interpolated.

| Display | Resolution | Logical 1 px ≈ | 11 px glyph ≈ | 19 px glyph ≈ |
| ------------ | ---------- | --------------- | ------------- | ------------- |
| SD / CRT | 640 × 480 | 1.0 px | 11 px | 19 px |
| 720p | 1280 × 720 | 2.0 × 1.5 px | 22 × 17 px | 38 × 29 px |
| 1080p | 1920 × 1080 | 3.0 × 2.25 px | 33 × 25 px | 57 × 43 px |
| 1440p | 2560 × 1440 | 4.0 × 3.0 px | 44 × 33 px | 76 × 57 px |
| 4K | 3840 × 2160 | 6.0 × 4.5 px | 66 × 50 px | 114 × 86 px |

### Screen Coordinates

All coordinates below are in the 640 × 480 logical pixel space.

| Property | Value |
| --------------- | --------- |
| Internal buffer | 640 × 480 |
| Color depth | 8-bit (indexed) |
| Origin | Top-left (0, 0) |

### Lobby Screen Panels

The lobby is laid out in three regions on the left column plus a Game List on
the right, with header and footer text spanning the full width.

| Region | Position `(x, y)` | Size `W × H` | Contents |
| ----------------- | ----------------- | ------------ | --------- |
| Title             | `(15, 32)`        | text         | `"zSilencer"`, font bank 135 / advance 11, color **152** (dark red) |
| Version label     | `(115, 39)`       | text         | `"v.00024"`, font bank 133 / advance 6, color **189** (orange) |
| Character panel   | `(10, 64)`        | `217 × 120`  | Username text at `(20, 71)` 133/6; agency toggles starting at `y=90` with `+42 px` horizontal spacing; level / wins / etc. at `y=130..169` |
| Chat panel        | `(15, 216)`       | `368 × 234`  | Messages TextBox `(19, 220) 242 × 207`, lineheight 11, fontwidth 6; Presence TextBox `(267, 220) 110 × 207`; Chat TextInput `(19, 437) 360 × 14` |
| Game List         | `(403, 87)`       | `222 × 267`  | SelectBox `(407, 89) 214 × 265` lineheight 13; Join button at `(405, 361)` `B112x33`; Create button at `(518, 361)` `B112x33` |
| Footer            | `(10, 463)`       | text         | Version, font bank 133 / advance 6 |

Origin is top-left `(0, 0)`. All measurements are in the 640 × 480 logical
pixel space.

### In-Game HUD

| Element | Position | Notes |
| ------------------- | ------------------- | -------------------------------------------- |
| Minimap | (235, 419) | 172 × 62 px, bordered by sprite bank 94 |
| Health bar | Sprite-offset-based | Fills bottom-up proportional to HP |
| Shield bar | Sprite-offset-based | Fills bottom-up proportional to shield |
| Fuel bar | Sprite-offset-based | Fills left-to-right proportional to fuel |
| Team panel | (5, 5+) | 20 px per team row |
| Chat overlay | (400, 280) 231×30 | 9-slice background, 10 px line spacing |
| Buy menu | Sprite bank 102 | 5 visible items, 25 px per row |
| Status messages | centered, y=370 | Stack upward (y -= 10 per line) |
| Top message | (200, 10) | 133/7, max 35 chars |

### Common Spacing Values

| Metric | Value | Context |
| -------------------- | ------- | ---------------------------------------- |
| Chat line height | 10 px | In-game chat overlay |
| TextBox line height | 11 px | Lobby chat, text boxes |
| SelectBox line height | 13 px | Game/map lists |
| Buy-menu row height | 25 px | In-game buy interface |
| Player-list row | 12 px | Player list per player |
| Team row height | 58 px | Player list per team block |
| Team HUD row | 20 px | Team indicator rows |
| Status line spacing | 10 px | Status messages (bottom-up) |
| Message line height | 20 px | In-game announcements |
| Win/loss gap line 1→2 | 40 px | Win/loss first to second line |
| Agency toggle spacing | 42 px | Horizontal between agency icons |
| Game options row | 18 px | Game-create form vertical spacing |
| Label-to-input gap | ~85 px | Form field alignment (x=245 → x=350) |

---

## Visual Effects

### Sprite Transformations

The first four effects are documented in detail under
[Color System](#color-system) (brightness, color tint, alpha blend, team color
decoding). The remaining effects are pixel-by-pixel sprite overlays composited
onto the destination surface.

| Effect | Function | Description |
| --------------------- | ---------------------- | --------------------------------------------------- |
| Brightness | `EffectBrightness()` | Linear lerp toward white (>128) or black (<128). See [Brightness Transform](#brightness-transform). |
| Color Tint | `EffectColor()` | Luminance-preserving recolor. See [Color Tint Transform](#color-tint-transform). |
| Alpha Blend | `Alpha()` / `DrawAlphaed()` | Per-pixel alpha blend with destination. See [Alpha Blend Transform](#alpha-blend-transform). |
| Team Color | `EffectTeamColor()` | Decodes packed `(brightness<<4) \| hue` byte; recolors palette ranges 81–92 and 195–208 (or ≥130 for robots) via `EffectColor` then `EffectBrightness`. |
| Ramp Color | `EffectRampColor()` | For each non-zero pixel, computes a new palette index by combining the source pixel's brightness level (its position within its 16-color ramp) with the target color's group: `output = ((src - 2) % 16) + (((tgt - 2) / 16) * 16) + 2`. Snaps to the same ramp brightness in a different color group. |
| Ramp Color Plus | `EffectRampColorPlus()` | Same as Ramp Color but adds a minimum-brightness floor: if the resulting brightness level would be below `min`, it is bumped up to `min` (clamped to 15). Used for "highlighted" ramp tints. |
| Checkered | `DrawCheckered()` | Every-other-pixel transparency. Effective 50% transparency without true alpha. |
| Hit Flash | `EffectHit()` | Plays sprite bank **153** (8-frame ping-pong hit-flash animation) at impact location, applies `EffectRampColor` with a damage-type code: 146 (health-only damage), 194 (shield-only damage), 210 (poison damage). If shield was hit, also runs `EffectShieldDamage` on the same surface. |
| Shield Damage | `EffectShieldDamage()` | Stencil overlay using sprite bank **177** (8 frames cycling every 8 ticks). Where the stencil is opaque AND the destination has a non-transparent pixel, applies `RampColorMin` with color 205 (`#1C60B0`). Visual: blue checkerboard flash on the silhouette. |
| Warp | `EffectWarp()` | Vertical-displacement effect using sprite bank 153 frame 7 as the displacement envelope, offset by `(state_warp - 8) * 12` px (reversed for `state_warp >= 12`). Where source and offset overlap, output is forced to mid-gray (palette 128); non-overlapping transparent pixels are zeroed. |
| Hacking | `EffectHacking()` | Glitch overlay using sprite bank **178** (8 animated glitch frames, one per tick). A random `(ex, ey)` offset regenerates every 8 ticks. Where the overlay is opaque AND the destination has a pixel, applies `RampColorMin` with color 190 (ally) or 124 (enemy). |

### Button Hover Animation

```
State:       INACTIVE → ACTIVATING (0-4) → ACTIVE → DEACTIVATING (0-4) → INACTIVE
Brightness:  128        128,130,132,134,136  136     136,134,132,130,128  128
Sound:       —          "whoom.wav" @i=0     —       —                    —
```

Each frame increments brightness by 2: `effectbrightness = 128 + (state_i * 2)`.
At brightness 128, RGB output is unchanged. At 136, output is
`ch * 0.9375 + 255 * 0.0625` — a subtle lightening (roughly +16 to each channel
for mid-tones).

### Text Shadow (DrawMessage)

Announcement text draws twice: once at `(x+1, y+1)` as a shadow with
`brightness = max(original_brightness - 64, 8)`, then at `(x, y)` at the original
brightness. This produces a 1 px drop shadow. For text at the default brightness
of 128, the shadow renders at brightness 64 (`ch * 0.5` — half-dark).

### Caret Blink

Text-input caret blinks on a 32-tick cycle: visible for ticks 0–15,
hidden for ticks 16–31 (`state_i % 32 < 16`).

### Message Brightness Animation

In-game announcements pulse brightness using a triangular wave:
`brightness += (i % 16) * 2` or `(16 - i % 16) * 2` alternating, with a
fade-out of `-8 per tick` after the display timer expires. Leading characters
get an additional `+40 - (distance * 8)` brightness boost for a
"typewriter reveal" effect.

---

## UI Sounds

There is only **one UI sound**: `whoom.wav`. It is played in three contexts,
all by the Interface input dispatcher:

| Trigger                                           | Source            |
| ------------------------------------------------- | ----------------- |
| Button enters `ACTIVATING` (mouse hover or focus) | `button.cpp:52`   |
| Item selected via mouse click in a SelectBox      | `interface.cpp:608` |
| Enter pressed in a focused TextInput              | `interface.cpp:651` |
| Scrollbar nudge from keyboard nav past viewport   | (also `whoom.wav`) |

No volume parameter is passed (default mix volume). There are no separate
sounds for: error / modal-open, toggle change, scrollbar arrow click, focus
change between fields, or text input keypresses. A reimplementation can match
the original by playing the same one-shot on these four events.

---

## Effect-Color Reference

The following palette indices appear in component pseudocode but warrant
extra cross-referencing because they are reused across contexts or are
specific to effect overlays. All RGB values come from Palette 0.

| Index | RGB / Hex | Used in |
| ----- | --------- | ------- |
| 114 | `#000000` | Hack progress (incomplete); secret-carrier indicator. Also marks the boundary between the lower and upper palette ramps. |
| 124 | (varies) | `EffectHacking` overlay color when hacking an enemy target. |
| 140 | `#FCFC00` | TextInput caret. |
| 146 | `#0C0404` | `EffectHit` damage flash for health-only damage. |
| 153 | (sprite bank) | Hit-flash animation frames for `EffectHit`. |
| 177 | (sprite bank) | Stencil frames for `EffectShieldDamage`. |
| 178 | (sprite bank) | Glitch overlay frames for `EffectHacking`. |
| 180 | mid-gray ramp | SelectBox selected-row background. |
| 190 | (varies) | `EffectHacking` overlay color when hacking an ally target. |
| 192 | `#FCC480` | Secret-dropped announcement; also minimap pickup marker. |
| 194 | `#000018` | `EffectHit` damage flash for shield-only damage. |
| 205 | `#1C60B0` | `EffectShieldDamage` stencil tint. |
| 210 | `#001800` | `EffectHit` poison-damage flash; also "player in base" indicator on the Team HUD. |

Indices 153, 177, and 178 are **sprite bank numbers** for animated effect
overlays, not palette indices — they appear here for cross-referencing with the
[Visual Effects](#visual-effects) table.

---

## Source File Reference

| File | Contents |
| ------------------- | ------------------------------------------------- |
| `src/renderer.cpp` | All Draw* functions, effects, HUD rendering, buy menu, chat overlay, player list, modal dialog rendering |
| `src/renderer.h` | Renderer class, drawing API surface |
| `src/resources.cpp` | Asset loading: BIN_SPR.DAT / SPR_NNN.BIN format, RLE codec, font bank loading, palette assignment |
| `src/resources.h` | `spritewidth`, `spriteheight`, `spriteoffsetx`, `spriteoffsety` arrays referenced throughout |
| `src/palette.cpp` | Palette loading (PALETTE.BIN), lookup-table calculation, brightness/color/alpha transforms |
| `src/palette.h` | Palette class, inline color/brightness transforms |
| `src/button.cpp` | Button types, sizing, animation state machine, sound trigger |
| `src/overlay.cpp` | Overlay defaults, sprite animations, text hit-testing |
| `src/textinput.cpp` | Text field defaults, caret, input handling, scrolling, intentionally limited editing |
| `src/textbox.cpp` | Multi-line text area, word-wrap, line storage format |
| `src/selectbox.cpp` | List selection, item management, file listing |
| `src/scrollbar.cpp` | Scroll bar hit regions, up/down logic |
| `src/toggle.cpp` | Toggle visual states, checkbox/agency modes |
| `src/interface.cpp` | Container/focus manager, tab order, keyboard/mouse dispatch, radio groups, modal handling |
| `src/sprite.h` | Base sprite properties: effectcolor, effectbrightness, draw flags |
| `src/sprite.cpp` | Bounding box calculation, nudge interpolation |
| `src/object.h` | Object base class (type, id, render flags) |
| `src/minimap.h` | Minimap pixel buffer (172 × 62) |
| `src/game.cpp` | UI construction (lobby, menus, options screens), loading bar, modal dialog factory, main loop / tick rate |
| `src/world.cpp` | Per-tick simulation entry point (`World::Tick`) |
| `src/team.cpp` | Team overlays, player name labels |
