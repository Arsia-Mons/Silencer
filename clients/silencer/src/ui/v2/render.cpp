#include "render.h"

#include "context.h"
#include "node.h"

#include "renderer.h"
#include "resources.h"
#include "surface.h"

namespace ui {
namespace v2 {

namespace {

// Static chrome facts per ButtonType — mirrors clients/silencer/src/ui/
// components/button.cpp `Button::SetType` + `Button::GetTextOffset`. Kept
// inline here (rather than reaching into the legacy Button class) so
// rendering doesn't depend on Object/World; the standalone preview
// harness can call Render() with just a Resources and a Surface.
struct ButtonChrome {
	Uint8 bank;          // 0xFF = no chrome (B52x21)
	Uint8 base_index;    // INACTIVE chrome frame
	int   width;
	int   height;
	Uint8 text_bank;
	Uint8 text_width;
	int   text_yoff;
	int   text_xoff_extra;
};

ButtonChrome ChromeFor(ButtonType type) {
	switch(type){
		case ButtonType::B112x33: return {6,    28, 112, 33, 135, 11, 8, 0};
		case ButtonType::B220x33: return {6,    23, 220, 33, 135, 11, 8, 0};
		case ButtonType::B196x33: return {6,     7, 196, 33, 135, 11, 8, 0};
		case ButtonType::B236x27: return {6,     2, 236, 27, 135, 11, 8, 0};
		case ButtonType::B52x21:  return {0xFF,  0,  52, 21, 133,  7, 8, 1};
		case ButtonType::B156x21: return {7,    24, 156, 21, 134,  8, 4, 0};
	}
	return {6, 7, 196, 33, 135, 11, 8, 0};
}

void RenderNode(const Node & n, const Context & ctx, Surface & target, Renderer & renderer)
{
	switch(n.kind){
		case NodeKind::Group:
			break;

		case NodeKind::Background:
		case NodeKind::Sprite:
			renderer.DrawSpriteAt(&target, n.sprite_bank, n.sprite_index, n.x, n.y);
			break;

		case NodeKind::Label:
			// Defaults match the legacy Overlay-text path:
			// alpha=false, tint=0, brightness=128, rampcolor=false.
			renderer.DrawText(&target, (Uint16)n.x, (Uint16)n.y,
			                  n.text.c_str(), n.text_bank, n.text_width);
			break;

		case NodeKind::Button: {
			ButtonChrome c = ChromeFor(n.button_type);
			if(c.bank != 0xFF){
				renderer.DrawSpriteAt(&target, c.bank, c.base_index, n.x, n.y);
			}
			// Text centering math = Button::GetTextOffset, evaluated at
			// the chrome's anchor offset so the label lands inside the
			// pill regardless of where the anchor lives in the asset.
			int text_len = (int)n.text.size();
			int xoff = (c.width - text_len * c.text_width) / 2 + c.text_xoff_extra;
			int yoff = c.text_yoff;
			Sint16 text_x = (Sint16)(n.x + xoff);
			Sint16 text_y = (Sint16)(n.y + yoff);
			if(c.bank != 0xFF){
				text_x = (Sint16)(text_x - ctx.resources.spriteoffsetx[c.bank][c.base_index]);
				text_y = (Sint16)(text_y - ctx.resources.spriteoffsety[c.bank][c.base_index]);
			}
			// alpha=true matches the legacy Button label render path
			// (renderer.cpp:841). Brightness is the INACTIVE default
			// (128); hover-state ramping lives with the input rewrite.
			renderer.DrawText(&target, (Uint16)text_x, (Uint16)text_y,
			                  n.text.c_str(), c.text_bank, c.text_width,
			                  /*alpha=*/true, /*tint=*/0, /*brightness=*/128);
			break;
		}
	}

	for(const Node & child : n.children){
		RenderNode(child, ctx, target, renderer);
	}
}

}  // namespace

void Render(const Node & root, const Context & ctx, Surface & target, Renderer & renderer)
{
	RenderNode(root, ctx, target, renderer);
}

}  // namespace v2
}  // namespace ui
