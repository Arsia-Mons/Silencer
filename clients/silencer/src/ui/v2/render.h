#ifndef SILENCER_UI_V2_RENDER_H
#define SILENCER_UI_V2_RENDER_H

class Surface;
class Renderer;

namespace ui {
namespace v2 {

struct Node;
struct Context;

// Walk a Node tree depth-first and draw each renderable node into
// `target` using `renderer` for text and `ctx.resources` for sprite
// lookups. Node positions are interpreted exactly like the legacy
// widgets — sprite-asset offsets are honored — so a screen built with
// the same (x, y) values as today produces the same pixels.
//
// Scale handling and hit-testing are not part of this entry point;
// they land alongside the standalone preview harness and the input
// rewrite respectively.
void Render(const Node & root, const Context & ctx,
            Surface & target, Renderer & renderer);

}  // namespace v2
}  // namespace ui

#endif
