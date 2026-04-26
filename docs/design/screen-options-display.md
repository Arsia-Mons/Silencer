# Screen — Options / Display Options

**Source:** `clients/silencer/src/game.cpp::CreateOptionsDisplayInterface`
(line 2536) plus the `OPTIONSDISPLAY` branch of `Game::Tick` (line 1602)
that updates the on/off-toggle overlay indices from `Config`.

A simpler companion to options-controls — just two boolean settings
(Fullscreen, Smooth Scaling), each rendered as a `B220x33` button
with a name label, plus a side-by-side On/Off toggle indicator pair
to the right. Save/Cancel below.

No inner panel sprite (bank-7 idx-7) on this screen — the rows
render directly on the main-menu cosmic plate.

## Activation

When `Game::state` enters `OPTIONSDISPLAY`:

1. Destroy all live objects.
2. Build the interface (this doc).
3. Active sub-palette is **inherited** — `OPTIONSDISPLAY` doesn't
   call `palette.SetPalette(...)`. In practice always sub-palette
   1 (entry path: MAINMENU → OPTIONS → OPTIONSDISPLAY).
4. The first Tick reads `Config::GetInstance().fullscreen` and
   `Config::GetInstance().scalefilter` and sets the on/off overlay
   `res_index`es accordingly (see
   [widget-overlay.md](widget-overlay.md) "On/off toggle pattern").
5. Once `FadedIn() == true` the screen is visually static.

## Composition

```
+-----------------------------------------------------------+   y=0
|                                                           |
|              Display Options                              |   title text, y=14
|                                                           |
|                                                           |
|                  [B220x33 Fullscreen]    [Off][On]        |   row 0
|                                                           |
|                  [B220x33 Smooth Scal..]  [Off][On]       |   row 1
|                                                           |
|                                                           |
|              [B196x33 Save]  [B196x33 Cancel]             |   y=405
|                                                           |
+-----------------------------------------------------------+   y=479
```

## Object list (in draw order)

| # | Type    | Properties |
| - | ------- | ---------- |
| 1 | Overlay | `res_bank=6, res_index=0` (full-screen plate). `(x, y) = (0, 0)`. |
| 2 | Overlay | text mode — `text="Display Options"`, `textbank=135`, `textwidth=12`, `x = 320 - (15 * 12) / 2 = 230`, `y=14`. |

Then for each `i = 0..1`:

| Sub-# | Type | Properties |
| ----- | ---- | ---------- |
| 3 + 3i | Button | `B220x33`, `text = ["Fullscreen", "Smooth Scaling"][i]`, `(x, y) = (100, 50 + i*53)`, `uid = i`. Tab-focusable. |
| 4 + 3i | Overlay (sprite) | "Off" half of the toggle. `res_bank=6`, `res_index=12` (dim, since both options default to ON). `(x, y) = (420, 137 + i*53)`. `uid = 20 + i`. |
| 5 + 3i | Overlay (sprite) | "On" half of the toggle. `res_bank=6`, `res_index=15` (bright). `(x, y) = (450, 137 + i*53)`. `uid = 40 + i`. |

Then:

| # | Type   | Properties |
| - | ------ | ---------- |
| 9 | Button | `B196x33`, `text="Save"`, `(x, y) = (-200, 117)`, `uid=200`. Tab-focusable. |
| 10 | Button | `B196x33`, `text="Cancel"`, `(x, y) = (20, 117)`, `uid=201`. Tab-focusable. |

(Total: 2 background/title overlays + 2 rows × 3 widgets + Save + Cancel = **10 objects**.)

## Default visible content

The hydration should mirror the real client's default `Config`
(`config.cpp:114..131`):

| Setting | Default value | Off-overlay idx | On-overlay idx |
| ------- | ------------- | --------------- | -------------- |
| Fullscreen | `true` (ON)  | 12 (dim) | 15 (bright) |
| Smooth Scaling | `true` (ON) | 12 (dim) | 15 (bright) |

Both rows render identically by index (only the row position and
button label differ).

## Focus / input wiring

- `tabobjects` order: Fullscreen, Smooth Scaling, Save, Cancel.
- `activeobject = 0` (no focus on entry).
- `buttonenter = savebutton.id` — Enter clicks Save. (Tick code
  re-points it to a focused option button on hover; for a
  static-frame hydration this can stay pinned to Save.)
- `buttonescape = cancelbutton.id` — Escape clicks Cancel.

## QA dump

```
SILENCER_DUMP_STATE=OPTIONSDISPLAY \
SILENCER_DUMP_PATH=/tmp/real_options_display.ppm \
  ./Silencer.app/Contents/MacOS/Silencer
```

Navigates MAINMENU → OPTIONS → OPTIONSDISPLAY by chained click
synthesis (uid 2 then uid 2 again — yes, both `Display` in OPTIONS
and `Options` in MAINMENU happen to be uid 2; the binary tracks
this via `target_state` rather than uid alone).

Hydration writes `${SILENCER_DUMP_DIR}/options_display.ppm`.

## What this screen reuses

- [palette.md](palette.md) — sub-palette 1
- [sprite-banks.md](sprite-banks.md) — bank 6 idx 0 plate, idx 12-15 toggle indicators, idx 23-27 B220x33 frames
- [font.md](font.md) — bank 135 button labels + title
- [widget-overlay.md](widget-overlay.md) — sprite Overlay (toggles) and text Overlay (title)
- [widget-button.md](widget-button.md) — `B220x33`, `B196x33`
- [widget-interface.md](widget-interface.md) — focus, Tab order, button_enter/button_escape

The only new substrate this screen adds is `B220x33` (covered in
widget-button.md).
