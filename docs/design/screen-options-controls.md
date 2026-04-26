# Screen — Options / Configure Controls

**Source:** `clients/silencer/src/game.cpp::CreateOptionsControlsInterface`
(line 2432) plus the `OPTIONSCONTROLS` branch of `Game::Tick` (line
1466) that populates dynamic button text from `Config::GetInstance()`.

This is the deepest screen the spec covers so far. It introduces:

- A second background overlay (bank 7 idx 7 — the inner panel)
- A title overlay rendered as text (bank 135 advance 12)
- Six action-name overlays rendered as text (bank 134 advance 10)
- Six rows of three buttons each (`B112x33` × 2 + `BNONE` × 1)
- A ScrollBar object (state-only — `draw == false`, no chrome rendered)
- `objectupscroll` / `objectdownscroll` wiring on the Interface
- Bottom Save / Cancel buttons (`B196x33` × 2)

All in [palette](palette.md) sub-palette **1** (inherited from
MAINMENU through OPTIONS without being explicitly set — same gotcha
as the OPTIONS screen).

## Activation

When `Game::state` enters `OPTIONSCONTROLS`:

1. Destroy all live objects.
2. Build the interface (this doc).
3. Active sub-palette is **inherited** — `OPTIONSCONTROLS` doesn't
   call `palette.SetPalette(...)`. In practice that's sub-palette 1
   (since the entry path is MAINMENU → OPTIONS → OPTIONSCONTROLS,
   and only MAINMENU sets the palette).
4. The first `Tick` populates `c1button[i].text` and `c2button[i].text`
   with the names of the currently-bound keys (read from
   `Config::GetInstance()`); also populates each op-button's text
   with `"OR"` or `"AND"` from the corresponding `keyXoperator`.
5. Once `FadedIn() == true` the screen is visually static.

## Composition

Logical surface 640 × 480, camera at `(320, 240)` so screen coords
== world coords (see [widget-interface.md](widget-interface.md)).

```
+-----------------------------------------------------------+   y=0
|                                                           |
|              Configure Controls                           |   title (text), y=14
|                                                           |
|     [bank-7 idx-7 inner panel — frames the rows]          |
|                                                           |
|   Move Up:        [Up]      OR        [   ]               |   row 0  y≈95..148
|   Move Down:      [Down]    OR        [   ]               |   row 1  y≈148..201
|   Move Left:      [Left]    OR        [   ]               |   row 2  y≈201..254
|   Move Right:     [Right]   OR        [   ]               |   row 3  y≈254..307
|   Aim Up/Left:    [Up]      AND       [Left]              |   row 4  y≈307..360
|   Aim Up/Right:   [Up]      AND       [Right]             |   row 5  y≈360..413
|                                                           |
|              [Save]              [Cancel]                 |   y=405
|                                                           |
+-----------------------------------------------------------+   y=479
```

(The `[bank-7 idx-7 inner panel]` is full-screen-ish — 628 × 454 at
anchor (-5, -6), so it covers most of the screen sitting *above*
the menu plate. It frames the rows by darker chrome / cosmic
texture, not by occluding them.)

## Object list (in draw order)

### Backgrounds

| # | Type    | Properties |
| - | ------- | ---------- |
| 1 | Overlay | `res_bank=6, res_index=0` (full-screen plate). `(x, y) = (0, 0)`. |
| 2 | Overlay | `res_bank=7, res_index=7` (inner panel). `(x, y) = (0, 0)` (anchor offsets `(-5, -6)` place its top-left at screen `(5, 6)`). |

### Title

| # | Type    | Properties |
| - | ------- | ---------- |
| 3 | Overlay | text mode — `text = "Configure Controls"`, `textbank=135`, `textwidth=12`, `x = 320 - (text.length() * 12) / 2 = 320 - (18 * 12) / 2 = 212`, `y = 14`. |

### Per-row widgets (six rows; `i = 0..5`)

For each row index `i`, the host code creates four objects in this
order. All `y` values use `y0 = 95 + i * 53` for the overlays /
op-button and `y1 = 0 + i * 53` for the c1/c2 buttons (those are
sprite-anchored and already include a `+86` from `B112x33`'s
`offset_y`, so `y1 + 86 == y0 - 9`).

| Sub-# | Type | Properties |
| ----- | ---- | ---------- |
| 4 + 4i | Overlay | text mode — action-name label. `text = keynames[i + scrollposition] + ":"`. `textbank=134`, `textwidth=10`, `x=80`, `y=y0`. |
| 5 + 4i | Button  | `B112x33`, primary key binding. `(x, y) = (-30, y1)`, `uid = i`. `text` = `GetKeyName(key1)` for index `i + scrollposition`. |
| 6 + 4i | Button  | `BNONE`, OR/AND op switch. `(x, y) = (383, y0)`, `uid = 150 + i`. `text` = `"OR"` or `"AND"` based on `Config::GetInstance().keyXoperator`. `width=40, height=30, textbank=134, textwidth=9`. |
| 7 + 4i | Button  | `B112x33`, secondary key binding. `(x, y) = (120, y1)`, `uid = 100 + i`. `text` = `GetKeyName(key2)`. |

### ScrollBar (state-only — not rendered as chrome)

