# Screen — Lobby

**Source:** `clients/silencer/src/game.cpp::CreateLobbyInterface`
(line 2762) plus three sibling factories — `CreateCharacterInterface`
(line 2817), `CreateGameSelectInterface` (line 2935),
`CreateChatInterface` (line 3020) — and the `LOBBY` branch of
`Game::Tick` (line 637) that drives the live state. Sub-palette **2**.

The deepest screen the spec covers. Composed of a **top-level
Interface** that hosts a global background and brand bar, plus three
sibling **sub-Interfaces** (character on the left, chat at the
bottom, game-select on the right). See
[widget-interface.md](widget-interface.md) "Sub-interfaces" for how
the nested-Interface dispatch works.

A static-frame hydration only needs to render the chrome plus a few
defaulted bits (the local username, the selected agency Toggle).
There's no chat history, no game list, and the map name is empty
unless the lobby has set one — for QA dumps against an unconfigured
local lobby everything dynamic is empty.

## Activation

When `Game::state` enters `LOBBY`:

1. `world.lobby.ForgetAllUserInfo()`; `gameplaystate = INLOBBY`;
   `Disconnect()` from any prior game; `lobbyinterface = 0` and
   the various lobby-child interface ids reset to 0.
2. `chatlinesprinted = 0`.
3. `palette.SetPalette(2)` — explicit, same as `LOBBYCONNECT`.
4. Build the top-level interface (this doc):
   `CreateLobbyInterface()` returns the parent; inside it,
   `CreateCharacterInterface`, `CreateGameSelectInterface`, and
   `CreateChatInterface` are called and their ids are added as
   `objects` of the parent.
5. `currentinterface = lobbyinterface.id`.
6. `chatinterface` becomes the parent's `activeobject` so the chat
   input has initial focus.
7. Once `FadedIn() == true`:
   - Music plays (`menumusic`).
   - `ProcessLobbyInterface` runs every Tick: handles incoming
     lobby messages (chat lines into the chat TextBox, presence
     updates into the presence TextBox, NEWGAME messages into the
     game-select SelectBox, MOTD lines, etc.).
   - If `world.lobby.state == DISCONNECTED`, the screen bounces
     back to `LOBBYCONNECT`. (For a static-frame dump against a
     real local lobby this never fires; against the auth-bypass
     dump path the bounce check is overridden.)

## Composition

```
┌─────────────────────────────────────────────────────────────┐  y=0
│                                                             │
│  Silencer  v.<version>   <Map Name>          [B156x21 Go Back]│  y≈29..50
│  ─────────────────────────────────────────────────────────  │
│                                                             │
│ ┌──────────────────┐  ┌──────────────────────┐              │
│ │ <localusername>  │  │  [B156x21 Create Game] │  Active Games │
│ │                  │  ├──────────────────────┤              │
│ │  [agency icons]  │  │                      │              │
│ │   ▢ ▢ ▢ ▢ ▢      │  │   (SelectBox area —  │              │
│ │                  │  │    empty in default) │              │
│ │  Level: --       │  │                      │              │
│ │  Wins:  --       │  │                      │              │
│ │  Losses:--       │  │                      │              │
│ │  Etc:   --       │  └──────────────────────┘              │
│ └──────────────────┘                                        │
│                                                             │
│ ┌──────────────────────────────────────┐ ┌──────────────┐   │  y=216
│ │ Channel name                         │ │              │   │
│ ├──────────────────────────────────────┤ │              │   │
│ │ chat lines... (bottomtotop = true)   │ │ presence box │   │
│ │                                      │ │              │   │
│ │                                      │ │              │   │
│ ├──────────────────────────────────────┤ ├──────────────┤   │
│ │ [chat input field] (focused)         │ │              │   │
│ └──────────────────────────────────────┘ └──────────────┘   │
│                                                             │  y=479
└─────────────────────────────────────────────────────────────┘
```

