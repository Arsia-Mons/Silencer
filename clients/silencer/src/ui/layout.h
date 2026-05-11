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
// tree using the vendored Clay layout engine. Writes results into each
// node's `rect_x/y/w/h` fields. Nodes outside any container subtree are
// left untouched — render + dispatch fall back to absolute `.at()`
// positioning when `rect_w == 0`.
void Layout(Node & root, const Context & ctx);

}  // namespace v2
}  // namespace ui

#endif
