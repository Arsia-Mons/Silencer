# `TextBox` widget

**Source:** `clients/silencer/src/textbox.h`,
`clients/silencer/src/textbox.cpp`,
render dispatch in `clients/silencer/src/renderer.cpp:783..807`.

A multi-line text display. No editing — TextBox is "log scrollback"
shaped: callers push lines via `AddLine` / `AddText`, the widget
truncates the oldest when `maxlines` is exceeded, and the renderer
draws as many lines as fit in `height / lineheight` starting from
`scrolled`.

The lobby-connect screen uses one to display connection-status
messages and the server's MOTD.

## Properties

| Field | Type | Default | Notes |
| ----- | ---- | ------- | ----- |
| `x`, `y` | i16 | `0`, `0` | Top-left of the rendered text area, in screen coords. |
| `width`, `height` | u16 | `100`, `100` | Pixel bounds. |
| `res_bank` | u8 | `133` | Font bank used for line text. |
| `lineheight` | u8 | `11` | Pixels between line origins. |
| `fontwidth` | u8 | `6` | Glyph advance for line text. |
| `bottomtotop` | bool | `false` | When true, lines fill from the bottom of the box upward (used by chat overlays). |
| `scrolled` | u16 | `0` | Index of the first line to render. Updated by `AddLine` to keep the most-recent line visible. |
| `maxlines` | u16 | `256` | Cap on retained history; older lines drop off the front. |
| `text` | `deque<vector<char>>` | empty | Per-line storage; each element is a `\0`-terminated char buffer with two extra bytes after the terminator: `[chars... \0 color brightness]`. |

## `AddLine(string, color = 0, brightness = 128, scroll = true)`

Per `textbox.cpp:23..44`:

1. If `text.size() > maxlines`, drop the front (oldest) line.
2. If `scroll`, recompute `scrolled` to keep the latest line visible:
   - `scrolled = max(0, text.size() - height / lineheight)`.
3. Truncate `string` to fit the box:
   `size = min(strlen(string), width / fontwidth)`.
4. Push a `vector<char>` of layout
   `[chars(size) \0 color brightness]` (size + 3 bytes).

Lines do **not** wrap — they're truncated at the first
`width / fontwidth` characters. Callers wanting wrap use `AddText`.

## `AddText(string, color = 0, brightness = 128, indent = 0, scroll = true)`

Calls `Interface::WordWrap(string, width / fontwidth, "\n" +
" " * indent)` then forwards each resulting line to `AddLine`. Used
for multi-paragraph content (e.g. announcement text) that needs to
respect the box width.

## Rendering

Per `renderer.cpp:783..807`:

```
line = 0
for each entry in text (in order):
    if entry_index < scrolled: skip
    y = textbox.y + (line * lineheight)
    if line > height / lineheight: stop
    if bottomtotop:
        size = min(text.size() * lineheight,
                   ceil(height / lineheight) * lineheight)
        y += height - size
    color = entry.bytes_after_null[0]
    brightness = entry.bytes_after_null[1]
    DrawText(surface, textbox.x, y, entry.chars,
             res_bank, fontwidth, alpha=false, color, brightness)
    line += 1
```

So:

- Lines render flush-left at `textbox.x`, vertically stacked at
  `textbox.y + i * lineheight`.
- Each line gets its own color/brightness from the trailing two
  bytes of its `vector<char>` storage.
- `bottomtotop = true` shifts the visible block down so the *latest*
  line is at `textbox.y + height - lineheight` instead of
  `textbox.y` (used by chat overlay; not used by lobby-connect).

## How lobby-connect uses it

The lobby-connect TextBox lives at `(185, 101)` with
`width=250, height=170`, `res_bank=133, lineheight=11, fontwidth=6`.
That's a 22-row × 41-char visible area
(`170 / 11 = 15` rows, `250 / 6 = 41` chars per row — actually 15
rows, not 22; let me re-do: `height / lineheight = 170 / 11 = 15`
visible rows).

Lines are added by `Game::ProcessLobbyConnectInterface` based on the
current `Lobby::state`. For a default-config dump (lobby host
unreachable at `127.0.0.1:517`), the typical content captured by
the time `FadedIn() == true` is one or more of:

```
Connecting to 127.0.0.1:517
Connection failed
```

…depending on how many ticks have run between state-entry and the
dump trigger. The exact number of lines is timing-sensitive; see
[screen-lobby-connect.md](screen-lobby-connect.md) for the dump
strategy.

A static-frame hydration that wants a deterministic dump can simply
seed the TextBox with `AddLine("Connecting to 127.0.0.1:517")` at
construction time and skip the network simulation.
