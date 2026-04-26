# Screen — Options

**Source:** `clients/silencer/src/game.cpp::CreateOptionsInterface`
(line 2352) plus the `OPTIONS` branch of `Game::Tick` (line 1391)
that routes button clicks to sub-states.

The Options menu is the parent screen reached via the main menu's
"Options" button. It's a vertical stack of four buttons over the
same cosmic-plate background — no logo, no version overlay, nothing
animated. From a spec point of view this is the simplest possible
screen extension: same components as the main menu, fewer of them,
a different layout.

## Activation

When `Game::state` enters `OPTIONS`:

1. Destroy all live objects (`world.DestroyAllObjects()`).
2. Build the options interface (this doc).
3. **Active sub-palette is unchanged.** Unlike the main menu, the
   `OPTIONS` branch doesn't call `palette.SetPalette(...)` at all.
   It inherits whichever sub-palette was active when the user
   transitioned in. In practice that's always sub-palette **1**
   because the only entry path to `OPTIONS` is via `MAINMENU →
   FADEOUT → OPTIONS`, and `MAINMENU` set palette 1 on its own
   entry. A hydration that wants to capture this screen standalone
   should set `SetActive(1)` explicitly.
4. No music change; whatever was playing keeps playing.

## Composition

```
+-----------------------------------------------------------+   y=0
|                                                           |
|                                                           |
|  [bank-6 idx-0 fullscreen background plate]               |
|                                                           |
|                                                           |
|              [B196x33 Controls]                           |   ~y=146
|                                                           |
|              [B196x33 Display]                            |   ~y=198
|                                                           |
|              [B196x33 Audio]                              |   ~y=250
|                                                           |
|              [B196x33 Go Back]                            |   ~y=303
|                                                           |
|                                                           |
+-----------------------------------------------------------+   y=479
```

## Object list (in draw order)

| # | Type    | Properties |
| - | ------- | ---------- |
| 1 | Overlay | `res_bank=6, res_index=0` (full-screen background plate). `x=0, y=0`. No effects. Same instance as on the main menu. |
| 2 | Button  | `B196x33`, `text="Controls"`, `x=-89, y=-142`, `uid=1`. |
| 3 | Button  | `B196x33`, `text="Display"`,  `x=-89, y=-90`,  `uid=2`. |
| 4 | Button  | `B196x33`, `text="Audio"`,    `x=-89, y=-38`,  `uid=3`. |
| 5 | Button  | `B196x33`, `text="Go Back"`,  `x=-89, y=15`,   `uid=0`. |

Buttons are spaced **52 px apart** vertically (closer than the main
menu's 67 px) and share the same `x = -89`. With the `B196x33`
sprite anchor offset of `(-310, -288)` (from
[sprite-banks.md](sprite-banks.md)), each button's rendered top-left
lands at `(x + 310, y + 288)`:

| Button   | Anchor `(x, y)` | Rendered top-left `(x + 310, y + 288)` |
| -------- | --------------- | --------------------------------------- |
| Controls | `(-89, -142)`   | `(221, 146)` |
| Display  | `(-89, -90)`    | `(221, 198)` |
| Audio    | `(-89, -38)`    | `(221, 250)` |
| Go Back  | `(-89, 15)`     | `(221, 303)` |

Each button's footprint is `196 × 33`.

## Focus / input wiring

- `tabobjects = [Controls, Display, Audio, Go Back]` (Tab cycles in
  this order — note the `Go Back` button is **last** in tab order
  even though it has `uid = 0`, because its tab-order position is
  determined by `AddTabObject` call order, not by `uid`).
- `activeobject = 0` initially — `0` is the "nothing focused"
  sentinel (see [widget-interface.md](widget-interface.md)).
- `buttonenter = 0` — Enter falls back to clicking the focused
  button via `Interface::EnterPressed`.
- `buttonescape = gobackbutton.id` — Escape clicks "Go Back".

## Click handlers (out of scope for visual hydration)

`Game::Tick` `OPTIONS` branch (`game.cpp:1391..1423`) routes each
button's `clicked` flag by `uid`:

| `uid` | Action |
| ----- | ------ |
| `0` (Go Back) | `GoToState(MAINMENU)` |
| `1` (Controls) | `GoToState(OPTIONSCONTROLS)` |
| `2` (Display)  | `GoToState(OPTIONSDISPLAY)` |
| `3` (Audio)    | `GoToState(OPTIONSAUDIO)` |

A hydration only verifying visual fidelity can stop at "render once
and dump"; transitions don't need to be implemented.

## QA dump

`clients/silencer`'s `Game::Present` accepts `SILENCER_DUMP_STATE`
as well as `SILENCER_DUMP_PATH`. To capture this screen specifically:

```
SILENCER_DUMP_STATE=OPTIONS \
SILENCER_DUMP_PATH=/tmp/real_options.ppm \
  ./Silencer.app/Contents/MacOS/Silencer
```

The binary navigates from MAINMENU → OPTIONS automatically by
synthesizing a click on the main menu's Options button (uid `2`)
once the main-menu logo reaches its steady-state frame. Once the
Options screen has faded in (`FadedIn() == true`), the dump fires
and the binary exits.

The hydration should accept the same `SILENCER_DUMP_STATE` selector
(values `MAINMENU`, `OPTIONS`) so the visual A/B can target both
screens with the same harness.

## What this screen reuses

Every component on this screen is already documented:

- [palette.md](palette.md) — sub-palette 1 active (inherited from MAINMENU)
- [sprite-banks.md](sprite-banks.md) — bank 6 idx 0 background, bank 6 idx 7+ button frames
- [font.md](font.md) — bank 135 button labels (advance 11)
- [widget-overlay.md](widget-overlay.md) — sprite-mode Overlay (bg only; no text mode here)
- [widget-button.md](widget-button.md) — `B196x33` only
- [widget-interface.md](widget-interface.md) — focus, Tab order, button_escape

The only new wrinkle vs. the main menu is the **inherited-palette**
behavior — Options doesn't call `SetPalette` itself, which means a
hydration capturing this screen in isolation must explicitly set
sub-palette 1 even though the spec page never mentions it as the
"options palette."
