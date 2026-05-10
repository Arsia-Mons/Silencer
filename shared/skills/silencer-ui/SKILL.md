---
name: editing-silencer-ui
description: Use when touching UI in the Silencer C++ client — adding or editing buttons, menus, screens, modals, HUD, lobby panels, or in-game popups (chat/buy/tech) under `clients/silencer/src/ui/`, `clients/silencer/src/render/`, or the procedural UI in `Player::Tick` (`clients/silencer/src/actors/player.cpp`).
---

# editing-silencer-ui

The Silencer client UI is **not a UI framework**. Every widget is a
subclass of `Object` (alongside players and projectiles), constructed
via `world.CreateObject(ObjectTypes::BUTTON)` with every field hand-set,
positioned by integer pixel coordinates, hit-tested by polling, and
dispatched through giant `switch(object->type)` blocks in
`interface.cpp` and `renderer.cpp`. There is no layout engine, no event
delivery, no widget defaults, and no virtual `Draw`/`OnClick`.

This skill captures the traps that bite agents writing UI here. Read
it before touching `clients/silencer/src/ui/` or adding in-game
popups in `Player::Tick`.

## Before you write any UI code

1. **Find a similar screen and copy its conventions** — coordinate
   convention, font choice, button chrome, cleanup pattern. The
   codebase has two coord conventions in active use; pick the one
   your neighbor uses.
2. **Skim the design docs.** `docs/design/widget-button.md`,
   `widget-overlay.md`, `widget-interface.md`, and `screen-*.md`
   document baked anchor offsets, sprite-bank meanings, and per-screen
   layout. They are reverse-engineered and accurate.
3. **Plan to verify with `using-silencer-cli`** (see
   `shared/skills/cli/SKILL.md`). UI changes that compile can still
   render at the wrong coords, drop input, or leak between screens.
   Drive the binary with `inspect` + `screenshot` end-to-end.

## Where UI lives (post-refactor; the top-level CLAUDE.md is stale)

| Area | Path |
|---|---|
| Widget classes | `clients/silencer/src/ui/components/` |
| Screen base + dispatch | `clients/silencer/src/ui/screens/screen.h`, `screen_context.h` |
| Top-level screens | `clients/silencer/src/ui/screens/<name>/` |
| Sub-panels (no stack) | `clients/silencer/src/ui/screens/lobby/panels/` |
| Modals | `clients/silencer/src/ui/modals/` |
| Renderer dispatch | `clients/silencer/src/render/renderer.cpp` |
| Screen-stack wiring | `Game::PushScreen / PopScreen / ReplaceScreen` in `clients/silencer/src/game/game.cpp` |
| In-game procedural UI | `Player::Tick` in `clients/silencer/src/actors/player.cpp` (chat / buy / tech) |
| HUD blits | `Renderer::DrawHUD` in `renderer.cpp` |

Active-screen input is routed via the single global
`Game::currentinterface` Uint16 — every SDL event goes to that one
Interface (set on push/pop, overridden by `ProcessInGameInterfaces`
when a chat/buy/tech popup is open).

## Required fields per widget

Widgets have **no defaults**. Setting one field but forgetting another
gives a working compile and a broken UI.

| Widget | Required fields | Notes |
|---|---|---|
| `Button` | `x, y, uid` + `strcpy(text, "...")` | `SetType(B156x21)` etc. picks chrome; default chrome has anchor `(-310,-288)` so negative `x/y` is normal. |
| `Overlay` (sprite) | `res_bank, res_index` (+ `x, y` if not full-screen) | Default position `(0,0)`; full-screen backgrounds rely on the baked anchor. |
| `Overlay` (text) | `text, textbank, textwidth, x, y` | `text` is `std::string`. Centering: `x = 320 - (text.length() * textwidth) / 2`. |
| `TextInput` | `x, y, width, height, fontwidth, maxchars, maxwidth, uid` | Plus `password`, `numbersonly`, `caretcolor` if needed. Forget any of the 8 → invisible / unfocusable. |
| `TextBox` | `x, y, width, height, lineheight, fontwidth, res_bank, uid` | `text` is `deque<vector<char>>` with embedded color bytes — append via the helper, not direct push. |
| `SelectBox` | `x, y, width, height, lineheight, uid` + populate `items` | `selecteditem`, `scrolled` are state. |
| `Scrollbar` | `barres_index, scrollpixels` + couple via `iface->scrollbar = sb->id` | Three-way wired with the SelectBox/TextBox it scrolls. |
| `Toggle` | `x, y, set, uid` + `strcpy(text, "...")` | `set` groups mutually-exclusive radios; `selected` is state. |
| `Interface` | (root) save `interfaceId = iface->id`; (nested) `x, y, width, height` for mouse routing | `AddObject` and `AddTabObject` are SEPARATE lists. |

