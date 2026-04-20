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

- **File:** `data/PALETTE.BIN` (8,436 bytes)
- **Format:** 11 sub-palettes × (4-byte header + 256 × 3 bytes RGB), 6-bit color depth
  (values `<< 2` to expand to 8-bit)
- **Lookup tables** (`PALETTECALC{n}.BIN`): pre-computed 256 × 256 tables for brightness,
  color-tint, and alpha-blend transformations; auto-calculated and cached if missing

### Palette Index Ranges (Palette 0)

| Range | Purpose |
| --------- | ------------------------------------------------------------------- |
| 0–1 | Transparent / black (protected — never transformed) |
| 2–113 | Main color ramps: 7 groups × 16 brightness levels each |
| 114–225 | Upper palette (secondary / effect colors) |
| 226–255 | Parallax sky colors (dynamic — swapped from palettes 5–9 per map) |

### Color Ramp Formula

Within indices 2–113 the layout is:

```
index = (colorGroup * 16) + brightnessLevel + 2
```

- `colorGroup` = `(index - 2) / 16` (0–6)
- `brightnessLevel` = `(index - 2) % 16` (0 = darkest, 15 = brightest)

### Semantic UI Colors

These `effectcolor` values are used on text and overlays to tint sprites via the
palette's color-lookup table.

| Index | Semantic Name | Used For |
| ----- | ---------------------- | ------------------------------------------------ |
| 112 | Toggle Active | Agency toggles (selected/deselected via brightness) |
| 114 | Hack Incomplete | Hacking progress lines, secret-carrier indicator |
| 126 | Neutral Light | Object labels (ramp-color mode) |
| 128 | Deploy Message | Deploy/spawn announcement text |
| 129 | Info Tint | Map name, level, wins, losses, stats labels |
| 140 | Caret | Text-input cursor color |
| 146 | Health Damage | Damage-flash on health-only hits |
| 150 | Minimap Tint | Minimap icon brightness |
| 152 | Title Text | "zSilencer" title in lobby |
| 153 | Red Alert | Neutron activated, game lost, connection lost |
| 161 | Health Value | Health number on HUD |
| 189 | Version Label | Version string in lobby |
| 192 | Secret Dropped | Secret-dropped message |
| 194 | Shield Damage | Damage-flash on shield-only hits |
| 200 | User Info | User info text |
| 202 | Warm / Orange | Credits display, shield value text |
| 205 | Shield Stencil | Shield-damage visual overlay |
| 208 | Standard Message | Default in-game announcement color |
| 210 | Poison / Base Entry | Poison-damage flash, player-in-base indicator |
| 224 | Highlight / Beacon | Win message, secret-beacon indicator, flare plume |

### Brightness Levels

Brightness is an 8-bit value passed to `EffectBrightness()`:

| Value | Effect |
| ----- | -------------------------------- |
| 0 | Full black |
| 32 | Very dark (inactive toggle) |
| 64 | Dark (inactive text input) |
| 96 | Dim (incomplete hack text) |
| 128 | **Neutral** — no change |
| 136 | Slightly bright (chat, HUD text) |
| 144 | Bright (tech description) |
| 160 | Brighter (info labels) |
| 192 | Very bright |
| 255 | Full white |

### Team Colors

Encoded in a single byte: upper 4 bits = brightness, lower 4 bits = hue.
Decoded via `TeamColorToIndex()` using the palette color + brightness lookups against
a base index of 204.

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

### Screen

| Property | Value |
| --------------- | --------- |
| Internal buffer | 640 × 480 |
| Color depth | 8-bit (indexed) |
| Origin | Top-left (0, 0) |
| Mouse scaling | `(event.x / window.w) * 640` |

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
Brightness:  128        128 + (i*2)         136      136 - (i*2)          128
Sound:       —          "whoom.wav" @i=0    —        —                    —
```

### Text Shadow (DrawMessage)

Announcement text draws twice: once at `(x+1, y+1)` with `brightness - 64` (min 8),
then at `(x, y)` at full brightness. This produces a 1 px drop shadow.

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
