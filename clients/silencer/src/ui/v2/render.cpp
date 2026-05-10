#include "render.h"

#include "button_chrome.h"
#include "context.h"
#include "dispatch.h"
#include "node.h"
#include "ui_state.h"

#include "renderer.h"
#include "resources.h"
#include "surface.h"

namespace ui {
namespace v2 {

namespace {

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
			bool hovered = ButtonHit(n, ctx);

			// hot_t: 0..1 hover-toward-1, exponentially approached. When
			// `ctx.state` is NULL (PPM dump path), we snap — keeps the
			// dump byte-identical to the legacy widget render. The state
			// path is the interactive preview / live game.
			float hot_t = hovered ? 1.0f : 0.0f;
			if(ctx.state && !n.key.empty()){
				uint64_t id = HashKey(n.key) ^ TAG_HOT;
				float & slot = ctx.state->AnimSlot(id, 0.0f);
				slot = Approach(slot, hovered ? 1.0f : 0.0f, /*rate=*/12.0f, ctx.dt);
				hot_t = slot;
			}

			// hot_t == 0 → INACTIVE chrome, brightness 128 (matches legacy).
			// hot_t == 1 → ACTIVE chrome (base + 4), brightness 136
			//              (matches legacy `effectbrightness = 128 + 4*2`).
			// Intermediate t ramps the integer res_index across the four
			// ACTIVATING frames, mirroring legacy `Button::Tick` ACTIVATING.
			int   step       = (int)(hot_t * 4.0f + 0.0001f);
			if(step > 4) step = 4;
			Uint8 res_idx    = (Uint8)(c.base_index + step);
			Uint8 brightness = (Uint8)(128 + step * 2);
			if(c.bank != 0xFF){
				renderer.DrawSpriteAt(&target, c.bank, res_idx, n.x, n.y);
			}
			// Text centering math = Button::GetTextOffset, evaluated at
			// the chrome's anchor offset so the label lands inside the
			// pill regardless of where the anchor lives in the asset. Use
			// the *current* frame's offset to mirror legacy GetTextOffset
			// (which reads spriteoffsetx[res_bank][res_index]).
			int text_len = (int)n.text.size();
			int xoff = (c.width - text_len * c.text_width) / 2 + c.text_xoff_extra;
			int yoff = c.text_yoff;
			Sint16 text_x = (Sint16)(n.x + xoff);
			Sint16 text_y = (Sint16)(n.y + yoff);
			if(c.bank != 0xFF){
				text_x = (Sint16)(text_x - ctx.resources.spriteoffsetx[c.bank][res_idx]);
				text_y = (Sint16)(text_y - ctx.resources.spriteoffsety[c.bank][res_idx]);
			}
			renderer.DrawText(&target, (Uint16)text_x, (Uint16)text_y,
			                  n.text.c_str(), c.text_bank, c.text_width,
			                  /*alpha=*/true, /*tint=*/0, brightness);
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
