# Screen — Lobby / Connect

**Source:** `clients/silencer/src/game.cpp::CreateLobbyConnectInterface`
(line 2668), `Game::Tick` `LOBBYCONNECT` branch (line 613), and
`Game::ProcessLobbyConnectInterface` (line 4143).

The first lobby screen — a login form with username/password text
inputs, two `B52x21` buttons (Login, Cancel), and a `TextBox` that
mirrors the lobby connection's status. The connection happens
asynchronously; the TextBox accumulates lines as the network state
machine advances.

This is also the **first screen that exercises sub-palette 2**.
Unlike the menu screens (palette 1) and options sub-screens (which
inherit palette 1), `LOBBYCONNECT` explicitly calls
`palette.SetPalette(2)` on entry. A hydration that has only ever
rendered palette 1 must wire palette 2 in here.

## Activation

When `Game::state` enters `LOBBYCONNECT`:

1. Destroy all live objects.
2. Build the interface (this doc).
3. **Active sub-palette = 2** (`palette.SetPalette(2)` at
   `game.cpp:620`).
4. `world.lobby.state = WAITING`; `motdprinted = false`.
5. Once `FadedIn() == true`:
   - Music plays.
   - `ProcessLobbyConnectInterface` runs every Tick. It reads
     `world.lobby.state` and pushes status lines into the TextBox
     as the network state machine advances:
     `WAITING → "Connecting to <host>:<port>" + call Connect()` →
     `RESOLVING/RESOLVED → "Hostname resolved"` →
     `CONNECTIONFAILED → "Connection failed", state = IDLE`.
6. The screen reaches a **deterministic steady state** when
   `world.lobby.state == IDLE` (terminal — no further lines added).

## Composition

```
+-----------------------------------------------------------+   y=0
|                                                           |
|             [bank-7 idx-2 lobby panel sprite]             |   green-bordered panel,
|             ┌───────────────────────────────┐             |   spans (178..462, 93..370)
|             │ Connecting to 127.0.0.1: 517  │             |
|             │ Hostname resolved             │             |   TextBox content,
|             │ Connection failed             │             |   bank 133, advance 6,
|             │                               │             |   lineheight 11
|             │                               │             |
|             │                               │             |
|             │                               │             |
|             │                               │             |
|             ├───────────────────────────────┤             |
|             │ Username  [_____________]     │             |   bank 134 labels,
|             │ Password  [_____________]     │             |   bank 133 inputs
|             │              [Login][Cancel]  │             |   B52x21 buttons
|             └───────────────────────────────┘             |
|                                                           |
+-----------------------------------------------------------+
```

## Object list (in draw order — note: this is `Interface::AddObject` order, NOT object-construction order)

| # | Type    | Properties |
| - | ------- | ---------- |
| 1 | Overlay | Background panel: `res_bank=7, res_index=2`. `(x, y) = (0, 0)`. Sprite anchor offset `(-178, -93)` places its top-left at screen `(178, 93)`; it spans `284 × 277` ending at `(462, 370)`. |
| 2 | TextBox | `(x, y) = (185, 101)`, `width = 250`, `height = 170`, `res_bank = 133`, `lineheight = 11`, `fontwidth = 6`. **Default-config content** (lobby host `127.0.0.1:517` unreachable) at the IDLE pin: three lines — `"Connecting to 127.0.0.1: 517"`, `"Hostname resolved"`, `"Connection failed"`. All at `color = 0`, `brightness = 128`. |
| 3 | TextInput | Username. `(x, y) = (275, 293)`, `width = 180`, `height = 14`, `res_bank = 133`, `fontwidth = 6`, `maxchars = 16`, `maxwidth = 16`, `password = false`, `uid = 1`. Empty `text`. **Initially focused** (`Interface::activeobject = usernameinput.id`, `ActiveChanged` called in constructor → `showcaret = true`, caret visible per `state_i % 32 < 16`). |
| 4 | TextInput | Password. `(x, y) = (275, 320)`, same shape, `maxchars = 28`, `maxwidth = 28`, `password = true`, `uid = 2`. Empty. Not focused → no caret rendered. |
| 5 | Button  | Login. `B52x21`, `text = "Login"`, `(x, y) = (264, 339)`, `uid = 0`. |
| 6 | Button  | Cancel. `B52x21`, `text = "Cancel"`, `(x, y) = (321, 339)`, `uid = 1`. |

(Plus two text-mode Overlays for the `"Username"` and `"Password"`
labels — these are *constructed* before the interface but **not
added** to `iface->objects`. They render anyway because every
Overlay in the world participates in the per-frame draw loop, but
they're not Interface-managed. Treat them as separate-but-required
objects:)

| # (extra) | Type | Properties |
| --------- | ---- | ---------- |
| L1 | Overlay (text) | `text = "Username"`, `textbank = 134`, `textwidth = 9`, `(x, y) = (190, 291)`. |
| L2 | Overlay (text) | `text = "Password"`, `textbank = 134`, `textwidth = 9`, `(x, y) = (190, 318)`. |

