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
3. [Components](#components)
4. [Layout & Spacing](#layout--spacing)
5. [Visual Effects](#visual-effects)

---

## Typography

All text is rendered from bitmap glyph sprite banks. There are no TrueType or vector
fonts — each "font" is a sprite bank containing one glyph per printable ASCII character
(starting at ASCII 33 `!`, or 34 `"` for bank 132).

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
| `DrawText()` | `renderer.cpp:1443` | Core glyph renderer (bank, width, color, alpha) |
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
- **Format:** 11 sub-palettes × (4-byte header + 256 × 3 bytes RGB), 6-bit color depth
  (raw values 0–63, shifted `<< 2` to 8-bit, giving an effective max of 252 per channel)
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

**Group 1 — Fire/Yellow (indices 18–33)**

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

## Components

### Button

Defined in `button.h` / `button.cpp`. Seven types:

| Type | Size (W × H) | Res Bank | Text Bank | Text Width | Text Y-Offset |
| -------------- | ------------- | -------- | --------- | ---------- | ------------- |
| `B112x33` | 112 × 33 | 6 | 135 | 11 | 8 |
| `B196x33` | 196 × 33 | 6 | 135 | 11 | 8 |
| `B220x33` | 220 × 33 | 6 | 135 | 11 | 8 |
| `B236x27` | 236 × 27 | 6 | 135 | 11 | 8 |
| `B52x21`       | 52 × 21       | —        | 133       | 7          | y=8, x+=1     |
| `B156x21` | 156 × 21 | 7 | 134 | 8 | 4 |
| `BCHECKBOX` | 13 × 13 | 7 | — | — | — |

**Text centering:** `x = (width - charCount * textwidth) / 2`

**Animation:** 5-frame hover animation: brightness ramps from 128 → 136 (`128 + state_i * 2`,
`state_i` 0–4). `B156x21` animates brightness only (no sprite index change).

**States:** `INACTIVE → ACTIVATING → ACTIVE → DEACTIVATING → INACTIVE`

### Toggle

Defined in `toggle.h` / `toggle.cpp`. Used for agency selection, checkbox-style picks.

| Property | Selected | Deselected |
| --------- | -------------------- | --------------------- |
| `effectcolor` | 112 (bank 181) | 112 (bank 181) |
| `effectbrightness` | 128 | 32 |
| Checkbox (bank 7) | `res_index = 18` | `res_index = 19` |

Width and height are dynamic — read from the sprite dimensions at runtime.

### TextInput

Defined in `textinput.h` / `textinput.cpp`.

| Property | Default | Notes |
| --------------- | ------- | ----------------------------------------- |
| `res_bank` | 135 | Font bank for rendering |
| `fontwidth` | 9 | Advance per character |
| `maxchars` | 256 | Maximum buffer size |
| `maxwidth` | 10 | Visible character slots before scrolling |
| `caretcolor` | 140 | Palette index for blinking caret |
| `password` | false | Renders `*` per character |
| `numbersonly` | false | Restricts to digits 0–9 |
| `inactive` | false | Dims to brightness 64 |

**Caret:** 1 px wide, height = `field_height * 0.8`, blinks every 16 ticks
(`state_i % 32 < 16`).

Common field sizes set in `game.cpp`:

| Field | Width × Height | Font Width | Max Chars |
| --------------- | -------------- | ---------- | --------- |
| Username | 180 × 14 | 6 | — |
| Password | 180 × 14 | 6 | — |
| Chat | 360 × 14 | 6 | 60 |
| Game Name | 210 × 14 | 6 | — |
| Small (numeric) | 20 × 20 | 8 | — |

### TextBox

Defined in `textbox.h` / `textbox.cpp`. Scrollable multi-line text area.

| Property | Default | Notes |
| -------------- | ------- | ---------------------------------------- |
| `res_bank` | 133 | Font bank |
| `lineheight` | 11 | Pixels per line |
| `fontwidth` | 6 | Character advance |
| `width` | 100 | Viewport width in pixels |
| `height` | 100 | Viewport height in pixels |
| `maxlines` | 256 | Max buffered lines |
| `bottomtotop` | false | Render direction |

Auto-scrolls to the bottom whenever a new line is added and the content exceeds
`height / lineheight` visible lines. Scroll offset: `lines.size() - visibleLines`.

### SelectBox

Defined in `selectbox.h` / `selectbox.cpp`. Single-selection list.

| Property | Default | Notes |
| ------------- | ------- | ---------------------------------- |
| `lineheight` | 13 | Pixels per item row |
| `maxlines` | 256 | Max items |
| Hit area | `width - 16` | 16 px reserved for scrollbar |

### ScrollBar

Defined in `scrollbar.h` / `scrollbar.cpp`.

| Property | Default | Notes |
| --------------- | ------- | --------------------------------- |
| `res_bank` | 7 | Sprite bank for scrollbar frame |
| `res_index` | 9 | Main scrollbar sprite |
| `barres_index` | 10 | Scroll thumb sprite |
| Up/Down button | 16 px | Hit area height for arrow buttons |

### Overlay

Defined in `overlay.h` / `overlay.cpp`. Generic sprite or text label.

| Property | Default | Notes |
| -------------------- | ------- | -------------------------------------- |
| `textbank` | 135 | Font bank for text overlays |
| `textwidth` | 8 | Character advance |
| `textlineheight` | 10 | Line height for multi-line text |
| `textcolorramp` | false | Use ramp-color vs standard color |
| `textallownewline` | false | Allow `\n` in text |
| `drawalpha` | false | Alpha-blend rendering |

### Chat Message Background

Rendered by `DrawMessageBackground()` using sprite bank **188** (9-slice panel):

| Index | Part |
| ----- | ------------ |
| 0 | Top-left |
| 1 | Top (tiled) |
| 2 | Top-right |
| 3 | Left |
| 4 | Center |
| 5 | Right |
| 6 | Bottom-left |
| 7 | Bottom (tiled) |
| 8 | Bottom-right |

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

**Effective font sizes on common displays** (approximate, assuming fullscreen;
fractional pixels rounded to nearest integer):

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

```
┌─────────────────────────────────────────────────────────────────────┐
│ "zSilencer" (135/11, color 152) @ (15, 32)                        │
│ "v.00024"   (133/6,  color 189) @ (115, 39)                       │
├──────────────────────────────────────┬──────────────────────────────┤
│ Character Panel (10, 64) 217×120     │ Game List (403, 87) 222×267 │
│ ┌─ user text (20, 71) 133/6         │ ┌─ SelectBox (407, 89)      │
│ │  agency toggles @ y=90, x+=42     │ │  214×265, lineheight=13   │
│ │  level/wins/etc @ y=130..169      │ ├─ Join (405, 361) B112x33  │
│ └────────────────────────────────────│ └─ Create (518, 361) B112  │
├──────────────────────────────────────┤                              │
│ Chat Panel (15, 216) 368×234         │                              │
│ ┌─ Messages (19, 220) 242×207       │                              │
│ │  lineheight=11, fontwidth=6        │                              │
│ ├─ Presence (267, 220) 110×207      │                              │
│ └─ Input    (19, 437) 360×14        │                              │
├──────────────────────────────────────┴──────────────────────────────┤
│ Version: (10, 463) 133/6                                           │
└─────────────────────────────────────────────────────────────────────┘
```

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

| Effect | Function | Description |
| --------------------- | ---------------------- | --------------------------------------------------- |
| Brightness | `EffectBrightness()` | Shifts all pixels via brightness lookup table |
| Color Tint | `EffectColor()` | Luminance-preserving color overlay |
| Ramp Color | `EffectRampColor()` | Recolors within 16-color ramp bands |
| Ramp Color Plus | `EffectRampColorPlus()` | Ramp with additive brightness offset |
| Alpha Blend | `DrawAlphaed()` | Per-pixel alpha blend with destination |
| Checkered | `DrawCheckered()` | Every-other-pixel transparency |
| Team Color | `EffectTeamColor()` | Applies team hue + brightness |
| Hit Flash | `EffectHit()` | Damage indicator (health/shield/poison) |
| Shield Damage | `EffectShieldDamage()` | Shield-specific damage overlay (color 205) |
| Warp | `EffectWarp()` | Warping visual distortion |
| Hacking | `EffectHacking()` | Hacking-state visual overlay |

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

## Source File Reference

| File | Contents |
| ------------------- | ------------------------------------------------- |
| `src/renderer.cpp` | All Draw* functions, effects, HUD rendering |
| `src/renderer.h` | Renderer class, drawing API surface |
| `src/palette.cpp` | Palette loading, lookup-table calculation |
| `src/palette.h` | Palette class, inline color/brightness transforms |
| `src/button.cpp` | Button types, sizing, animation |
| `src/overlay.cpp` | Overlay defaults, text hit-testing |
| `src/textinput.cpp` | Text field defaults, caret, input handling |
| `src/textbox.cpp` | Multi-line text area, word-wrap, scrolling |
| `src/selectbox.cpp` | List selection, item management |
| `src/scrollbar.cpp` | Scroll bar hit areas, scrolling |
| `src/toggle.cpp` | Toggle/checkbox selected/deselected states |
| `src/sprite.h` | Base sprite: effectcolor, effectbrightness |
| `src/game.cpp` | UI construction (lobby, menus, options screens) |
| `src/team.cpp` | Team overlays, player name labels |