| # | Type | Properties |
| - | ---- | ---------- |
| 28 | ScrollBar | `res_bank=7, res_index=9, barres_index=10`, `scrollpixels=53`, `scrollposition=0`, `scrollmax = numkeys - 6 = 14`, **`draw = false` (default — never set true on this screen)**. Wired into `Interface.scrollbar`. Not rendered as a widget; its `scrollposition` is read by Tick to decide which 6-of-20 keys to show. |

**The visible "scrollbar" on the rendered screen comes from the
bank-7 idx-7 inner-panel sprite's own pixel art** — the panel
includes a decorative scrollbar shape baked into the sprite at the
right edge. The `ScrollBar` widget object the spec describes is a
separate thing that holds the scroll-position state and routes
mouse-wheel events; it has no visible chrome on this screen. A
hydration that simply blits bank 7 idx 7 will get the visible
"scrollbar" for free without implementing any ScrollBar rendering.

### Bottom buttons

| # | Type   | Properties |
| - | ------ | ---------- |
| 29 | Button | `B196x33`, `text = "Save"`, `(x, y) = (-200, 117)`, `uid = 200`. |
| 30 | Button | `B196x33`, `text = "Cancel"`, `(x, y) = (20, 117)`, `uid = 201`. |

## Default visible content (scrollposition = 0)

For QA dumps, the hydration should mirror the real client's
defaults so the visual A/B works. The default key bindings (from
`Config::LoadDefaults` in `clients/silencer/src/config.cpp:114..183`,
non-OUYA build path) for the first six action indices:

| Row | Action name (`keynames[i]`) | C1 (key1) | OR/AND | C2 (key2) |
| --- | --------------------------- | --------- | ------ | --------- |
| 0 | `Move Up:`        | `Up`    | `OR`  | `` (empty — `SDL_SCANCODE_UNKNOWN`) |
| 1 | `Move Down:`      | `Down`  | `OR`  | `` |
| 2 | `Move Left:`      | `Left`  | `OR`  | `` |
| 3 | `Move Right:`     | `Right` | `OR`  | `` |
| 4 | `Aim Up/Left:`    | `Up`    | `AND` | `Left`  |
| 5 | `Aim Up/Right:`   | `Up`    | `AND` | `Right` |

The full action list (20 entries) is in `Game::Game()` at
`game.cpp:59..78`; the default bindings for the remaining 14 are
in `config.cpp:170..181`. The hydration only needs the first six
to match the dump.

`GetKeyName` (`game.cpp:5436..5560`) renders SDL scancodes as
human-readable strings: `SDL_SCANCODE_UP → "Up"`,
`SDL_SCANCODE_LEFT → "Left"`, `SDL_SCANCODE_UNKNOWN → ""`, etc. A
hydration can hardcode the six labels above and skip the full table.

## Focus / input wiring

- `tabobjects` order (from `AddTabObject` calls): six `c1button[i]`,
  six `c2button[i]`, then `Save`, `Cancel`. Tab cycles among the
  twelve key buttons plus the two bottom buttons; the OR/AND op
  buttons are not tab-focusable.
- `activeobject = 0` (no focus on entry — "nothing focused" sentinel).
- `buttonenter = savebutton.id` — Enter clicks Save. **Note:** the
  Tick code re-points `buttonenter` per frame to whichever c1/c2
  button is currently active; a hydration not implementing hover
  can leave it pinned to Save.
- `buttonescape = cancelbutton.id` — Escape clicks Cancel.
- `scrollbar = scrollbar.id` — wires mouse-wheel events.
- `objectupscroll = c1button[0].id`, `objectdownscroll = c2button[5].id` —
  arrow-key wraparound triggers scroll instead of focus wrap.

## Click handlers (out of scope for visual hydration)

`Game::Tick` `OPTIONSCONTROLS` branch (`game.cpp:1466..1635`):

- Clicking a c1/c2 button enters key-rebind mode (the next pressed
  key is captured via `iface->lastsym`).
- Clicking an op button toggles `keyXoperator` between `OR` and `AND`.
- `Save` writes `Config::GetInstance().Save()` and returns to OPTIONS.
- `Cancel` discards changes and returns to OPTIONS.

## QA dump

`clients/silencer`'s `Game::Present` accepts
`SILENCER_DUMP_STATE=OPTIONSCONTROLS`, which navigates from
MAINMENU → OPTIONS → OPTIONSCONTROLS by synthesizing clicks on
uid 2 (Options) then uid 1 (Controls). Once the screen has faded
in (`FadedIn() == true`), the dump fires.

The hydration writes `${SILENCER_DUMP_DIR}/options_controls.ppm`.

## Spec gaps surfaced while authoring this screen

(Recorded so future readers don't repeat them.)

- The "scrollbar with `draw = false`" pattern wasn't covered before;
  it's now in [widget-scrollbar.md](widget-scrollbar.md). Hydrations
  for screens currently in scope can implement just the state
  fields (no rendering).
- `BNONE` button variant required documenting because its text-only
  form has different hit-test semantics (no anchor offset) and a
  different `yoff` rule (no centering — top-anchored). See
  [widget-button.md](widget-button.md).
- `Interface.objectupscroll` / `objectdownscroll` are new on this
  screen and now appear in [widget-interface.md](widget-interface.md).
