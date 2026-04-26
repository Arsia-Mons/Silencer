# Screen — Main Menu

**Source:** `clients/silencer/src/game.cpp::CreateMainMenuInterface`
(line 2266) plus the MAINMENU branch of `Game::Tick`
(line 484) and `Game::ProcessMainMenuInterface` (line 4004).

## Activation

When `Game::state` enters `MAINMENU`:

1. Disconnect any active world / lobby session, destroy all
   objects.
2. Build the menu interface (this doc).
3. Camera position → `(320, 240)`.
4. **Active palette → sub-palette 1** (`renderer.palette.SetPalette(1)`).
   See [palette.md](palette.md) — this is the most-missed step.
5. Background music (`menumusic`) plays once `FadedIn()` is true.

## Composition

Logical surface is **640 × 480 8-bit indexed**. Camera at
`(320, 240)` so screen coords == world coords.

```
+-----------------------------------------------------------+   y=0
|                                                           |
|  [bank-6 idx-0 fullscreen background plate]               |   the cosmic / planet plate
|                                                           |
|                                                           |
|       [bank-208 logo (animated, idx 29..60)]              |   y≈170..260, x≈0..348
|                                                           |
|                                                           |
|                                       [B196x33 Tutorial]  |   pill, right side
|                                                           |
|                                       [B196x33 Connect…]  |
|                                                           |
|                                       [B196x33 Options]   |
|                                                           |
|                                       [B196x33 Exit]      |
|                                                           |
|  Silencer v<VERSION>                                      |   text bank 133, bottom-left
+-----------------------------------------------------------+   y=479
```

## Object list (in draw order)

| # | Type    | Properties |
| - | ------- | ---------- |
| 1 | Overlay | `res_bank=6, res_index=0` (full-screen background plate). `x=0, y=0`. No effects. |
| 2 | Overlay | `res_bank=208`, `res_index` ticked (animation; see [widget-overlay.md](widget-overlay.md)). `x=0, y=0`. |
| 3 | Overlay | text mode — `text = "Silencer v" + version_string`, `textbank=133`, `textwidth=11`, `x=10`, `y = 480 - 10 - 7 = 463`. The `version_string` is whatever the build was tagged with (real client passes `-DSILENCER_VERSION=...` at compile time and reads `world.version`); for hydrations producing a comparison dump, hardcode the same value the real client was built with so the bottom-left text matches character-for-character. The current default is `00028`. |
| 4 | Button  | `B196x33`, `text="Tutorial"`, `x=40, y=-134`, `uid=0`. |
| 5 | Button  | `B196x33`, `text="Connect To Lobby"`, `x=80, y=-67`, `uid=1`. |
| 6 | Button  | `B196x33`, `text="Options"`, `x=40, y=0`, `uid=2`. |
| 7 | Button  | `B196x33`, `text="Exit"`, `x=0, y=67`, `uid=3`. |

Note the buttons' `y` values are spaced **67 px apart** vertically
(roughly 2× the button height), and `x` zig-zags `40 / 80 / 40 / 0`
giving a slight left-stagger on rows 1 and 3. The `(x + 310, y +
288)` mapping (sprite anchor offset, see
[sprite-banks.md](sprite-banks.md)) places these on the right side
of the framebuffer:

| Button | Anchor `(x, y)` | Rendered top-left `(x + 310, y + 288)` |
| ------ | --------------- | -------------------------------------- |
| Tutorial         | `(40, -134)` | `(350, 154)` |
| Connect To Lobby | `(80, -67)`  | `(390, 221)` |
| Options          | `(40, 0)`    | `(350, 288)` |
| Exit             | `(0, 67)`    | `(310, 355)` |

Each button's footprint is `196 × 33`.

## Focus / input wiring

- `tabobjects = [Tutorial, Connect To Lobby, Options, Exit]`
  (Tab cycles in this order).
- `activeobject = 0` initially — `0` is the "nothing focused" sentinel (object IDs start at `1`); the menu opens with no button highlighted. See [widget-interface.md](widget-interface.md).
- `buttonenter = 0` — Enter has no menu-level binding. Pressing
  Enter while a button is the `activeobject` clicks that button
  via the generic `Interface::EnterPressed` fallback.
- `buttonescape = exitbutton.id` — Escape clicks Exit.

## Click handlers (out of scope for visual hydration)

`Game::ProcessMainMenuInterface` (`game.cpp:4004`) routes each
button's `clicked` flag to a state transition:

- Tutorial → `SINGLEPLAYERGAME` with the tutorial map.
- Connect To Lobby → `LOBBYCONNECT`.
- Options → `OPTIONS`.
- Exit → returns `false` from `Loop`, exits the program.

A hydration only verifying visual fidelity can stop at "render once
and dump"; it doesn't need to implement these transitions.

## QA dump

`clients/silencer` has an env-gated framebuffer dump in
`Game::Present` (`game.cpp` near line 231). To capture a
ground-truth PPM of the main menu:

```
SILENCER_DUMP_PATH=/tmp/silencer_main_menu.ppm \
  ./Silencer.app/Contents/MacOS/Silencer
```

`shared/design/sdl3` has the equivalent for hydrations:

```
SILENCER_DUMP_DIR=/tmp/sdl3_dump \
  ./silencer_design ../../../assets
```

Visual A/B between the two PPMs is the validation gate.
