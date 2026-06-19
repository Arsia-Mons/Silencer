# Chat virtualization — infinite scrollback (lobby + in-game)

Issue #299. Depends on #297/#298 (render-pipeline totality).

## Problem

The chat is drawn as **one Text node**. `lobby_ui_model.cpp` joins the whole
`lobbyChatLog` (a `vector<string>`) into a single `'\n'`-separated string, caps
it at 2048 bytes, and hands it to one Text node. That node then hits two hard
per-node caps:

- `UI_RETAINED_VALUE_CAP` = 640 bytes — the retained tree truncates the node's
  text value, so the chat never holds more than ~640 bytes regardless.
- `UI_MAX_TEXT_LINES` = 32 — the measurer caps a node at 32 wrapped lines.

So the chat cannot hold or scroll real history. (Totality #298 stopped the
overflow from corrupting sibling panels, but the history limit remains.)

## Why a windowed list is mandatory, not optional

The retained tree caps the **whole UI** at `UI_RETAINED_MAX_NODES` = 1024 nodes.
A chat with thousands of messages cannot put a node per message in the tree. The
only way to hold large scrollback is to **materialize only the rows visible in
the viewport** (plus a small overscan) — windowing / virtualization. This is how
Discord/Slack render chat.

## Approach

Render the chat as a **virtualized list of per-message rows**:

1. **Data** — the chat hooks expose the message **list**, not a joined string.
   `LobbyChat.scrollback: string` → `messages: vector<string>`. The in-game chat
   hook (`use_ingame_chat`) already carries `log: vector<string>`. The retained
   history is a bounded ring (last `N` messages) so the source is bounded; `N`
   set generously (chat history people actually scroll). Drop the 2048-byte /
   640-byte flattening in `lobby_ui_model.cpp`.

2. **ScrollView variable-height virtualization** — the existing `row_height`
   path windows *uniform*-height rows. Chat messages wrap to 1/2/3 lines, so add
   a variable-height path: the caller passes a parallel `row_heights` array;
   `ScrollView` builds cumulative offsets, maps the scroll offset to the first
   visible row, renders `[first, last)` + overscan, and positions the content
   track by the cumulative offset of `first`. Content height = total of all row
   heights. Uniform `row_height` path stays for fixed-height lists.

3. **Row component + measurement** — each message is a row (a Box wrapping a
   Text node). The chat measures each message's wrapped height at the well width
   via the injected `ui::text_measurer()` (SDL-free, the same measurer layout +
   paint use) to feed `row_heights`. Measuring the bounded ring each frame is
   sub-millisecond; memoize per (message, wrap-width) if needed.

4. **Lobby chat** (`ChatLog` in `lobby_components.cppx`) is the scrollable
   transcript that gets virtualized. The **in-game chat is out of scope**: it is
   a transient fixed-size HUD overlay (origin `hud_chat_overlay` parity), bounded
   to the last 4 lines at the source (`world_session_model` `kMaxLog`), already
   one node per line and non-scrolling — no giant-text-node / overflow class, so
   virtualization does not apply (and would break parity). Totality (#298)
   already protects the HUD from the abort-drop class.

## Decisions

- **Variable height, measured** (not fixed-height-with-clip): a real chat must
  show full multi-line messages. Heights come from the injected measurer.
- **Bounded history ring** at the data layer: "infinite" in UX terms = "scroll
  plenty of history without breaking," and the node cap forces a window anyway.
  A few hundred to low-thousands retained is plenty and keeps measurement cheap.
- **stick-to-bottom preserved**: the well stays pinned to the newest message and
  un-pins when the user scrolls up, re-pins on return to bottom (already in
  `ScrollView`).

## Verification

- E2E: spam a few hundred messages, confirm (a) the newest are shown pinned to
  the bottom, (b) scrolling up reaches old history, (c) only a small window of
  row nodes exists (no node-cap overflow / commit errors), (d) sibling panels
  persist. Drive the real binary via the CLI (fake-player chat spam), capture
  frames.
- No regression on the lobby/in-game screens at rest.
