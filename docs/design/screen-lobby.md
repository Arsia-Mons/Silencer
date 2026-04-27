# screen-lobby — Lobby (post-authentication)

The post-auth lobby. Renders sub-palette 2 with bank-7 idx-1 panel
chrome, a top header bar (Silencer / version / map / Go Back), and
three sub-interfaces: **CharacterInterface** (left, agency + player
profile), **GameSelectInterface** (right, tabbed Create Game /
Active Games + Join Game), **ChatInterface** (bottom-left, channel
chat).

**Scope of this spec:** chrome + header + sub-interface bounding
boxes (the structural skeleton). Sub-interface internal contents
(agency toggles, game list rows, chat messages, character stats)
are **runtime-driven** and warrant their own per-sub-interface specs
(`screen-lobby-character.md`, `screen-lobby-gameselect.md`,
`screen-lobby-chat.md`) — explicitly out of scope here.

Reference dump: `/tmp/real_lobby_dump.ppm` (640×480 P6, sub-palette
2, captured via `SILENCER_DUMP_STATE=LOBBY` after the harness sets
`world.lobby.state = Lobby::AUTHENTICATED` to bypass the
engine's bounce-back-to-LOBBYCONNECT path. The reference therefore
shows an "empty" lobby — no chat messages, no games in the list,
no map selected — exactly the structural skeleton this spec gates).

## Sub-palette

`2` (lobby palette — same as LOBBYCONNECT). The candidate already
has the palette decoder validated correctly via the LOBBYCONNECT L1
fix.

## Object inventory (chrome + header only)

In z-order:

| z | Object | Type | Bank | Index | x | y | Notes |
| - | --- | --- | --- | --- | --- | --- | --- |
| 0 | Background panel  | overlay | 7 | 1 | 0 | 0 | Full-screen lobby panel chrome — distinct from LOBBYCONNECT idx 2 and CONTROLS idx 7. Includes corner LED ornaments and the framing of all three sub-interfaces. |
| 1 | "Silencer" header | overlay (font 135, w=11, color=152) | — | — | 15 | 32 | text=`Silencer` |
| 2 | Version text      | overlay (font 133, w=6, color=189)  | — | — | 115 | 39 | text=`v.<world.version>` (runtime: shows actual build version) |
| 3 | Map name text     | overlay (font 135, w=11, color=129, brightness 128+32, ramp=true) | — | — | 180 | 32 | uid=8. Runtime-populated; empty on canonical dump. |
| 4 | Exit button       | B156x21 | — | — | 473 | 29 | text=`Go Back`, uid=10 |

## Sub-interface bounding boxes

These are spec'd as opaque rectangles. Their internal contents are
out of scope; the candidate must render the bank-7 idx-1 panel chrome
correctly (which surrounds these regions), and **may render the
sub-interfaces as empty regions** for this Ralph's gates.

| Sub-interface | Bounding box (engine values) |
| --- | --- |
| CharacterInterface  | x=10, y=64, width=217, height=120 |
| GameSelectInterface | (engine-internal, occupies right ~60% of the screen, tabbed UI with Create Game / Active Games tabs) |
| ChatInterface       | (engine-internal, occupies bottom-left below CharacterInterface) |

The `width`/`height` for GameSelectInterface and ChatInterface are
defined in their respective `Create*Interface` engine functions —
**spec gap:** `screen-lobby-gameselect.md` and `screen-lobby-chat.md`
need to be authored from those before deeper Ralphs target the
sub-interface contents.

For *this* Ralph, gating only on chrome + header is sufficient.

## What's NOT on this screen

- No starfield/planet bank-6 idx-0 background. The bank-7 idx-1
  panel sprite covers the entire framebuffer.
- No bank-208 logo overlay.
- No menu-style background.

## New widget compared to prior screens

### B156x21 button variant

A wider button (~156 × 21 px) used for the lobby's Go Back action.
Same anchor convention as B196x33 / B220x33 / B112x33 / B52x21.
Spec gap: extend [`widget-button.md`](widget-button.md) to list this
variant.

## Activation / state

Static-enough for the dump modulo:
- Map name text (runtime; empty on dump).
- Sub-interface contents (chat scrollback, game list, agency
  selection — all runtime-driven and out of scope).

The dump fires after a 60-tick settle pin in LOBBY state. With
`world.lobby.state = Lobby::AUTHENTICATED` forced by the harness, the
engine does not bounce back to LOBBYCONNECT.

Buttons render in INACTIVE state at dump time.

## Spec gaps to flag

- `widget-button.md` — add B156x21.
- `screen-lobby-character.md` — sub-interface for left panel (player
  profile + agency toggles, bank 181 toggle widget per
  `CreateCharacterInterface`).
- `screen-lobby-gameselect.md` — sub-interface for right panel (Create
  Game / Active Games tabs, Join Game button, scrollable game list).
- `screen-lobby-chat.md` — sub-interface for bottom-left panel
  (channel chat scrollback + input).
- `palette.md` — explicitly document sub-palette 2 as the lobby
  palette (already empirically validated by LOBBYCONNECT L1).
- `sprite-banks.md` — bank 7 idx 1 is yet another distinct panel
  sprite; co-document with idx 2 (LOBBYCONNECT) and idx 7 (CONTROLS).

## Auth-bypass mechanism (engine harness)

`clients/silencer/src/game.cpp::Game::Present` includes a
`target_state == LOBBY` branch that forces
`world.lobby.state = Lobby::AUTHENTICATED` every frame, preventing
the engine's `if (lobby.state == DISCONNECTED) GoToState(LOBBYCONNECT)`
bounce. This is **dump-mode only** and inactive when
`SILENCER_DUMP_PATH` is unset. Documented here so future maintainers
of the harness understand the dependency.

## Cross-references

- [`screen-lobby-connect.md`](screen-lobby-connect.md) — predecessor; same sub-palette, different panel sprite (bank 7 idx 2)
- [`widget-button.md`](widget-button.md) — B156x21 (extend)
- [`widget-overlay.md`](widget-overlay.md), [`palette.md`](palette.md), [`sprite-banks.md`](sprite-banks.md), [`tick.md`](tick.md)
