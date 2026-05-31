#pragma once

// build_draw_command_list: the tree -> tagged-union DrawCommandList IR
// transcriber (styling/render design §8). Walks the retained UiTree depth-first
// and emits the IR (draw_command.h): colors are PREMULTIPLIED at emit, variable
// data goes to the list's arenas, and box paint is split into a Rect (fill)
// command plus a fused Border command (border + focus outline). Reads
// node.visual exclusively (the legacy node.style paint fallback is gone).

#include "draw_command.h"
#include "tree.h"

namespace ui {

bool build_draw_command_list(const UiTree &tree, DrawCommandList *out,
                             NodeId focused_id = 0);

} // namespace ui
