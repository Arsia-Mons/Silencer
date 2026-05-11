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
		case NodeKind::VStack:
		case NodeKind::HStack:
		case NodeKind::Center:
		case NodeKind::Padding:
		case NodeKind::Spacer:
			// Containers and Group draw nothing of their own. Their
			// computed rect is consumed by descendant nodes.
			break;

		case NodeKind::Background:
		case NodeKind::Sprite:
			renderer.DrawSpriteAt(&target, n.sprite_bank, n.sprite_index, n.x, n.y);
			break;

		case NodeKind::FilledRect:
			renderer.DrawFilledRectangle(&target, n.x, n.y,
			                             n.x + (int)n.fill_w,
			                             n.y + (int)n.fill_h,
			                             n.fill_color);
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

			// Two coordinate systems exist:
			//   - Layout-managed (rect_w > 0): rect_x/rect_y is the chrome's
			//     screen-pixel top-left. DrawSpriteAt expects an *anchor*,
			//     and the sprite is drawn at (anchor - baked_offset). So
			//     anchor = rect_top_left + baked_offset to land the chrome.
			//   - Absolute (rect_w == 0): n.x/n.y is the anchor directly —
			//     the legacy `.at()` convention preserved for screens that
			//     still need pixel-identical parity with the legacy widget.
			Sint16 anchor_x, anchor_y;
			Sint16 box_x,    box_y;
			if(n.rect_w > 0){
				box_x    = n.rect_x;
				box_y    = n.rect_y;
				anchor_x = (Sint16)(box_x + (c.bank != 0xFF ? ctx.resources.spriteoffsetx[c.bank][res_idx] : 0));
				anchor_y = (Sint16)(box_y + (c.bank != 0xFF ? ctx.resources.spriteoffsety[c.bank][res_idx] : 0));
			}else{
				anchor_x = n.x;
				anchor_y = n.y;
				box_x = (Sint16)(n.x - (c.bank != 0xFF ? ctx.resources.spriteoffsetx[c.bank][res_idx] : 0));
				box_y = (Sint16)(n.y - (c.bank != 0xFF ? ctx.resources.spriteoffsety[c.bank][res_idx] : 0));
			}

			if(c.bank != 0xFF){
				renderer.DrawSpriteAt(&target, c.bank, res_idx, anchor_x, anchor_y);
			}
			// Text centering math (mirrors Button::GetTextOffset). Centered
			// inside the box, so we compute against the chrome's screen
			// top-left — no extra anchor-offset correction needed.
			int text_len = (int)n.text.size();
			int xoff = (c.width - text_len * c.text_width) / 2 + c.text_xoff_extra;
			int yoff = c.text_yoff;
			Sint16 text_x = (Sint16)(box_x + xoff);
			Sint16 text_y = (Sint16)(box_y + yoff);
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
