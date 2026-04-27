# screen-lobby-game-create — Create Game modal

The "Create Game" form modal that appears over the LOBBY when the user
clicks the **Create Game** button. Contains game-options form, map
selector, game name + password inputs, and a Create action button.

Reference dump: `/tmp/real_lobby-gamecreate_dump.ppm` (640×480 P6,
sub-palette 2, captured via `SILENCER_DUMP_STATE=LOBBY_GAMECREATE`
which auto-jumps to LOBBY then injects `gamecreateinterface =
CreateGameCreateInterface()->id` after a 30-tick settle).

The base LOBBY chrome (panel, header, character panel left, chat
bottom) is visible behind. This modal **replaces** the
GameSelectInterface region (right side, x=403..625).

## Sub-palette

`2` (lobby palette, same as parent LOBBY).

## Object inventory (replaces right-side panel content)

Bounding box: `(x=403, y=87, width=222, height=390)` — taller than
GameSelectInterface (267) to fit the form.

| z | Object | Type | Bank | Index | x | y | Notes |
| - | --- | --- | --- | --- | --- | --- | --- |
| 0 | Right border       | overlay | 7 | 8 | 0 | 0 | Same chrome sprite as GameSelectInterface |
| 1 | "Game Options" label | overlay (font 134, w=8) | — | — | 272 | 70 | top of form |
| 2 | "Security:" label  | overlay (font 134, w=8) | — | — | 245 | 93  | y = 87 + 0×18 + 6 |
| 3 | Security button    | BNONE                   | — | — | 323 | 93  | width=70, height=20, font 134 advance 9, text=`Medium`, uid=40 |
| 4 | "Min Level:" label | overlay (font 134, w=8) | — | — | 245 | 111 | y = 87 + 1×18 + 6 |
| 5 | Min Level input    | TextInput               | — | — | 350 | 111 | width=20, height=20, font 134 advance 8, default=`0`, uid=41 |
| 6 | "Max Level:" label | overlay (font 134, w=8) | — | — | 245 | 129 | |
| 7 | Max Level input    | TextInput               | — | — | 350 | 129 | default=`99`, uid=42 |
| 8 | "Max Players:" label | overlay (font 134, w=8) | — | — | 245 | 147 | |
| 9 | Max Players input  | TextInput               | — | — | 350 | 147 | default=`24`, uid=43 |
| 10| "Max Teams:" label | overlay (font 134, w=8) | — | — | 245 | 165 | |
| 11| Max Teams input    | TextInput               | — | — | 350 | 165 | default=`6`, uid=44 |
| 12| "Select Maps:" label | overlay (font 134, w=8) | — | — | (right column) | (~190) | |
| 13| Map SelectBox      | SelectBox               | — | — | (right column) | — | scrollable map list |
| 14| Map ScrollBar      | ScrollBar               | (engine) | (engine) | — | — | |
| 15| "Game Name:" label | overlay (font 134, w=8) | — | — | 405 | 360 | |
| 16| Game Name input    | TextInput               | — | — | 410 | 375 | width=210, height=14, font 133 advance 6, maxchars=35, uid=5 |
| 17| "Password (optional):" label | overlay (font 134, w=8) | — | — | 405 | 390 | |
| 18| Password input     | TextInput               | — | — | 410 | 405 | width=210, height=14, password=true, uid=6 |
| 19| Create button      | B156x21                 | — | — | 436 | 430 | text=`Create`, uid=35 |

## What's runtime / non-structural

- Default values shown in inputs (Min/Max level, Max Players, Game
  Name from `Config::defaultgamename`). Render as shown or empty;
  position is the gate.
- Map list rows (populated from server's available maps).
- Selected map highlight.

## What's behind the modal

The LOBBY screen continues to render: panel chrome, header (Silencer /
v.00028 / Go Back), CharacterInterface (left), ChatInterface (bottom).
The modal **only replaces** the GameSelectInterface region at top-right.

## Spec gaps

- `widget-selectbox.md` — multi-row list (used here for maps + by
  GameSelectInterface for games).
- `widget-textinput.md` — already needed; here `numbersonly` flag is
  used for level/players/teams inputs.
- `widget-button.md` — BNONE variant (text-only container) reused.

## Cross-references

- [`screen-lobby.md`](screen-lobby.md) — parent state
- [`screen-lobby-gameselect.md`](screen-lobby-gameselect.md) — sibling that this modal replaces