## Top-level Interface object list

| # | Type    | Properties |
| - | ------- | ---------- |
| 1 | Overlay | `res_bank=7, res_index=1` (full-screen lobby background plate). `(x, y) = (0, 0)`, anchor `(0, 0)` so it covers the whole 640×480 surface. |
| 2 | Overlay | text mode — `text="Silencer"`, `textbank=135`, `textwidth=11`, `effectcolor=152` (red), `(x, y) = (15, 32)`. |
| 3 | Overlay | text mode — `text = "v." + world.version`, `textbank=133`, `textwidth=6`, `effectcolor=189` (orange), `(x, y) = (115, 39)`. |
| 4 | Overlay | text mode — map name. `textbank=135`, `textwidth=11`, `effectcolor=129`, **`effectbrightness=160`**, **`textcolorramp=true`**. `(x, y) = (180, 32)`. `text` is dynamic (set by lobby messages); empty in default. `uid=8`. |
| 5 | Button  | `B156x21`, `text = "Go Back"`, `(x, y) = (473, 29)`, `uid = 10`. |
| 6 | Interface | `chatinterface` (sub-interface; see below). |
| 7 | Interface | `characterinterface` (sub-interface). |
| 8 | Interface | `gameselectinterface` (sub-interface). |

Top-level focus: `activeobject = chatinterface.id`,
`buttonescape = exitbutton.id` (Go Back).

## Character sub-interface

`(x, y) = (10, 64)`, `width = 217`, `height = 120`. Object list:

| # | Type    | Properties |
| - | ------- | ---------- |
| 1 | Overlay (text) | `text = localusername` (empty by default), `textbank=134`, `textwidth=8`, `effectcolor=200`, `(x, y) = (20, 71)`. |
| 2 | Overlay (text) | Level. `uid=2`, `textbank=133`, `textwidth=7`, `effectcolor=129`, `effectbrightness=160`, `textcolorramp=true`, `(x, y) = (17, 130)`. Empty `text` in default. |
| 3 | Overlay (text) | Wins.   `uid=3`, same params, `(x, y) = (17, 143)`. |
| 4 | Overlay (text) | Losses. `uid=4`, same params, `(x, y) = (17, 156)`. |
| 5 | Overlay (text) | Etc.    `uid=5`, same params, `(x, y) = (17, 169)`. |
| 6..10 | Toggle | Five agency icons at `(20 + i * 42, 90)` for `i = 0..4`. `res_bank = 181`, `res_index = 0..4`, `set = 1`, `uid = 1..5`. **The Toggle whose `res_index` matches `Config::GetInstance().defaultagency` (default `NOXIS = 0`, so index 0) starts `selected = true`; the others `false`.** Tick brightens the selected one (`effectbrightness = 128`) and dims the others (`effectbrightness = 32`). All with `effectcolor = 112`. See [widget-toggle.md](widget-toggle.md). |

Toggles are added to `objects` only — **not `tabobjects`**. Keyboard
nav does not iterate them; selection is mouse-only on this screen.

## Game-select sub-interface

`(x, y) = (403, 87)`, `width = 222`, `height = 267`. Object list:

| # | Type    | Properties |
| - | ------- | ---------- |
| 1 | Overlay | Right-border panel. `res_bank=7, res_index=8`, anchor `(-403, -87)`, so at `(0, 0)` it renders at `(403, 87)..(627, 354)`. |
| 2 | Overlay (text) | `text = "Active Games"`, `textbank=134`, `textwidth=8`, `(x, y) = (405, 70)`. |
| 3 | Button | Join Game. `B156x21`, `(x, y) = (436, 430)`, `uid = 20`. |
| 4 | Button | Create Game. `B156x21`, `(x, y) = (242, 68)`, `uid = 30`. |
| 5 | SelectBox | `(x, y) = (407, 89)`, `width = 214`, `height = 265`, `lineheight = 14`, `uid = 10`. **Empty in default** (no games returned by an unconfigured local lobby). |
| 6 | ScrollBar | `res_index = 9`, `scrollpixels = 14`, `scrollposition = 0`. **`draw = false` by default**; the chrome would only render if the SelectBox accumulates more rows than fit. Empty SelectBox → no scrollbar visible. |
| 7..11 | Overlay (text) | Five game-info labels (uid 1..5) at `(405, 358..406)`, bank 133 advance 6. All empty in default. |