A hydration that builds its own renderer can just render all six
"Interface objects" plus the two label Overlays in any order — the
panel sprite occludes nothing transparent so layering doesn't
matter for this composition.

## Default visible content

For QA dumps the host is the compile-time default
(`SILENCER_LOBBY_HOST = "127.0.0.1"`, `SILENCER_LOBBY_PORT = 517`).
With no lobby running on that port, the network state machine
walks `WAITING → RESOLVING → RESOLVED → CONNECTED-attempted →
CONNECTIONFAILED → IDLE`, adding TextBox lines as it goes:

| Order | Line | Color | Brightness |
| ----- | ---- | ----- | ---------- |
| 1     | `Connecting to 127.0.0.1: 517` | 0 | 128 |
| 2     | `Hostname resolved`              | 0 | 128 |
| 3     | `Connection failed`              | 0 | 128 |

Note the formatted host string includes a space before the port:
`"Connecting to %s:%d"` with `host = "127.0.0.1"`, `port = 517`,
yielding `Connecting to 127.0.0.1: 517` — wait, the printf here
is `snprintf(line, sizeof(line), "Connecting to %s:%d", host, port)`
which produces `Connecting to 127.0.0.1:517` (no space). The
visible space between `:` and `517` in the rendered output is a
glyph-spacing artifact of bank 133 — the `:` glyph is narrower
than other glyphs at advance `6`, leaving an apparent gap. A
hydration passing the literal string `"Connecting to 127.0.0.1:517"`
will reproduce the same gap.

## Focus / input wiring

- `tabobjects` order: Username, Password, Login, Cancel.
- `activeobject = usernameinput.id` (set explicitly in the
  constructor, then `ActiveChanged` propagates focus state to the
  inputs — sets `usernameinput.showcaret = true`,
  `passwordinput.showcaret = false`).
- `buttonenter = loginbutton.id` — Enter clicks Login.
- `buttonescape = cancelbutton.id` — Escape clicks Cancel.

## Caret render

The username caret is a 1-px-wide vertical bar at
`(275 + 0, 293 - 1) = (275, 292)`, height `round(14 * 0.8) = 11`,
color = `caretcolor`. Visibility cycles on `state_i % 32 < 16`
(visible for ticks 0..15, hidden for 16..31). For the dump to
include the caret, fire on a tick where `state_i % 32 < 16`.
`Lobby::state == IDLE` is reached well before `state_i` overflows
into the off-half of the cycle, but a sufficiently slow run can
hit either half — a hydration that hardcodes the caret as visible
matches the modal real-client output.

The default `caretcolor` should be set by the parent screen for
visibility against the panel; the lobby-connect code doesn't
explicitly set it, leaving it at the Object-default `caretcolor`
(implementation-defined — verify against the dump). The visible
caret in the reference dump is a near-white stroke roughly matching
the panel's text-color palette ramp.

## QA dump

```
SILENCER_DUMP_STATE=LOBBYCONNECT \
SILENCER_DUMP_PATH=/tmp/real_lobby_connect.ppm \
  ./Silencer.app/Contents/MacOS/Silencer
```

Navigates MAINMENU → LOBBYCONNECT by clicking the main menu's
"Connect To Lobby" button (uid 1), then waits until
`world.lobby.state == IDLE` before dumping (the network must finish
its connect attempt and fail).

Hydration writes `${SILENCER_DUMP_DIR}/lobby_connect.ppm`. The
hydration doesn't simulate the network — it pre-populates the
TextBox with the three default-config lines.

## What this screen reuses

- [palette.md](palette.md) — **sub-palette 2** (first screen that uses it)
- [sprite-banks.md](sprite-banks.md) — bank 7 idx 2 panel
- [font.md](font.md) — bank 133 (TextBox + TextInput), bank 134 (labels)
- [widget-overlay.md](widget-overlay.md) — sprite Overlay (panel) + text Overlays (Username/Password labels)
- [widget-textbox.md](widget-textbox.md) — TextBox
- [widget-textinput.md](widget-textinput.md) — TextInput (incl. caret)
- [widget-button.md](widget-button.md) — `B52x21`
- [widget-interface.md](widget-interface.md) — focus, Tab, button_enter/button_escape

## Spec gaps surfaced while authoring this screen

- The two label Overlays are constructed before the Interface but
  never added to `iface->objects`. They render anyway because the
  engine's per-frame draw loop iterates *all* Overlay objects, not
  just Interface-managed ones. This is a global-iteration assumption
  that hadn't been called out in [widget-interface.md](widget-interface.md);
  it's now mentioned in this screen's object list.
- TextInput's default `caretcolor` isn't set by the screen — it's
  whatever the Object class default is. The visible caret color in
  the reference dump should be checked against a hydration default
  if the dumps don't match.
