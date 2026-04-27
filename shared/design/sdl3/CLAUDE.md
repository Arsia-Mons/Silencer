# shared/design/sdl3 — design-system component library + screen hydrations

C++17 + SDL3 reference renderer for the Silencer design system, built only
from `docs/design/` + binary assets in `shared/assets/`. Hydrates 12
screens to 640x480 indexed-framebuffer PPM dumps that are byte-compared
against engine-captured references as the regression contract.

## Build

```
cd shared/design/sdl3
cmake -B build
cmake --build build
```

## Run (dump mode)

```
SILENCER_DUMP_DIR=/tmp/sdl3_dump \
  ./build/silencer_design /Users/hv/repos/Silencer/shared/assets
```

Defaults to `main_menu`. Set `SILENCER_DUMP_SCREEN=<name>` to pick:

```
main_menu  options  options_audio  options_display  options_controls
lobby_connect  lobby  lobby_gamecreate  lobby_gamejoin
lobby_gametech  lobby_gamesummary  updating
```

Each invocation writes `${SILENCER_DUMP_DIR}/screen_00.ppm` (binary P6).

## Layout

```
src/
├── palette.{h,cpp}      PALETTE.BIN decoder + brightness LUT
├── sprite.{h,cpp}       SpriteSet + Framebuffer + BlitSprite (RLE codec)
├── font.{h,cpp}         DrawText (per-glyph, brightness-aware)
│
├── components/          pure-render primitives — no state, no SDL types
│   ├── panel.{h,cpp}    sized sprite blit (backgrounds + chrome frames)
│   ├── header.{h,cpp}   LOBBY-family title bar + version + GoBack
│   ├── button.{h,cpp}   chrome + centered label, variant table
│   ├── character.{h,cpp}  CharacterInterface left panel
│   ├── chat.{h,cpp}     ChatInterface bottom-left panel
│   ├── gameselect.{h,cpp}  GameSelectInterface right panel
│   └── modal.{h,cpp}    GameCreate/Join/Tech right-pane variants
│
├── screens/             one Compose<Screen>(...) per file
│   ├── main_menu.cpp    options.cpp, options_audio.cpp, ...
│   ├── lobby.cpp        lobby_gamecreate.cpp, lobby_gamejoin.cpp, ...
│   └── demo_data.{h,cpp}  shared seed values for the LOBBY family
│
├── dump_runner.{h,cpp}  ScreenSpec + RunScreenDump shared scaffolding
├── screens.h            forward decls for every Compose<Screen>
└── main.cpp             ~80-line registry + argv/env dispatcher
```

## Boundaries

- **Components are pure functions** of `(Framebuffer&, SpriteSet&,
  [Palette&, int active_sub,] View&)`. Plain data views — no SDL types
  cross the boundary, no input/state/animation logic.
- **Screens compose components**. `screens/<name>.cpp` is the only place
  per-screen layout decisions live.
- **`DumpRunner` owns the lifecycle**: SDL_Init → load palette/banks →
  fill background → compose → write PPM → SDL_Quit.
- **`main.cpp` is just a registry + dispatcher** — keep it under 100
  lines.

This shape positions the component layer as a future call target for
`clients/silencer/`'s widget classes (e.g.
`Button::Draw()` → `silencer::RenderButton(fb, sprites, font, ToButtonView(*this))`).
That migration is out of scope here; the design protects it by keeping
SDL3 runtime types out of component interfaces.

## Regression contract

The PPMs *are* the test. Workflow:

1. Snapshot every reference PPM with the current binary.
2. Make a change.
3. Re-dump every screen, `cmp`-compare to the snapshot.

Zero diff is required. A single byte change is a regression to root-cause
(usually a missing brightness/sub-palette thread-through or a forgotten
bank in a `ScreenSpec`).

A scripted version of step 3 lives in commit history as
`/tmp/verify_sdl3_dump.sh` — easy to recreate when needed.

## What's faithful

- `PALETTE.BIN` decoder: per-sub offset `4 + s * 772`, 6-bit -> 8-bit,
  zero-filled past EOF (matches engine over-read).
- Sprite RLE codec: linear (mode 0) and tile (any non-zero mode, 64x64
  tiles, 4-pixel chunks).
- BIN_SPR.DAT 16,384-byte index, count at byte offset 2 of each
  64-byte record.
- Anchor convention: `top_left = object - sprite.offset`.
- Steady-state values per screen (all buttons INACTIVE, brightness=128;
  logo bank-208 ticked to the hold frame at `res_index = 60`; demo
  lobby data captured from `services/lobby -demo`).

## What's faked / skipped

- No mouse/keyboard input. Each screen renders one static frame.
- Brightness LUT is built lazily — INACTIVE buttons (brightness=128)
  pass through identity.
- No alpha-blend LUT (transparent pixels skipped at blit time).
- No window/event loop in dump mode — straight to PPM and exit.
- `effectcolor` (palette-tint) values lit in the spec aren't yet
  modelled by `DrawText`; current screens render with brightness=128
  only and still match the engine references.
