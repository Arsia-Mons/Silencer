#ifndef SILENCER_UI_V2_DISPATCH_H
#define SILENCER_UI_V2_DISPATCH_H

namespace ui {
namespace v2 {

struct Node;
struct Context;

// Returns true iff (ctx.mouse_x, ctx.mouse_y) lies inside this Button's
// chrome rect, in logical pixels. Mirrors clients/silencer/src/ui/components/
// button.cpp `Button::MouseInside` — the rect accounts for the sprite asset's
// baked anchor offset so authoring (x, y) match the rendered pill. Returns
// false for non-Button nodes and when ctx.mouse_x < 0 (no mouse this frame).
bool ButtonHit(const Node & button, const Context & ctx);

// Walk the tree depth-first and fire the on_click handler of any Button under
// the mouse cursor. Caller invokes this on the mouse-down edge (matching
// legacy semantics — `clicked = true` on mousedown, not mouseup). Children
// are visited after self in declaration order, so later siblings drawn on top
// also dispatch later — keep the hit-test tree visually layered if you care
// about overlap precedence.
void DispatchClick(const Node & root, const Context & ctx);

}  // namespace v2
}  // namespace ui

#endif
