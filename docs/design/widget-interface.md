# `Interface` (container, focus manager, dispatcher)

**Source:** `clients/silencer/src/interface.h`,
`clients/silencer/src/interface.cpp`,
camera offset in `clients/silencer/src/camera.cpp:52..58`.

`Interface` is a non-rendering container that owns a list of child
objects (Buttons, Overlays, …), tracks which one has focus, and
dispatches mouse/keyboard input. The main menu uses exactly one
`Interface`.

## What it owns

| Field | Purpose |
| ----- | ------- |
| `objects` (vector) | Every child id; the renderer iterates them in order to draw. |
| `tabobjects` (vector) | Subset of `objects` that participate in Tab focus order. |
| `activeobject` | Currently focused child (the one whose chrome is `ACTIVATING`/`ACTIVE`). The sentinel "nothing focused" is the integer `0` — Silencer object IDs start at `1`, so `id == 0` is never a valid object. A port that uses pointers instead of IDs should use `nullptr` for the same role. |
| `buttonenter` | Optional button id triggered by Enter. `0` = no Enter binding. |
| `buttonescape` | Optional button id triggered by Escape. |
| `disabled`, `modal`, `scrollbar`, `width/height/x/y` | Used by other screens (lobby, modals); main menu leaves them at defaults. |

`AddObject(id)` appends to `objects`. `AddTabObject(id)` appends to
`tabobjects`; if `activeobject` is unset, the first call also seeds
`activeobject = id`.

## Coordinate convention

There is **no special coordinate transform inside `Interface`**.
Child objects' `(x, y)` flow straight into the renderer. What looks
like a coordinate convention is actually two upstream things:

1. **Camera offset.** For non-INGAME screens, `Game` sets
   `renderer.camera.SetPosition(640/2, 480/2)`. The camera is sized
   to match the screen (`Camera(640, 480)`), so:

   ```
   camera.GetXOffset() = (w/2) - x = 0
   camera.GetYOffset() = (h/2) - y = 0
   ```

   World coordinates and screen coordinates are 1:1 on the menu.

2. **Sprite anchor offsets.** Bank-6 button sprites have
   `offset_x ≈ -310, offset_y ≈ -288`, baked into the asset (see
   [sprite-banks.md](sprite-banks.md)). So a child Button placed at
   `object.x = 40, object.y = -134` ends up rendering at screen
   `(40 + 310, -134 + 288) = (350, 154)`.

That's why the menu's button positions are tiny single- or
double-digit numbers ("`y = -134`") — the offset baked into the
sprite does the heavy lifting. There's no implicit "from-center" or
"from-bottom" convention to learn at the `Interface` level.

## Tab navigation

```
TabPressed:
    if activeobject not in tabobjects:
        activeobject = tabobjects[0]
    else:
        activeobject = next entry in tabobjects (wraps)
    notify all children of focus change → ActiveChanged
```

Each `Button` checks `Interface.activeobject == self.id` in its
`Tick`; if so, it forces its state to `ACTIVATING` (mouse-equivalent
of "hover"). Losing focus forces `DEACTIVATING`. Mouse hover and
keyboard focus drive the same animation.

## Enter / Escape

```
EnterPressed:
    if buttonenter is set: that button.clicked = true (one-shot)
    else if activeobject is a Button: that button.clicked = true

EscapePressed:
    if buttonescape is set: that button.clicked = true
```

The main menu sets `buttonescape = exitbutton.id` (so Esc quits)
and leaves `buttonenter = 0`.

## Mouse dispatch

```
ProcessMousePress / Move / Wheel:
    record mousedown / mousex / mousey
    ActiveChanged(this, mouse=true)
ActiveChanged:
    for each child:
        if child responds to hit-test and contains (mousex, mousey):
            activeobject = child
            child receives press / hover via its OnMouse path
```

Children that don't reply true to a hit-test keep their state
unchanged.

## Render

`Interface` does not draw itself; the engine's render loop iterates
`world.objects` (filtered by `currentinterface`'s `objects` list)
and draws each child via the standard Object→sprite path
(see [sprite-banks.md](sprite-banks.md)) and
the Overlay/Button/etc. specifics covered in their own docs.

## How the main menu wires it

```
Interface (the menu container):
    objects (draw order):
        Overlay  background  (bank 6, idx 0)
        Overlay  logo        (bank 208, animated)
        Overlay  version     (text mode, bottom-left)
        Button   "Tutorial"          → uid 0
        Button   "Connect To Lobby"  → uid 1
        Button   "Options"           → uid 2
        Button   "Exit"              → uid 3
    tabobjects (focus order):
        Tutorial → Connect To Lobby → Options → Exit
    activeobject = 0          # sentinel = nothing focused (object IDs start at 1)
    buttonenter  = 0          # Enter unbound
    buttonescape = exit.id    # Esc triggers Exit
```

Constructed in `Game::CreateMainMenuInterface`
(`clients/silencer/src/game.cpp:2266..2341`).
