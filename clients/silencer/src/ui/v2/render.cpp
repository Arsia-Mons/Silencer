#include "render.h"

#include "button_chrome.h"
#include "context.h"
#include "dispatch.h"
#include "nine_slice_meta.h"
#include "node.h"
#include "ui_state.h"

#include "renderer.h"
#include "resources.h"
#include "surface.h"

namespace ui {
namespace v2 {

namespace {

// Pull the screen-pixel rect for a node. Container layout (rect_w > 0) is
// preferred; absolute `.at()` + fill_w/fill_h is the fallback. Returns
// false if neither path gives a non-empty rect.
bool NodeRect(const Node & n, int & x, int & y, int & w, int & h){
	if(n.rect_w > 0){
		x = n.rect_x; y = n.rect_y; w = n.rect_w; h = n.rect_h;
		return w > 0 && h > 0;
	}
	x = n.x; y = n.y; w = n.fill_w; h = n.fill_h;
	return w > 0 && h > 0;
}

// Draw a 4-rim rect (code-drawn .stroke() modifier). Each rim is a filled
// rectangle of `width` thickness inset against the box.
void DrawStrokeRects(Renderer & renderer, Surface & target, int x, int y, int w, int h, Uint8 color, int width){
	if(width <= 0 || w <= 0 || h <= 0) return;
	if(width > w) width = w;
	if(width > h) width = h;
	renderer.DrawFilledRectangle(&target, x,             y,             x + w,         y + width,     color); // top
	renderer.DrawFilledRectangle(&target, x,             y + h - width, x + w,         y + h,         color); // bottom
	renderer.DrawFilledRectangle(&target, x,             y + width,     x + width,     y + h - width, color); // left
	renderer.DrawFilledRectangle(&target, x + w - width, y + width,     x + w,         y + h - width, color); // right
}

// Draw a 9-slice border around (dst_x, dst_y, dst_w, dst_h). Corner pieces
// are blitted 1:1; edges are tiled along their axis; the center is tiled
// across the remaining area. Tiling is pixel-perfect — no stretching, so
// pixel-art crispness is preserved at any size.
void DrawNineSlice(const Resources & res, Renderer & renderer, Surface & target,
                   Uint8 bank, Uint8 index, int dst_x, int dst_y, int dst_w, int dst_h)
{
	if(bank >= res.spritewidth.size()) return;
	if(index >= res.spritewidth[bank].size()) return;
	int sw = (int)res.spritewidth[bank][index];
	int sh = (int)res.spriteheight[bank][index];
	if(sw <= 0 || sh <= 0) return;

	NineSliceMeta m{};
	(void)LookupNineSlice(bank, index, m);
	int cl = m.corner_l, cr = m.corner_r, ct = m.corner_t, cb = m.corner_b;
	if(cl + cr >= sw){ cl = sw / 2; cr = sw - cl; }
	if(ct + cb >= sh){ ct = sh / 2; cb = sh - ct; }
	int inner_sw = sw - cl - cr;
	int inner_sh = sh - ct - cb;
	if(inner_sw < 1) inner_sw = 1;
	if(inner_sh < 1) inner_sh = 1;

	// Corners (1:1).
	renderer.DrawSpriteSubRect(&target, bank, index, 0,         0,         cl, ct, dst_x,                dst_y);
	renderer.DrawSpriteSubRect(&target, bank, index, sw - cr,   0,         cr, ct, dst_x + dst_w - cr,   dst_y);
	renderer.DrawSpriteSubRect(&target, bank, index, 0,         sh - cb,   cl, cb, dst_x,                dst_y + dst_h - cb);
	renderer.DrawSpriteSubRect(&target, bank, index, sw - cr,   sh - cb,   cr, cb, dst_x + dst_w - cr,   dst_y + dst_h - cb);

	// Top / bottom edges (tile horizontally).
	int inner_dw = dst_w - cl - cr;
	int inner_dh = dst_h - ct - cb;
	for(int xo = 0; xo < inner_dw; xo += inner_sw){
		int tile_w = (xo + inner_sw <= inner_dw) ? inner_sw : (inner_dw - xo);
		renderer.DrawSpriteSubRect(&target, bank, index, cl, 0,       tile_w, ct, dst_x + cl + xo, dst_y);
		renderer.DrawSpriteSubRect(&target, bank, index, cl, sh - cb, tile_w, cb, dst_x + cl + xo, dst_y + dst_h - cb);
	}
	// Left / right edges (tile vertically).
	for(int yo = 0; yo < inner_dh; yo += inner_sh){
		int tile_h = (yo + inner_sh <= inner_dh) ? inner_sh : (inner_dh - yo);
		renderer.DrawSpriteSubRect(&target, bank, index, 0,       ct, cl, tile_h, dst_x,             dst_y + ct + yo);
		renderer.DrawSpriteSubRect(&target, bank, index, sw - cr, ct, cr, tile_h, dst_x + dst_w - cr, dst_y + ct + yo);
	}
	// Center fill.
	if(m.has_center){
		for(int yo = 0; yo < inner_dh; yo += inner_sh){
			int tile_h = (yo + inner_sh <= inner_dh) ? inner_sh : (inner_dh - yo);
			for(int xo = 0; xo < inner_dw; xo += inner_sw){
				int tile_w = (xo + inner_sw <= inner_dw) ? inner_sw : (inner_dw - xo);
				renderer.DrawSpriteSubRect(&target, bank, index, cl, ct, tile_w, tile_h,
				                           dst_x + cl + xo, dst_y + ct + yo);
			}
		}
	}
}

void RenderNode(const Node & n, const Context & ctx, Surface & target, Renderer & renderer)
{
	switch(n.kind){
		case NodeKind::Group:
		case NodeKind::VStack:
		case NodeKind::HStack:
		case NodeKind::Center:
		case NodeKind::Padding:
		case NodeKind::Spacer:
		case NodeKind::Scroll:
			// Containers and Group draw nothing of their own. Their
			// computed rect is consumed by descendant nodes. Scroll
			// pushes a scissor below before recursing into children.
			break;

		case NodeKind::Background:
		case NodeKind::Sprite:
			// effect_color / effect_brightness apply per the legacy Object
			// effect path (e.g. CharacterPanel toggles: effect_color=112,
			// effect_brightness=32 when unselected). Defaults are no-ops.
			renderer.DrawSpriteAt(&target, n.sprite_bank, n.sprite_index, n.x, n.y,
			                     n.effect_color, n.effect_brightness);
			break;

		case NodeKind::FilledRect:
			renderer.DrawFilledRectangle(&target, n.x, n.y,
			                             n.x + (int)n.fill_w,
			                             n.y + (int)n.fill_h,
			                             n.fill_color);
			break;

		case NodeKind::NineSliceFrame: {
			int x, y, w, h;
			if(NodeRect(n, x, y, w, h)){
				DrawNineSlice(ctx.resources, renderer, target,
				              n.nine_bank, n.nine_index, x, y, w, h);
			}
			break;
		}

		case NodeKind::Label:
			// alpha=false matches the legacy Overlay-text path; color /
			// brightness / ramp come from the Node fields (defaults
			// effect_color=0, effect_brightness=128, text_color_ramp=false
			// reproduce the previous behavior for unstyled Labels).
			renderer.DrawText(&target, (Uint16)n.x, (Uint16)n.y,
			                  n.text.c_str(), n.text_bank, n.text_width,
			                  n.text_alpha, n.effect_color, n.effect_brightness,
			                  n.text_color_ramp);
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

	// Scroll containers push a scissor for the duration of their subtree.
	// Empty scissor stack ⇒ no-op so byte-identity holds for every other
	// node kind. Clay computes the inner rect via rect_*; .at() + fill_w/h
	// is the absolute fallback.
	bool pushed_scissor = false;
	if(n.kind == NodeKind::Scroll){
		int x, y, w, h;
		if(NodeRect(n, x, y, w, h)){
			target.PushScissor(x, y, w, h);
			pushed_scissor = true;
		}
	}

	for(const Node & child : n.children){
		RenderNode(child, ctx, target, renderer);
	}

	if(pushed_scissor) target.PopScissor();

	// Stroke modifier — drawn AFTER children so it sits on top of any chrome
	// the children rendered (matches CSS semantics: borders cover content).
	// Default stroke_width=0 ⇒ no-op so byte-identity holds for every node
	// that doesn't opt in.
	if(n.stroke_width > 0){
		int x, y, w, h;
		if(NodeRect(n, x, y, w, h)){
			DrawStrokeRects(renderer, target, x, y, w, h, n.stroke_color, n.stroke_width);
		}
	}
}

}  // namespace

void Render(const Node & root, const Context & ctx, Surface & target, Renderer & renderer)
{
	RenderNode(root, ctx, target, renderer);
}

}  // namespace v2
}  // namespace ui
