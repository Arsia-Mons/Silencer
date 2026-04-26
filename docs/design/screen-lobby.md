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

## Default visible content (nothing chat / no games)

For the default-config QA dump (auth via the local Go lobby with
fresh credentials), every dynamic field is empty:

- `localusername`, channel name, map name overlay text — empty.
- All four character stat overlays (level/wins/losses/etc) — empty.
- All five game-info overlays — empty.
- Chat TextBox, presence TextBox — empty.
- SelectBox — empty.
- All five agency Toggles render — Noxis (idx 0) at
  `effectbrightness=128`, the other four at `effectbrightness=32`.
- Chat input has caret blinking at its `(x, y) = (18, 437)` origin
  (since text is empty, caret renders at `(18, 436)`).

So a hydration only needs to render the static chrome (background,
borders, brand text, version, button labels, agency icons) plus the
single visible caret. No dynamic-content simulation required.

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