`text` buffer sizes per widget are inconsistent — check the header
before assigning: `Button::text` is `char[32]`, `Toggle::text` is
`char[64]`, `TextInput::text` is `char[256]`, `Overlay::text` is
`std::string`.

## Adding a button to an existing screen — copy this template

```cpp
// 1. Top of the .cpp file: extend the screen's button enum.
//    The 'BTN_' prefix dodges anonymous-namespace collisions when
//    SILENCER_UNITY_BUILD merges this TU. Don't drop the prefix.
namespace
{
enum OptionsButton : Uint8 {
    BTN_GO_BACK  = 0,
    BTN_CONTROLS = 1,
    BTN_DISPLAY  = 2,
    BTN_AUDIO    = 3,
    BTN_NEW      = 4,    // <- new uid
};
}

// 2. In Build(), create the button. Coords here are anchor-space
//    (default Button chrome) — copy a sibling's offsets.
Button * newbutton = (Button *)world.CreateObject(ObjectTypes::BUTTON);
newbutton->y = 67;
newbutton->x = -89;
newbutton->uid = BTN_NEW;
strcpy(newbutton->text, "New Button");      // NOT `newbutton->text = "..."`

// 3. Wire it into BOTH lists.
iface->AddObject(newbutton->id);             // draw + mouse hit-test
iface->AddTabObject(newbutton->id);          // keyboard / gamepad nav

// 4. In Tick(), handle the click and CLEAR THE FLAG.
case BTN_NEW:
    ctx.GoToState(GameState::SOMETHING);
    break;
// ... after the switch:
button->clicked = false;                     // forgetting → re-fires every frame
```

## Cleaning up — `Destroy()` must release widgets

`Game::PopScreen` calls `Screen::Destroy(ctx)`. If your `Build`
allocated widgets via `world.CreateObject`, your `Destroy` must
release them or they linger in `world.objectlist`:

```cpp
void MyScreen::Destroy(ScreenContext & ctx)
{
    if(interfaceId){
        Interface * iface = (Interface *)ctx.world.GetObjectFromId(interfaceId);
        if(iface) iface->DestroyInterface(ctx.world);   // recursive
        interfaceId = 0;
    }
}
```

`DestroyInterface` walks `objects` recursively and `MarkDestroyObject`s
each. Some screens (`OptionsScreen`, `MainMenuScreen`) ship empty
`Destroy()` bodies — that is a latent bug, **not** a pattern to copy.
The `MessageModal::Destroy` body above is the correct template.

## The traps

1. **Coords are screen-OR-anchor depending on the chrome.** A `Button`
   with default chrome at `x=-89` lands at screen `x=221` (anchor
   `-310` baked into bank-6). Small variants like `B156x21` have a
   near-zero anchor, so screens like `MessageModal` use literal screen
   pixels (`x=242, y=230`). Don't guess — copy a sibling.

2. **Widgets have no defaults.** Forget `res_bank` on a `TextInput` →
   it renders invisible. Cross-reference the table above for every
   required field per widget type.

3. **`uid` collisions silently break input.** `uid` is `Uint8` and
   per-screen — there is no global registry. Always wrap your enum in
   `namespace { enum SomethingButton : Uint8 { BTN_FOO = 0, ... }; }`
   with a screen-specific prefix. `SILENCER_UNITY_BUILD` merges TUs
   and unprefixed enumerator names collide.

4. **Polled `clicked` must be reset.** `if(button->clicked) { ...;
   button->clicked = false; }` — forget the reset and the action fires
   every frame. Same applies to `enterpressed`, `tabpressed`,
   `selectbox->enterpressed`.

5. **`Button::text` is a fixed buffer; use `strcpy`.** Inconsistent
   across widget types (see Required-fields table). Strings longer
   than the buffer overflow silently. Pick shorter labels or use a
   wider chrome variant — do not extend the buffer.

6. **`AddObject` ≠ `AddTabObject`.** `objects` drives draw + mouse
   hit-testing; `tabobjects` drives keyboard/gamepad navigation.
   Forget the second → button works on mouse but is unreachable on
   gamepad (the game ships gamepad support).

