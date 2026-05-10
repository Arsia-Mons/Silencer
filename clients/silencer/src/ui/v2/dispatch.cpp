#include "dispatch.h"

#include "button_chrome.h"
#include "context.h"
#include "node.h"

#include "resources.h"

namespace ui {
namespace v2 {

bool ButtonHit(const Node & n, const Context & ctx)
{
	if(n.kind != NodeKind::Button) return false;
	if(ctx.mouse_x < 0 || ctx.mouse_y < 0) return false;

	ButtonChrome c = ChromeFor(n.button_type);

	// Anchor offset matches the sprite the chrome draws with. For B52x21
	// (no chrome, bank=0xFF) the legacy MouseInside indexes spriteoffsetx
	// out of bounds — use 0 here so the rect is just (x, y, w, h).
	int off_x = 0, off_y = 0;
	if(c.bank != 0xFF){
		off_x = ctx.resources.spriteoffsetx[c.bank][c.base_index];
		off_y = ctx.resources.spriteoffsety[c.bank][c.base_index];
	}

	int x1 = n.x - off_x;
	int x2 = x1 + c.width;
	int y1 = n.y - off_y;
	int y2 = y1 + c.height;

	return ctx.mouse_x > x1 && ctx.mouse_x < x2 &&
	       ctx.mouse_y > y1 && ctx.mouse_y < y2;
}

namespace {

void DispatchInto(const Node & n, const Context & ctx)
{
	if(n.kind == NodeKind::Button && n.on_click && ButtonHit(n, ctx)){
		n.on_click();
	}
	for(const Node & child : n.children){
		DispatchInto(child, ctx);
	}
}

}  // namespace

void DispatchClick(const Node & root, const Context & ctx)
{
	DispatchInto(root, ctx);
}

}  // namespace v2
}  // namespace ui
