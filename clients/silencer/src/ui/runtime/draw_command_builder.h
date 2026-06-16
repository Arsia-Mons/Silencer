#pragma once

// build_draw_command_list: the tree -> tagged-union DrawCommandList IR
// transcriber (styling/render design §8). Walks the retained UiTree depth-first
// and emits the IR (draw_command.h): colors are PREMULTIPLIED at emit, variable
// data goes to the list's arenas, and box paint is split into a Rect (fill)
// command plus a fused Border command (border + focus outline). Reads
// node.visual exclusively (the legacy node.style paint fallback is gone).

#include "draw_command.h"
#include "tree.h"

#include <unordered_map>

namespace ui {

// Persistent per-input horizontal scroll offsets (logical px), keyed by node id
// and owned by the caller ACROSS frames. With it the focused single-line field
// scrolls MINIMALLY — the window only shifts when the caret would leave it, so
// arrowing left keeps the rightmost glyph in view instead of dragging the whole
// value with the caret. Pass nullptr (the default, e.g. tests/goldens) for the
// stateless right-pinned fallback.
using InputScrollStore = std::unordered_map<NodeId, float>;

bool build_draw_command_list(const UiTree &tree, DrawCommandList *out,
                             NodeId focused_id = 0,
                             InputScrollStore *input_scroll = nullptr);

} // namespace ui
