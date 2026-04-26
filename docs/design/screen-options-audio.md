# Screen — Options / Audio Options

**Source:** `clients/silencer/src/game.cpp::CreateOptionsAudioInterface`
(line 2597) plus the `OPTIONSAUDIO` branch of `Game::Tick` (line 1692).

The simplest options sub-screen — a single boolean ("Music") rendered
identically to the rows on
[options-display](screen-options-display.md), then Save/Cancel below.
No volume slider, no other settings; `Config::musicvolume` exists in
the config struct but has no UI to edit it.

## Activation

When `Game::state` enters `OPTIONSAUDIO`:

1. Destroy all live objects.
2. Build the interface (this doc).
3. Active sub-palette inherited from upstream — sub-palette 1 in
   practice (entry path: MAINMENU → OPTIONS → OPTIONSAUDIO).
4. Tick reads `Config::GetInstance().music` and sets the on/off
   overlay `res_index`es accordingly.
5. Once `FadedIn() == true` the screen is visually static.

## Composition

Identical structure to options-display, with a single row.

```
+-----------------------------------------------------------+
|              Audio Options                                |   title text, y=14
|                                                           |
|                  [B220x33 Music]    [Off][On]             |   row 0
|                                                           |
|              [B196x33 Save]  [B196x33 Cancel]             |   y=405
+-----------------------------------------------------------+
```

## Object list (in draw order)

| # | Type    | Properties |
| - | ------- | ---------- |
| 1 | Overlay | `res_bank=6, res_index=0` (full-screen plate). `(x, y) = (0, 0)`. |
| 2 | Overlay | text mode — `text="Audio Options"`, `textbank=135`, `textwidth=12`, `x = 320 - (13 * 12) / 2 = 242`, `y=14`. |
| 3 | Button  | `B220x33`, `text="Music"`, `(x, y) = (100, 50)`, `uid=0`. Tab-focusable. |
| 4 | Overlay | "Off" half of toggle. `res_bank=6`, `res_index=12` (dim, since music defaults to ON). `(x, y) = (420, 137)`. `uid=20`. |
| 5 | Overlay | "On" half of toggle. `res_bank=6`, `res_index=15` (bright). `(x, y) = (450, 137)`. `uid=40`. |
| 6 | Button  | `B196x33`, `text="Save"`, `(x, y) = (-200, 117)`, `uid=200`. Tab-focusable. |
| 7 | Button  | `B196x33`, `text="Cancel"`, `(x, y) = (20, 117)`, `uid=201`. Tab-focusable. |

(Total: **7 objects** — half of options-display because there's only
one toggle row.)

## Default visible content

| Setting | Default value | Off-overlay idx | On-overlay idx |
| ------- | ------------- | --------------- | -------------- |
| Music | `true` (ON) | 12 (dim) | 15 (bright) |

## Focus / input wiring

- `tabobjects` order: Music, Save, Cancel.
- `activeobject = 0`.
- `buttonenter = savebutton.id`.
- `buttonescape = cancelbutton.id`.

## QA dump

```
SILENCER_DUMP_STATE=OPTIONSAUDIO \
SILENCER_DUMP_PATH=/tmp/real_options_audio.ppm \
  ./Silencer.app/Contents/MacOS/Silencer
```

Navigates MAINMENU → OPTIONS → OPTIONSAUDIO by chained click
synthesis (MAINMENU.Options uid 2 → OPTIONS.Audio uid 3).

Hydration writes `${SILENCER_DUMP_DIR}/options_audio.ppm`.

## What this screen reuses

100% reuse — every component on this screen was already documented
when options-display landed. This doc exists as a minimal
composition reference, not because the spec needs new substrate.
