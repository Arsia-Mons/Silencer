#ifndef SILENCER_UI_V2_LAYOUT_H
#define SILENCER_UI_V2_LAYOUT_H

#include "clay/clay.h"

namespace ui {
namespace v2 {

struct Node;
struct Context;

// Initialize Clay on first call (one process-lifetime arena) and bind it
// as the current Clay context. Idempotent. Runtimes call this once per
// Render() before calling Clay_SetPointerState / Clay_UpdateScrollContainers
// (which both need a live current context).
void EnsureClayContext(const Context & ctx);

// Compute rectangles for every container/leaf-in-container node in the
// tree using the vendored Clay layout engine. Records the Clay element
// id on each emitted Node (`n.clay_id`). Nodes outside any container
// subtree are left with `clay_id.id == 0` — render + dispatch fall back
// to absolute `.at()` positioning + ChromeFor dimensions in that case.
//
// Returns Clay's render-command array for the laid-out frame so the
// caller can feed it into `ui::DrawRenderCommands` (render_commands.h).
// Existing call sites that ignore the return value continue to work —
// the per-Node walker still consumes the same tree via `Render()`.
Clay_RenderCommandArray Layout(Node & root, const Context & ctx);

// Walk the tree depth-first and fire the on_click handler of any Button
// the pointer is over. Caller invokes this on the mouse-down edge after
// `Layout()` has populated each Node's `clay_id`. For layout-managed
// buttons the hit-test is `Clay_PointerOver(n.clay_id)` (so the caller
// must have set Clay's pointer state); for absolute `.at()` buttons,
// inline chrome-rect math against `ctx.mouse_x/y` matches the legacy
// MouseInside semantics — `Button` sprite-anchor offsets are applied to
// the rect, so authoring (x, y) matches the rendered pill. Children are
// visited after self in declaration order, so later siblings drawn on
// top also dispatch later.
void DispatchClicks(const Node & root, const Context & ctx);

}  // namespace v2
}  // namespace ui

#endif
