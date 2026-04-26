# shared/design/sdl3

Standalone SDL3 (C++17) hydration of the Silencer UI design system. Renders
each widget and a few representative screen compositions from
`docs/design-system.md` against the original 8-bit indexed assets in
`shared/assets/` so the spec is reviewable end-to-end.

## Sources

- `docs/design-system.md` is the only behavioral spec.
- `shared/assets/PALETTE.BIN`, `BIN_SPR.DAT`, `bin_spr/SPR_NNN.BIN` are the
  authoritative pixel content. The loader in `src/sprite.cpp` implements the
  RLE codec (linear + 64x64 tile-ordered) documented in §Asset Formats.
- This program intentionally does **not** read anything from
  `clients/silencer/` — it must stand on the spec alone.

## Build

```
cmake -B build
cmake --build build
```

Requires SDL3 (`brew install sdl3` on macOS, `apt install libsdl3-dev` on
recent Debian/Ubuntu, or build from source).

## Run

```
./build/silencer_design [path/to/shared/assets]
```

Default asset path is `../../assets/` relative to the executable, which
resolves to the in-repo assets when running from `build/`.

## Controls

| Key            | Action                                            |
| -------------- | ------------------------------------------------- |
| Left / Right   | Previous / next demo screen                       |
| 1..9, 0        | Jump to numbered screen                           |
| Tab            | Cycle focus inside the active screen's Interface  |
| Enter / Esc    | Forwarded to `buttonenter` / `buttonescape`       |
| F              | Toggle fullscreen                                 |
| Q              | Quit                                              |

Mouse + text input go to the active screen.

## Demo screens

1. Palette swatches — 7 ramp groups + semantic colors
2. Typography — font banks 132..136 with their advance widths
3. Buttons — all 7 variants with 4-tick activation animation
4. Text inputs + multi-line TextBox (chat-style)
5. SelectBox + ScrollBar
6. Toggles (agency icons radio set + checkboxes) + Overlay text
7. Horizontal-stretch panel (chat background, sprite bank 188)
8. Modal Dialog (sprite bank 40 idx 4 + OK button)
9. Loading Bar
10. In-game HUD composition (bars, minimap, chat overlay, top message)
11. Minimap (paletted 172x62 buffer)
12. Main menu composition
13. Lobby composition (header + character + chat + game-list panels)
14. Buy menu composition (sprite bank 102 + 25 px row pulse)

## Layout

```
src/
  main.cpp                  entry point + demo navigator
  palette.{h,cpp}           PALETTE.BIN loader; brightness/color tint helpers
  sprite.{h,cpp}            BIN_SPR.DAT + SPR_NNN.BIN loader, RLE codec, blit
  font.{h,cpp}              text drawing using sprite banks 132–136
  widgets/
    widget.h                shared base
    button.{h,cpp}          Button (7 variants, state machine)
    toggle.{h,cpp}          Toggle (agency + checkbox modes)
    textinput.{h,cpp}       TextInput (caret blink, scroll, no cursor keys)
    textbox.{h,cpp}         TextBox (multi-line, auto-scroll)
    selectbox.{h,cpp}       SelectBox (palette-180 highlight)
    scrollbar.{h,cpp}       ScrollBar (up/down arrows + thumb)
    overlay.{h,cpp}         Overlay (sprite or text label, sprite anims)
    interface.{h,cpp}       focus manager / keyboard+mouse dispatch
    panel.{h,cpp}           Horizontal-stretch (chat bg) panel
    modal.{h,cpp}           Modal dialog
    loadingbar.{h,cpp}      Loading bar
    hudbars.{h,cpp}         HUD bars + ammo/credits/value labels
    minimap.{h,cpp}         172x62 paletted minimap
    primitives.{h,cpp}      FilledRect / Line / Circle / Checkered
  screens/
    screen.h                Screen interface + factories
    screen_*.cpp            one per demo screen
```

## Faithfulness notes

- Logical surface is **640 x 480 8-bit indexed**, converted to RGBA each
  frame and presented through `SDL_LOGICAL_PRESENTATION_LETTERBOX` so the
  aspect ratio is preserved when the window is resized.
- 6-bit palette channels are expanded with `v << 2` per spec.
- Effect color / brightness LUTs are computed by applying the documented
  transforms then nearest-neighbour matching to a palette index. This is
  the same pipeline `EffectColor` + `EffectBrightness` describe.
- Simulation ticks at 23.8 Hz (42 ms); render loop is uncapped. The
  caret blink, button activation, and buy-menu pulse are all driven by
  the global `state_i` counter.

## Skipped / faked

- `EffectAlpha`, `EffectRampColor[Plus]`, `EffectTeamColor`,
  `EffectShieldDamage`, `EffectHacking`, `EffectWarp`, `EffectHit` are not
  implemented (they are gameplay effects, not UI chrome).
- The horizontal-stretch panel does not source-clip the tiled edge sprite
  (full tile is blitted; the right-edge corner still anchors at
  `x + w - 36`). Visually fine for the chat box width.
- `SelectBox` ScrollBar wiring is decorative — the demo's SelectBox
  manages its own `scrolled` field, but the ScrollBar widget is rendered
  next to it without two-way binding.
- Per-bank Overlay sprite animations (54, 57, 58, 171, 222) tick state
  but only a few are exercised on screen.
- The Lobby screen draws faux 1-px panel borders rather than rendering
  the lobby background plate (bank 7 idx 1) — consult the source asset
  via the typography or palette demo screens to see real bank pixels.
