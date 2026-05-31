#include "retained_surface_renderer.h"

#include "resources.h"
#include "renderer.h"
#include "render/clay_ui_payloads.h"
#include "surface.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace silencer {
namespace client_ui {
namespace {

SDL_Color Unpremultiply(::ui::Color color) {
	if(color.a == 0) return SDL_Color{0, 0, 0, 0};
	if(color.a == 255) return SDL_Color{color.r, color.g, color.b, 255};
	auto un = [&](Uint8 value) -> Uint8 {
		int scaled = (static_cast<int>(value) * 255 + color.a / 2) / color.a;
		return static_cast<Uint8>(scaled > 255 ? 255 : scaled);
	};
	return SDL_Color{un(color.r), un(color.g), un(color.b), color.a};
}

Uint8 PaletteIndex(Renderer& renderer, ::ui::Color color) {
	SDL_Color straight = Unpremultiply(color);
	straight.a = 255;
	return renderer.palette.ClosestMatch(straight);
}

bool ClipRect(const Surface& dst, int& x, int& y, int& w, int& h) {
	if(w <= 0 || h <= 0 || dst.w <= 0 || dst.h <= 0) return false;
	int x1 = std::max(0, x);
	int y1 = std::max(0, y);
	int x2 = std::min(dst.w, x + w);
	int y2 = std::min(dst.h, y + h);
	if(x2 <= x1 || y2 <= y1) return false;
	x = x1;
	y = y1;
	w = x2 - x1;
	h = y2 - y1;
	return true;
}

void FillRect(Renderer& renderer, Surface& dst, const ::ui::DrawRect& rect, ::ui::Color color) {
	if(color.a == 0) return;
	int x = static_cast<int>(std::floor(rect.x));
	int y = static_cast<int>(std::floor(rect.y));
	int w = static_cast<int>(std::ceil(rect.x + rect.w)) - x;
	int h = static_cast<int>(std::ceil(rect.y + rect.h)) - y;
	if(!ClipRect(dst, x, y, w, h)) return;
	Renderer::DrawFilledRectangle(&dst, x, y, x + w, y + h, PaletteIndex(renderer, color));
}

void DrawBorderSide(Renderer& renderer,
                    Surface& dst,
                    int x,
                    int y,
                    int w,
                    int h,
                    float thickness,
                    ::ui::Color color) {
	if(thickness <= 0.0f || color.a == 0 || w <= 0 || h <= 0) return;
	FillRect(renderer, dst, ::ui::DrawRect{
		static_cast<float>(x),
		static_cast<float>(y),
		static_cast<float>(w),
		static_cast<float>(h),
	}, color);
}

void DrawBorder(Renderer& renderer, Surface& dst, const ::ui::DrawCommand& command) {
	const ::ui::Border& border = command.payload.border.border;
	int x = static_cast<int>(std::floor(command.rect.x));
	int y = static_cast<int>(std::floor(command.rect.y));
	int w = static_cast<int>(std::ceil(command.rect.w));
	int h = static_cast<int>(std::ceil(command.rect.h));
	if(w <= 0 || h <= 0) return;

	int top = std::max(0, static_cast<int>(std::round(border.width.top)));
	int right = std::max(0, static_cast<int>(std::round(border.width.right)));
	int bottom = std::max(0, static_cast<int>(std::round(border.width.bottom)));
	int left = std::max(0, static_cast<int>(std::round(border.width.left)));
	DrawBorderSide(renderer, dst, x, y, w, top, border.width.top, border.color.top);
	DrawBorderSide(renderer, dst, x, y + h - bottom, w, bottom, border.width.bottom, border.color.bottom);
	DrawBorderSide(renderer, dst, x, y + top, left, h - top - bottom, border.width.left, border.color.left);
	DrawBorderSide(renderer, dst, x + w - right, y + top, right, h - top - bottom, border.width.right, border.color.right);

	if(command.payload.border.has_outline && command.payload.border.outline.width > 0.0f){
		const ::ui::Outline& outline = command.payload.border.outline;
		int o = static_cast<int>(std::round(outline.offset));
		int t = std::max(1, static_cast<int>(std::round(outline.width)));
		::ui::Color color = outline.color;
		FillRect(renderer, dst, ::ui::DrawRect{static_cast<float>(x - o - t), static_cast<float>(y - o - t), static_cast<float>(w + (o + t) * 2), static_cast<float>(t)}, color);
		FillRect(renderer, dst, ::ui::DrawRect{static_cast<float>(x - o - t), static_cast<float>(y + h + o), static_cast<float>(w + (o + t) * 2), static_cast<float>(t)}, color);
		FillRect(renderer, dst, ::ui::DrawRect{static_cast<float>(x - o - t), static_cast<float>(y - o), static_cast<float>(t), static_cast<float>(h + o * 2)}, color);
		FillRect(renderer, dst, ::ui::DrawRect{static_cast<float>(x + w + o), static_cast<float>(y - o), static_cast<float>(t), static_cast<float>(h + o * 2)}, color);
	}
}

void DrawText(Renderer& renderer, Surface& dst, const ::ui::DrawCommandList& list, const ::ui::DrawCommand& command) {
	const ::ui::TextData& text = command.payload.text;
	if(text.text_len == 0 || text.color.a == 0) return;
	if(text.text_off + text.text_len > static_cast<uint32_t>(::ui::UI_DRAW_TEXT_ARENA_BYTES)) return;
	char buffer[512];
	int len = std::min<int>(text.text_len, static_cast<int>(sizeof(buffer)) - 1);
	std::memcpy(buffer, &list.text_arena[text.text_off], static_cast<size_t>(len));
	buffer[len] = '\0';
	int x = std::max(0, static_cast<int>(std::floor(command.rect.x)));
	int y = std::max(0, static_cast<int>(std::floor(command.rect.y)));
	renderer.DrawText(&dst,
	                  static_cast<Uint16>(x),
	                  static_cast<Uint16>(y),
	                  buffer,
	                  text.font_id > 0 ? static_cast<Uint8>(text.font_id) : 133,
	                  text.font_size > 0 ? static_cast<Uint8>(text.font_size) : 7,
	                  false,
	                  PaletteIndex(renderer, text.color),
	                  128,
	                  false);
}

Surface * ResolveSprite(const Resources& resources, uint32_t textureId) {
	const uint32_t flags = static_cast<uint32_t>(
		silencer::clay_bridge::kImageContainBit |
		silencer::clay_bridge::kImageStretchBit);
	const uint32_t raw = textureId & ~flags;
	const Uint8 bank = static_cast<Uint8>((raw >> 16) & 0xFFu);
	const Uint16 index = static_cast<Uint16>(raw & 0xFFFFu);
	if(bank >= resources.spritebank.size()) return nullptr;
	if(index >= resources.spritebank[bank].size()) return nullptr;
	return resources.spritebank[bank][index].get();
}

void DrawImage(const Resources& resources, Surface& dst, const ::ui::DrawCommand& command) {
	if(command.payload.image.texture_id == 0) return;
	Surface * src = ResolveSprite(resources, command.payload.image.texture_id);
	if(!src) return;
	Renderer::Rect dstrect{
		src->w,
		src->h,
		static_cast<int>(std::floor(command.rect.x)),
		static_cast<int>(std::floor(command.rect.y)),
	};
	Renderer::BlitSurface(src, nullptr, &dst, &dstrect);
}

}  // namespace

void RenderRetainedDrawCommands(Renderer& renderer,
                                const Resources& resources,
                                Surface& dst,
                                const ::ui::DrawCommandList& commands) {
	for(int i = 0; i < commands.count; ++i){
		const ::ui::DrawCommand& command = commands.commands[i];
		switch(command.kind){
			case ::ui::DrawCommandKind::Rect:
				FillRect(renderer, dst, command.rect, command.payload.rect.fill);
				break;
			case ::ui::DrawCommandKind::Gradient:
				if(command.payload.gradient.stop_count > 0){
					FillRect(renderer, dst, command.rect,
					         commands.grad_arena[command.payload.gradient.stop_off].color);
				}
				break;
			case ::ui::DrawCommandKind::Border:
				DrawBorder(renderer, dst, command);
				break;
			case ::ui::DrawCommandKind::Text:
				DrawText(renderer, dst, commands, command);
				break;
			case ::ui::DrawCommandKind::Image:
				DrawImage(resources, dst, command);
				break;
			default:
				break;
		}
	}
}

}  // namespace client_ui
}  // namespace silencer
