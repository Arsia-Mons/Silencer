# Silencer UI Editor

The admin web UI editor is a WYSIWYG layout tool for Silencer client UI
surfaces. It lives at `/ui-editor` and edits versioned
`.silencer-ui.json` documents that model the same Clay-style layout concepts
used by `clients/silencer/src/client/ui`: stable element IDs, flex direction,
grow/fit/fixed sizing, padding, gaps, alignment, color tokens, text, buttons,
inputs, and containers.

## Current Surface

- Palette for adding panel, stack, row, text, button, input, and spacer nodes.
- Hierarchy tree for selecting layout nodes by stable ID and dragging existing
  nodes to reparent or reorder them.
- Live scaled browser fallback for common Silencer viewport sizes.
- Live client preview through the Silencer control socket: each edit is sent to
  the running client, rendered by the production `ClientUi`/Clay frame, captured
  as a screenshot, and returned with `inspect` bounds for editor selection
  outlines. Existing elements can also be dragged from those live overlay
  bounds onto another element to move them in the layout tree.
- Inspector controls for identity, text/action data, sizing, flex layout,
  padding, gap, radius, colors, and font family.
- JSON import and download for `.silencer-ui.json` documents.
- Clay scaffold output for turning the edited tree into client UI code.

## Data Model

The editor data model is implemented in `web/admin/lib/ui-layout.ts`. The root
document contains:

- `schemaVersion` — currently `1`.
- `surface` — slug used for exported filenames and generated function names.
- `viewport` — preview dimensions in pixels.
- `root` — a recursive `UiNode` tree.

Every node has a stable `id`, `kind`, display `name`, `style`, and optional
content fields such as `text`, `placeholder`, and `action`. Container nodes
(`screen`, `panel`, `stack`, `row`) own children; leaf nodes do not.

## Runtime Integration

`clients/silencer/src/client/ui/screens/ui_editor_preview_screen.*` maps the
editor document into real Silencer UI primitives inside the existing
`ClientUi`/Clay frame. The admin route at
`web/admin/app/api/ui-editor/preview/route.ts` sends documents to the client
with the `ui_editor_preview_capture` control-socket op. That single game-thread
operation validates the document, renders the preview, captures a real client
screenshot, and returns the screenshot plus `inspect` metadata.

The browser preview is only a fallback when no client is listening on
`SILENCER_CONTROL_HOST` / `SILENCER_CONTROL_PORT`. The client screenshot is the
authoritative WYSIWYG preview.
