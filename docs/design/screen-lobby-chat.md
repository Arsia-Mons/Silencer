# screen-lobby-chat — Lobby ChatInterface (bottom-left panel)

The bottom-left sub-interface in `screen-lobby` — channel chat
scrollback, presence list, chat input. Bounding box `(x=15, y=216,
width=368, height=234)`.

Reference: `/tmp/real_lobby_dump.ppm`. This Ralph gates on the
ChatInterface region.

## Object inventory

| z | Object | Type | Bank | Index | x | y | Notes |
| - | --- | --- | --- | --- | --- | --- | --- |
| 0 | Chat border       | overlay | 7 | 11 | 0 | 0 | Chat-area chrome |
| 1 | Chat input border | overlay | 7 | 14 | 0 | 0 | Input-row chrome |
| 2 | Channel name      | overlay (font 134, w=8) | — | — | 15 | 200 | uid=1, runtime (e.g., `#general`) |
| 3 | Chat textbox      | textbox | 133 (font) | — | 19  | 220 | width=242, height=207, lineheight=11, fontwidth=6, **bottom-to-top** |
| 4 | Presence textbox  | textbox | 133 (font) | — | 267 | 220 | width=110, height=207, lineheight=11, fontwidth=6, top-to-bottom, uid=9 |
| 5 | Chat input        | textinput | 133 (font) | — | 18 | 437 | width=360, height=14, fontwidth=6, maxchars=200, uid=1 |
| 6 | Chat scrollbar    | scrollbar | (bank 7) | 12 (track) + 13 (thumb) | (engine-positioned) | — | scrollpixels=11, scrollposition=0 |

## Bottom-to-top textbox

The chat scrollback uses `bottomtotop=true` — new lines appear at the
bottom and push older ones up. For a static dump where the textbox
is empty (no chat messages without a running lobby), this distinction
doesn't matter — render an empty bordered region.

## What's runtime / non-structural

- Channel name text content (e.g., `#general`). Empty without server.
- Chat textbox content (chat messages). Empty without server.
- Presence textbox content (who's online). Empty without server.
- Chat input field (user-typed message). Empty.
- Chat scrollbar thumb position: scrollposition=0, thumb at top.

## Sprite indices to validate

Bank 7 idx 11 (chat border), idx 12 (scrollbar track), idx 13
(scrollbar thumb), idx 14 (chat input border). All distinct from
prior bank-7 sprites (idx 1 LOBBY, idx 2 LOBBYCONNECT, idx 7
OPTIONSCONTROLS, idx 8 GAMESELECT).

**Spec gap:** `sprite-banks.md` should enumerate bank 7's full sprite
inventory now that 5 distinct indices have been validated across the
hydration.

## Cross-references

- [`screen-lobby.md`](screen-lobby.md) — parent
- [`screen-lobby-connect.md`](screen-lobby-connect.md) — TextBox/TextInput pattern (validated)
- [`screen-options-controls.md`](screen-options-controls.md) — Scrollbar pattern (different bank/idx)
- [`palette.md`](palette.md), [`sprite-banks.md`](sprite-banks.md), [`tick.md`](tick.md)
