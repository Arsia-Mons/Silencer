# Keybind rebind — original design (reference)

How re-binding a key works on **Options → Configure Controls**. This is the
**pre-refactor (origin) design**, captured by building the commit before the
cppx UI migration (`b2e4edb0^`) and driving the live controls screen.

There is **no modal**. The interaction is entirely inline on the oval:

1. The user clicks a bind oval (e.g. **Move Up**, currently "Up"). This arms
   keybind capture for that action/slot.
2. The clicked oval immediately shows **`-`** — the waiting-for-input glyph —
   while everything else stays put. See `keybind-rebind-waiting.png` (the
   "Move Up" oval reads `-`). Origin source:
   `controls_keybind_list.cpp` → `display = rebinding ? "-" : text`.
3. The first key / mouse button / gamepad button / axis pressed becomes the new
   binding and is committed immediately (origin's "next press binds"). The oval
   then shows the new key. See `keybind-rebind-committed.png` ("Move Up" rebound
   to **J**; the builtin preset forks to "Default-Custom").
4. An abandoned capture (no key pressed) auto-cancels after the idle timeout.

`keybind-controls-screen.png` is the steady-state screen for reference.

Source: `clients/silencer/src/client/ui/screens/options/options_controls.cppx`
(the `-` glyph in the row loop + the first-edge commit in `OptionsControlsView`).

## Regression history

The cppx UI migration (#267) kept the keybind capture state machine but dropped
the in-game capture UI: clicking a bind oval silently armed capture with no `-`
glyph and no commit path (only the headless control socket called
`confirm_chord`), so keys could not be rebound. This restores the origin flow.