Sub-interface fields: `scrollbar = gamescrollbar.id`,
`buttonenter = gamejoinbutton.id`.

## Chat sub-interface

`(x, y) = (15, 216)`, `width = 368`, `height = 234`. Object list:

| # | Type    | Properties |
| - | ------- | ---------- |
| 1 | Overlay | Chat panel border. `res_bank=7, res_index=11`, anchor `(-15, -216)`. |
| 2 | Overlay | Chat input border. `res_bank=7, res_index=14`, anchor `(-15, -433)`. |
| 3 | Overlay (text) | Channel name. `uid=1`, `textbank=134`, `textwidth=8`, `(x, y) = (15, 200)`. Empty in default (joins a default channel after auth, but the server has no channel set). |
| 4 | TextBox | Chat scrollback. `(x, y) = (19, 220)`, `width = 242`, `height = 207`, `res_bank = 133`, `lineheight = 11`, `fontwidth = 6`, **`bottomtotop = true`**. Empty by default. See [widget-textbox.md](widget-textbox.md). |
| 5 | TextBox | Presence (player list). `(x, y) = (267, 220)`, `width = 110`, `height = 207`, `res_bank = 133`, `lineheight = 11`, `fontwidth = 6`, `bottomtotop = false`. `uid = 9`. Empty by default. |
| 6 | TextInput | Chat input. `(x, y) = (18, 437)`, `width = 360`, `height = 14`, `res_bank = 133`, `fontwidth = 6`, `maxchars = 200`, `maxwidth = 60`, `uid = 1`. Empty `text`. **Initially focused** (chatinterface's `activeobject` = chatinput.id, set after the parent assigns chat as its own `activeobject`). |
| 7 | ScrollBar | `res_index = 12`, `barres_index = 13` (note: different track/thumb indices than the game-select scrollbar's `9`/`10`), `scrollpixels = 11` (matches textbox lineheight). `draw = false` by default. |

Sub-interface fields: `scrollbar = chatscrollbar.id`,
`tabobjects = [chatinput]`.

## Visible content under `-demo` (populated lobby)

For visual A/B against a real lobby, run
`services/lobby/silencer-lobby` with the `-demo` flag (see that
file's `hub_demo.go` for the seed data). After the dump path
authenticates as `dump`/`dump`, the lobby pushes pre-seeded games,
chat, and presence to the client; the user record's agency stats
also get force-set to non-trivial values. This drives every
populated-state code path the LOBBY has, so hydrations can stress-
test full rendering.

The seed values, in the order they're observed in the dump:

### Character panel (left)

- **Username overlay** (the user's display name): `text = "dump"`,
  rendered at the position spec'd above (`(20, 71)`, bank 134,
  advance 8, `effectcolor=200`).
- **Selected agency**: NOXIS (uid 1) — stats drawn from
  `user.agency[0]`.
- **Stat overlays** (all `effectcolor=129, effectbrightness=160,
  textcolorramp=true`):
  - `text = "LEVEL: 8"` at `(17, 130)`
  - `text = "WINS: 47"` at `(17, 143)`
  - `text = "LOSSES: 12"` at `(17, 156)`
  - `text = "XP TO NEXT LEVEL: 220"` at `(17, 169)`

  The text format is `"<LABEL>: <number>"` — the prefix label is
  hardcoded per uid, the value comes from the user's agency record.
  See `Game::ProcessLobbyInterface` `case 2..5` for the exact
  string templates.

### Game-select SelectBox (right)

Four rows, in lobby insertion order (no client-side sort):

| `selecteditem` | text |
| -------------- | ---- |
| (none — `-1`) | `Casual Match #1` |
|               | `Veterans Only` |
|               | `Tutorial` |
|               | `Capture the Tag` |

Bank 133, advance 6, `lineheight = 14`. No row highlighted (no
selection); ScrollBar `draw = false` because 4 rows ≤ 19 visible
(`265 / 14 ≈ 18.9`).

### Chat sub-interface

- **Channel-name overlay**: `text = "Lobby"` (the client-side
  `world.lobby.channel`, set by `MSG_CHANNEL` immediately after
  auth). Bank 134, advance 8, position `(15, 200)`.
- **Chat TextBox** (the bottom-up scrollback, `bottomtotop = true`):
  five lines, in order pushed (rendered bottom-to-top so the
  newest sits at the bottom):

  | order | line |
  | ----- | ---- |
  | 1 (oldest, top of visible block when ≥ 5 lines fit) | `Vector: anyone up for a round?` |
  | 2 | `Solace: still waiting on Krieg's match to finish` |
  | 3 | `Ember: we got 4 in casual #1` |
  | 4 | `Vector: joining` |
  | 5 (newest, bottom-most) | `Halcyon: gg everyone` |

  Lines come in via `MSG_CHAT` (TextBox `AddText` with
  word-wrap on `width / fontwidth = 250 / 6 = 41` chars and
  `indent = 2`). All `color = 0`, `brightness = 128`.

- **Presence TextBox** (right side, `bottomtotop = false`,
  `uid = 9`): rebuilt on every Tick when `presencechanged` is set.
  Rows are grouped by `PresenceEntry.status` (0 = In Lobby,
  1 = Pregame, 2 = Playing), sorted alphabetically within group.
  Each group prefixed with a header line at `brightness = 128 + 32 = 160`,
  individual rows at the standard `brightness = 128`. Names of
  players in a non-zero `gameID` get their game's name appended
  in brackets — `"Krieg [Casual Match #1]"`.

  For the `-demo` seed plus the local user (`dump`):

  ```
  In Lobby                  ← header, brightness 160
  Ember                     ← brightness 128
  Halcyon
  Solace
  Vector
  dump                      ← self
  Pregame                   ← header
  Quill [Capture the Tag]
  Playing                   ← header
  Krieg [Casual Match #1]
  ```

  (Sort order is alphabetical within each group;
  `dump` lands after Vector because lowercase 'd' (0x64) > 'V' (0x56).)

- **Chat TextInput**: empty `text`, **focused**. Caret visible at
  `(18, 437)` blinking on the standard `state_i % 32 < 16` cycle.

### Top bar (top-level Interface)

- **Brand text**: `"Silencer"` at `(15, 32)`, bank 135 advance 11,
  `effectcolor = 152` (red).
- **Version text**: `"v.<world.version>"` (default build:
  `"v.00028"`) at `(115, 39)`, bank 133 advance 6,
  `effectcolor = 189` (orange).
- **Map name overlay**: empty. (Only set when joining a game.)

### Empty / not-shown elements

- Five game-info overlays (uid 1..5 in the game-select sub-interface):
  empty unless a SelectBox row is selected. Default `selecteditem = -1`.
- ScrollBar chrome on both sub-interfaces: `draw = false`.

## Empty-state dump (no `-demo`)

Without the `-demo` flag, every dynamic overlay/textbox is empty —
the screen renders just the chrome (background, borders, brand bar,
buttons, agency icons, caret). See git commit `d431611` for an
empty-state subagent run that converged in one shot. Useful as a
"chrome-only" sanity check; not exercised by default.

## EffectColor + textcolorramp combination

This is the first hydrated screen using both `effectcolor != 0` and
`textcolorramp = true`. See `Renderer::DrawText` in
[font.md](font.md): when `color != 0` and `rampcolor == true`,
`EffectRampColor()` runs (instead of the more common `EffectColor()`):
each non-zero source pixel maps to a new palette index by combining
the **source pixel's brightness level** (its position within its
16-color ramp) with the **target color's group**:

```
output = ((src - 2) % 16) + (((tgt - 2) / 16) * 16) + 2
```

Then `EffectBrightness(=160)` brightens the result.

For `effectcolor = 129` and `effectbrightness = 160` on the lobby's
text overlays (map name, character stats), this produces a
softly-bright color in palette-2's group containing index 129 — a
greenish-cyan band — slightly overshooting neutral brightness for a
"highlighted" look. The rendered text in the reference dump comes
out as a muted teal.

A hydration using just `EffectColor` (without `textcolorramp`)
will produce a flat-color result instead of a ramped one. For the
LOBBY screen specifically, every text overlay using these params has
empty `text` in the default-config dump, so the difference doesn't
show on screen — but it WILL surface as soon as a dump captures live
content (e.g. with a populated chat).

## Focus / input wiring

- Top-level `tabobjects` — empty (the parent doesn't tab between
  its sub-interfaces; each sub manages its own).
- Top-level `activeobject = chatinterface.id`.
- Top-level `buttonescape = exitbutton.id` — Esc returns to MAINMENU.
- Chat sub-interface `tabobjects = [chatinput]`,
  `activeobject = chatinput.id`, `scrollbar = chatscrollbar.id`.
- Game-select sub-interface `buttonenter = gamejoinbutton.id`,
  `scrollbar = gamescrollbar.id`. No tabobjects (mouse-driven).
- Character sub-interface — no tabobjects, no scrollbar.

## QA dump

```
SILENCER_DUMP_STATE=LOBBY \
SILENCER_DUMP_PATH=/tmp/real_lobby.ppm \
  ./Silencer.app/Contents/MacOS/Silencer
```

Two paths exist:

1. **Real auth (preferred).** Run `services/lobby/silencer-lobby
   -addr :15170 -version 00028 -db /tmp/silencer-lobby-data/lobby.json
   -game-binary <path>`; write the matching port into
   `~/Library/Application Support/Silencer/config.cfg` (`lobbyport
   = 15170`). The dump path injects `dump`/`dump` credentials into
   the LOBBYCONNECT TextInputs and synthesizes a Login click; the
   lobby auto-creates the account on first connect; the natural
   AUTHENTICATED → LOBBY transition fires; dump fires on FadedIn.

2. **Auth-bypass (fallback when no lobby is reachable).** A previous
   version of `Game::Present` forced `world.lobby.state =
   AUTHENTICATED` and called `GoToState(LOBBY)` directly, plus
   overrode the LOBBY DISCONNECTED-bounce check. Visually
   indistinguishable from path 1 because both produce an empty
   LOBBY shell. Available in git history if needed.

Hydration writes `${SILENCER_DUMP_DIR}/lobby.ppm`.

## What this screen reuses + what's new

Reused: every existing widget and most of the substrate.
New on this screen:

- `B156x21` button variant (sprite-backed, brightness-only
  animation; see [widget-button.md](widget-button.md)).
- `Toggle` widget (radio-group icon picker; see
  [widget-toggle.md](widget-toggle.md)).
- `SelectBox` widget (scrollable list; see
  [widget-selectbox.md](widget-selectbox.md)).
- Sub-interfaces — Interface as a child of another Interface (see
  [widget-interface.md](widget-interface.md) "Sub-interfaces").
- New sprite indices in bank 7 (idx 1, 8, 11, 14, 24) and bank
  181 (idx 0..4) — see [sprite-banks.md](sprite-banks.md).
- `EffectColor + textcolorramp` text rendering combo — empty in
  the default dump but documented above.
