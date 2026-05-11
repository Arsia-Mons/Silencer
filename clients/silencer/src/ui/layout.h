#ifndef SILENCER_UI_V2_LAYOUT_H
#define SILENCER_UI_V2_LAYOUT_H

namespace ui {
namespace v2 {

struct Node;
struct Context;

// Compute rectangles for every container/leaf-in-container node in the
// tree using the vendored Clay layout engine. Writes results into each
// node's `rect_x/y/w/h` fields. Nodes outside any container subtree are
// left untouched — render + dispatch fall back to absolute `.at()`
// positioning when `rect_w == 0`.
//
// Initializes Clay on first call (one process-lifetime arena, ~64 KB).
// Subsequent calls reuse the same arena across frames; the layout API
// is rebuild-every-frame so there is no retained tree to invalidate.
void Layout(Node & root, const Context & ctx);

// Apply pointer state + scroll-wheel deltas to Clay before the next Layout()
// pass. Call once per frame from the interactive caller. `scroll_dy` is the
// signed wheel-delta in logical pixels for this frame (0 if no event). The
// `dt` field on `ctx` drives Clay's momentum scrolling — callers passing
// dt=0 (PPM-dump path) get instant-snap scrolling, which is fine since
// scroll only fires from explicit user input.
void BeforeLayout(const Context & ctx, float scroll_dy);

}  // namespace v2
}  // namespace ui

#endif