7. **Don't add new widget types.** `Interface::ActiveChanged`
   (`interface.cpp:133-331`) and the renderer
   (`renderer.cpp:835-948`) both have ~200-line `switch(type)` blocks.
   Adding a widget type means editing both files plus
   `objecttypes.cpp`. Reuse `Overlay` (with `customsprite[]` for raw
   pixel blits, or `text` for labels) or compose existing widgets in a
   `Panel` instead.

8. **Don't reach into the renderer for new fields.** The renderer
   already references widget-specific fields directly
   (e.g. `selectbox->downloadprogress` for the map-download UX). Do
   not add new such couplings — fold new visual state into existing
   fields.

9. **Magic resource indices.** `res_bank` and `res_index` are bare
   integers. The bank ↔ content map is in
   `docs/design/sprite-banks.md` and scattered through `renderer.cpp`
   switches. Don't guess — look it up or copy from a sibling screen.

10. **`Destroy()` must call `DestroyInterface`.** See above. Use
    `MessageModal::Destroy` as your template, not `OptionsScreen`.

11. **In-game popups bypass the Screen system entirely.** Chat, buy,
    and tech menus are built imperatively in `Player::Tick`
    (`clients/silencer/src/actors/player.cpp:261-595`); their
    interface ids live on `Player` (`chatinterfaceid`,
    `buyinterfaceid`, `techinterfaceid`) and
    `Game::ProcessInGameInterfaces` overrides `currentinterface` when
    one is open (priority order: chat > buy > tech). To add a fourth,
    extend `Player` and add a fourth branch in
    `ProcessInGameInterfaces` — do not try to use `PushScreen`
    mid-game.

12. **Camera offset infects menu coords.** Menus call
    `ctx.renderer.camera.SetPosition(320, 240)` to make the camera
    offset zero so `(x, y)` works as authored. If a menu inherits a
    non-default camera position from a previous state, every widget
    shifts by `(dx, dy)`. If your menu looks translated by a constant,
    suspect the camera before suspecting the layout.

13. **Manual text centering.** `x = 320 - (text.length() * textwidth)
    / 2` is the centering idiom; re-run it whenever `text` mutates.
    There is no `align: center`.

## Red flags — STOP and re-read this skill

- "I'll just write `button->text = "Foo";`" — won't compile, it's
  `char[32]`. Use `strcpy`.
- "I'll add `clicked = false` after I wire the click" — write the
  reset in the same patch as the `if(clicked)` check, never separately.
- "I'll skip `AddTabObject` for now" — gamepad users break silently.
- "I'll put the `enum` at file scope without `namespace { }`" — unity
  build collisions, hours of debugging.
- "I'll set `x = 100, y = 100` and see where it lands" — coords are
  meaningless without knowing the chrome's baked anchor. Copy a
  sibling and adjust from there.
- "I'll add a new `ObjectTypes::MYWIDGET`" — touching the dispatch
  switches is rarely the right answer; reuse `Overlay` or compose
  existing widgets in a `Panel`.
- "Empty `Destroy()` is fine" — only if `Build()` allocated nothing.
  Verify by reading the widget creates in `Build`.
- "I'll edit `shared/design/sdl3/components/button.cpp`" — that's the
  parallel pure-function design-system codebase used for PPM
  regression dumps; it does not affect the running game. Edit
  `clients/silencer/src/ui/components/button.cpp` instead.
- "I'll claim done after compile + push" — UI compiles can still drop
  input, mis-render, or leak across screens. Verify with the CLI.

## Verifying changes end-to-end

A successful build proves nothing about UI. Drive the binary through
`using-silencer-cli` to confirm position, input, and screen
transitions:

```bash
. tests/cli-agent/e2e/lib.sh
PORT=$(pick_port); PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

cli --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 15000
cli --port "$PORT" click --label OPTIONS
cli --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000
cli --port "$PORT" inspect | jq '.widgets[] | {id,kind,label,x,y}'
cli --port "$PORT" screenshot --out /tmp/options.png
cli --port "$PORT" click --label "New Button"
# Verify the click did the right thing — state change, screen swap, etc.
```

`inspect` exposes every widget on the active screen (id, kind, label,
position); `click --label` matches button text case-insensitively;
`screenshot` writes a PNG so you can eyeball alignment. Full op set in
`shared/skills/cli/SKILL.md`.
