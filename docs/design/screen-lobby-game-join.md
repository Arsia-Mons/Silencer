# screen-lobby-game-join — Game waiting lobby (per-game pre-match)

The per-game waiting lobby that appears when a player joins a game,
before the match starts. Has Choose Tech / Change Team / Ready
action buttons.

Reference dump: `/tmp/real_lobby-gamejoin_dump.ppm` (640×480 P6,
sub-palette 2, captured via `SILENCER_DUMP_STATE=LOBBY_GAMEJOIN`
which auto-jumps to LOBBY then injects `gamejoininterface =
CreateGameJoinInterface()->id`).

**Faux-state caveat:** the captured reference shows only the 3
action buttons (Choose Tech / Change Team / Ready) plus the LOBBY
chrome and presence list behind. The engine's
`CreateGameJoinInterface` itself only adds those 3 buttons — there
is no separate "player roster" UI element here. In a real game
session, the player list / team rosters render as in-world Player
objects (with sprites + name overlays) populated from `world.peerlist`
and the Team objects set up at game-load time, not as part of
GameJoinInterface. Capturing those would require an actual
multi-peer game session, which is beyond what the dump-mode harness
fakes. The "Disconnected from game" engine modal that would
otherwise overlay is suppressed by the harness via
`world.state = World::CONNECTED`.

## Sub-palette

`2`.

## Object inventory (replaces right-side panel)

Bounding box: `(x=403, y=87, width=222, height=267)` — same as
GameSelectInterface.

| z | Object | Type | x | y | Notes |
| - | --- | --- | --- | --- | --- |
| 0 | Choose Tech button | B156x21 | 242 | 68  | text=`Choose Tech`, uid=27 |
| 1 | Change Team button | B156x21 | 242 | 100 | text=`Change Team`, uid= |
| 2 | Ready button       | B156x21 | 242 | 160 | text=`Ready`, uid=25 |

The bbox starts at x=403 but the buttons are at x=242 — that's
because button anchors are center-relative in the engine. On screen,
all three buttons appear in the right-of-center region.

Per host vs non-host: `gamestartbutton->text` is conditionally `Start
Game` (host) or `Ready` (non-host). The canonical dump shows `Ready`.

## What's behind / over the modal

- **Behind:** LOBBY chrome (panel, header, CharacterInterface,
  ChatInterface) — visible underneath.
- **Over (non-structural):** "Disconnected from game" modal dialog
  centered around (320, 200), with OK button. The candidate may
  render this if it implements `CreateModalDialog` (bank 40 idx 4
  background + text overlay + B156x21 OK button), but it is **not
  required** for the structural gate.

## Spec gaps

- `widget-modaldialog.md` (or extend an existing widget doc) —
  documents the renderpass=3 dialog overlay used by `CreateModalDialog`
  for "Disconnected"/"Could not create game"/etc messages. Bank 40
  idx 4 background.

## Cross-references

- [`screen-lobby.md`](screen-lobby.md), [`screen-lobby-gameselect.md`](screen-lobby-gameselect.md)
