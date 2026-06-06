#include "retained_ui_compositor.h"

#include "renderer.h"
#include "resources.h"
#include "surface.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace silencer {
namespace retained_bridge {

namespace {

struct ClipRect {
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

std::vector<ClipRect> g_clipStack;

Uint8 AlphaSrcIndex(Uint8 color, Uint8 /*opacity255*/) {
	if(color < 2) return color;
	if(color >= 256 - 30) return color;
	int rampBase = ((color - 2) / 16) * 16 + 2;
	return static_cast<Uint8>(rampBase + 8);
}

bool CurrentClip(int dstW, int dstH, ClipRect& out) {
	if(g_clipStack.empty()){
		out = ClipRect{0, 0, dstW, dstH};
		return true;
	}
	out = g_clipStack.back();
	return out.w > 0 && out.h > 0;
}

bool ClipDrawRect(int dstW, int dstH, int& x, int& y, int& w, int& h) {
	ClipRect c;
	if(!CurrentClip(dstW, dstH, c)) return false;
	int x1 = std::max(x, c.x);
	int y1 = std::max(y, c.y);
	int x2 = std::min(x + w, c.x + c.w);
	int y2 = std::min(y + h, c.y + c.h);
	if(x2 <= x1 || y2 <= y1) return false;
	x = x1;
	y = y1;
	w = x2 - x1;
	h = y2 - y1;
	return true;
}

bool ClipDrawRectWithin(int dstW,
                        int dstH,
                        const ClipRect& clip,
                        int& x,
                        int& y,
                        int& w,
                        int& h) {
	ClipRect c;
	if(!CurrentClip(dstW, dstH, c)) return false;
	int clipX1 = std::max(c.x, clip.x);
	int clipY1 = std::max(c.y, clip.y);
	int clipX2 = std::min(c.x + c.w, clip.x + clip.w);
	int clipY2 = std::min(c.y + c.h, clip.y + clip.h);
	int x1 = std::max(x, clipX1);
	int y1 = std::max(y, clipY1);
	int x2 = std::min(x + w, clipX2);
	int y2 = std::min(y + h, clipY2);
	if(x2 <= x1 || y2 <= y1) return false;
	x = x1;
	y = y1;
	w = x2 - x1;
	h = y2 - y1;
	return true;
}

Surface * ResolveSprite(Resources& resources, uint32_t textureId) {
	const uint8_t bank = static_cast<uint8_t>((textureId >> 16) & 0xFFu);
	const uint16_t index = static_cast<uint16_t>(textureId & 0xFFFFu);
	const auto& banks = resources.spritebank;
	if(bank >= banks.size()) return nullptr;
	if(index >= banks[bank].size()) return nullptr;
	Surface * src = banks[bank][index].get();
	if(!src || src->w <= 0 || src->h <= 0) return nullptr;
	return src;
}

bool BlitClipped(Surface * src,
                 Renderer::Rect srcRect,
                 Surface * dst,
                 int x,
                 int y) {
	if(!src || !dst || srcRect.w <= 0 || srcRect.h <= 0) return false;
	ClipRect clip;
	if(!CurrentClip(dst->w, dst->h, clip)) return false;
	int x1 = std::max(x, clip.x);
	int y1 = std::max(y, clip.y);
	int x2 = std::min(x + srcRect.w, clip.x + clip.w);
	int y2 = std::min(y + srcRect.h, clip.y + clip.h);
	if(x2 <= x1 || y2 <= y1) return false;

	srcRect.x += x1 - x;
	srcRect.y += y1 - y;
	srcRect.w = x2 - x1;
	srcRect.h = y2 - y1;
	Renderer::Rect dstRect{srcRect.w, srcRect.h, x1, y1};
	Renderer::BlitSurface(src, &srcRect, dst, &dstRect);
	return true;
}

void TileClipped(Surface * src,
                 Renderer::Rect srcRect,
                 Surface * dst,
                 int x,
                 int y,
                 int w,
                 int h) {
	if(!src || !dst || srcRect.w <= 0 || srcRect.h <= 0 || w <= 0 || h <= 0){
		return;
	}
	for(int ty = 0; ty < h; ty += srcRect.h){
		int tileH = std::min(srcRect.h, h - ty);
		for(int tx = 0; tx < w; tx += srcRect.w){
			int tileW = std::min(srcRect.w, w - tx);
			Renderer::Rect tileSrc{tileW, tileH, srcRect.x, srcRect.y};
			BlitClipped(src, tileSrc, dst, x + tx, y + ty);
		}
	}
}

void StretchClipped(Surface * src,
                    Renderer::Rect srcRect,
                    Surface * dst,
                    int x,
                    int y,
                    int w,
                    int h) {
	if(!src || !dst || srcRect.w <= 0 || srcRect.h <= 0 || w <= 0 || h <= 0) return;
	int cx = x;
	int cy = y;
	int cw = w;
	int ch = h;
	if(!ClipDrawRect(dst->w, dst->h, cx, cy, cw, ch)) return;
	for(int py = cy; py < cy + ch; ++py){
		int sy = srcRect.y + ((py - y) * srcRect.h) / h;
		if(sy < srcRect.y) sy = srcRect.y;
		if(sy >= srcRect.y + srcRect.h) sy = srcRect.y + srcRect.h - 1;
		for(int px = cx; px < cx + cw; ++px){
			int sx = srcRect.x + ((px - x) * srcRect.w) / w;
			if(sx < srcRect.x) sx = srcRect.x;
			if(sx >= srcRect.x + srcRect.w) sx = srcRect.x + srcRect.w - 1;
			Uint8 col = Renderer::GetPixel(src, sx, sy);
			if(col) Renderer::SetPixel(dst, px, py, col);
		}
	}
}

bool HasNineSlice(const ::ui::SideWidths& widths) {
	return widths.left > 0.0f || widths.right > 0.0f ||
	       widths.top > 0.0f || widths.bottom > 0.0f;
}

void DrawNineSlice(Surface * src,
                   Surface * dst,
                   int x,
                   int y,
                   int w,
                   int h,
                   const ::ui::SideWidths& widths) {
	if(!src || !dst || w <= 0 || h <= 0) return;
	const int srcLeft = std::min<int>(static_cast<int>(widths.left), src->w / 2);
	const int srcRight = std::min<int>(static_cast<int>(widths.right), src->w - srcLeft);
	const int srcTop = std::min<int>(static_cast<int>(widths.top), src->h / 2);
	const int srcBottom = std::min<int>(static_cast<int>(widths.bottom), src->h - srcTop);
	const int dstLeft = std::min(srcLeft, w / 2);
	const int dstRight = std::min(srcRight, w - dstLeft);
	const int dstTop = std::min(srcTop, h / 2);
	const int dstBottom = std::min(srcBottom, h - dstTop);
	const int dstMidW = w - dstLeft - dstRight;
	const int dstMidH = h - dstTop - dstBottom;
	const int srcMidW = std::max(1, src->w - srcLeft - srcRight);
	const int srcMidH = std::max(1, src->h - srcTop - srcBottom);

	TileClipped(src, Renderer::Rect{srcMidW, srcMidH, srcLeft, srcTop},
	            dst, x + dstLeft, y + dstTop, dstMidW, dstMidH);
	TileClipped(src, Renderer::Rect{srcMidW, dstTop, srcLeft, 0},
	            dst, x + dstLeft, y, dstMidW, dstTop);
	TileClipped(src, Renderer::Rect{srcMidW, dstBottom, srcLeft, src->h - dstBottom},
	            dst, x + dstLeft, y + h - dstBottom, dstMidW, dstBottom);
	TileClipped(src, Renderer::Rect{dstLeft, srcMidH, 0, srcTop},
	            dst, x, y + dstTop, dstLeft, dstMidH);
	TileClipped(src, Renderer::Rect{dstRight, srcMidH, src->w - dstRight, srcTop},
	            dst, x + w - dstRight, y + dstTop, dstRight, dstMidH);

	BlitClipped(src, Renderer::Rect{dstLeft, dstTop, 0, 0}, dst, x, y);
	BlitClipped(src, Renderer::Rect{dstRight, dstTop, src->w - dstRight, 0},
	            dst, x + w - dstRight, y);
	BlitClipped(src, Renderer::Rect{dstLeft, dstBottom, 0, src->h - dstBottom},
	            dst, x, y + h - dstBottom);
	BlitClipped(src, Renderer::Rect{dstRight, dstBottom,
	                                src->w - dstRight, src->h - dstBottom},
	            dst, x + w - dstRight, y + h - dstBottom);
}

void OutlineVisiblePixels(::Renderer & renderer,
                          Surface * source,
                          Surface * target,
                          Uint8 color) {
	if(!source || !target) return;
	int sw = std::min(source->w, target->w);
	int sh = std::min(source->h, target->h);
	for(int py = 0; py < sh; py++){
		for(int px = 0; px < sw; px++){
			if(renderer.GetPixel(source, px, py)) continue;
			if((px > 0 && renderer.GetPixel(source, px - 1, py)) ||
			   (px < sw - 1 && renderer.GetPixel(source, px + 1, py)) ||
			   (py > 0 && renderer.GetPixel(source, px, py - 1)) ||
			   (py < sh - 1 && renderer.GetPixel(source, px, py + 1))){
				renderer.SetPixel(target, px, py, color);
			}
		}
	}
}

void StretchClippedWithin(Surface * src,
                          Renderer::Rect srcRect,
                          Surface * dst,
                          int x,
                          int y,
                          int w,
                          int h,
                          const ClipRect& clip) {
	if(!src || !dst || srcRect.w <= 0 || srcRect.h <= 0 || w <= 0 || h <= 0) return;
	int cx = x;
	int cy = y;
	int cw = w;
	int ch = h;
	if(!ClipDrawRectWithin(dst->w, dst->h, clip, cx, cy, cw, ch)) return;
	for(int py = cy; py < cy + ch; ++py){
		int sy = srcRect.y + ((py - y) * srcRect.h) / h;
		if(sy < srcRect.y) sy = srcRect.y;
		if(sy >= srcRect.y + srcRect.h) sy = srcRect.y + srcRect.h - 1;
		for(int px = cx; px < cx + cw; ++px){
			int sx = srcRect.x + ((px - x) * srcRect.w) / w;
			if(sx < srcRect.x) sx = srcRect.x;
			if(sx >= srcRect.x + srcRect.w) sx = srcRect.x + srcRect.w - 1;
			Uint8 col = Renderer::GetPixel(src, sx, sy);
			if(col) Renderer::SetPixel(dst, px, py, col);
		}
	}
}

void DrawTeamEmblem(Renderer& renderer,
                    Surface * src,
                    Surface * dst,
                    int x,
                    int y,
                    const ::ui::ImageData& image) {
	if(!src || !dst) return;
	int scale = image.scaled ? 2 : 1;
	int w = src->w * scale;
	int h = src->h * scale;
	int cx = x, cy = y, cw = w, ch = h;
	if(!ClipDrawRect(dst->w, dst->h, cx, cy, cw, ch)) return;

	Surface * copy = renderer.CreateSurfaceCopy(src);
	if(!copy) return;
	renderer.EffectTeamColor(copy, nullptr, image.team_color, false, true);
	OutlineVisiblePixels(renderer, src, copy, image.outline_color);
	Renderer::Rect dstrect{w, h, x, y};
	if(image.scaled){
		Renderer::DrawScaled(copy, nullptr, dst, &dstrect);
	}else{
		Renderer::BlitSurface(copy, nullptr, dst, &dstrect);
	}
	delete copy;
}

void DrawImage(Resources& resources,
               Renderer& renderer,
               Surface * dst,
               const ::ui::DrawCommand& command) {
	const ::ui::ImageData& image = command.payload.image;
	if(!dst || image.texture_id == 0 || image.tint.a == 0) return;
	Surface * src = ResolveSprite(resources, image.texture_id);
	if(!src) return;
	int x = static_cast<int>(std::floor(command.rect.x));
	int y = static_cast<int>(std::floor(command.rect.y));
	int w = static_cast<int>(std::ceil(command.rect.x + command.rect.w)) - x;
	int h = static_cast<int>(std::ceil(command.rect.y + command.rect.h)) - y;
	if(w <= 0 || h <= 0) return;
	if(image.team_emblem){
		DrawTeamEmblem(renderer, src, dst, x, y, image);
		return;
	}
	Surface * work = src;
	if(image.effect_color != 0 || image.ramp_color != 0 ||
	   image.brightness != 128){
		work = renderer.CreateSurfaceCopy(src);
		if(!work) return;
		if(image.effect_color != 0){
			renderer.EffectColor(work, nullptr, image.effect_color);
		}
		if(image.ramp_color != 0){
			if(image.ramp_plus != 0){
				renderer.EffectRampColorPlus(work, nullptr, image.ramp_color,
				                             image.ramp_plus);
			}else{
				renderer.EffectRampColor(work, nullptr, image.ramp_color);
			}
		}
		if(image.brightness != 128){
			renderer.EffectBrightness(work, nullptr, image.brightness);
		}
	}
	if(HasNineSlice(image.nine_slice)){
		DrawNineSlice(work, dst, x, y, w, h, image.nine_slice);
		if(work != src) delete work;
		return;
	}
	const int srcX = std::min<int>(image.source_x, std::max(0, work->w - 1));
	const int srcY = std::min<int>(image.source_y, std::max(0, work->h - 1));
	const int maxSourceW = std::max(0, work->w - srcX);
	const int maxSourceH = std::max(0, work->h - srcY);
	const int sourceW = image.source_w > 0
		? std::min<int>(image.source_w, maxSourceW)
		: maxSourceW;
	const int sourceH = image.source_h > 0
		? std::min<int>(image.source_h, maxSourceH)
		: maxSourceH;
	if(sourceW <= 0 || sourceH <= 0){
		if(work != src) delete work;
		return;
	}
	Renderer::Rect srcRect{sourceW, sourceH, srcX, srcY};
	if(image.tile){
		TileClipped(work, srcRect, dst, x, y, w, h);
	}else if(w == sourceW && h == sourceH){
		BlitClipped(work, srcRect, dst, x, y);
	}else if(image.fit == ::ui::ImageFit::Cover ||
	         image.fit == ::ui::ImageFit::Contain){
		const float sx = static_cast<float>(w) / static_cast<float>(sourceW);
		const float sy = static_cast<float>(h) / static_cast<float>(sourceH);
		const float scale = image.fit == ::ui::ImageFit::Cover
			? std::max(sx, sy)
			: std::min(sx, sy);
		const int drawW = std::max(1, static_cast<int>(sourceW * scale + 0.5f));
		const int drawH = std::max(1, static_cast<int>(sourceH * scale + 0.5f));
		const int drawX = x + (w - drawW) / 2;
		const int drawY = y + (h - drawH) / 2;
		StretchClippedWithin(work,
		                     srcRect,
		                     dst,
		                     drawX,
		                     drawY,
		                     drawW,
		                     drawH,
		                     ClipRect{x, y, w, h});
	}else{
		StretchClipped(work, srcRect, dst, x, y, w, h);
	}
	if(work != src) delete work;
}

void DrawBitmap(Surface * dst,
                const ::ui::DrawCommand& command) {
	const ::ui::BitmapData& bitmap = command.payload.bitmap;
	if(!dst || !bitmap.pixels || bitmap.width == 0 || bitmap.height == 0) return;
	int x = static_cast<int>(std::floor(command.rect.x));
	int y = static_cast<int>(std::floor(command.rect.y));
	int w = static_cast<int>(std::ceil(command.rect.x + command.rect.w)) - x;
	int h = static_cast<int>(std::ceil(command.rect.y + command.rect.h)) - y;
	if(w <= 0) w = bitmap.width;
	if(h <= 0) h = bitmap.height;
	if(w <= 0 || h <= 0) return;

	int cx = x;
	int cy = y;
	int cw = w;
	int ch = h;
	if(!ClipDrawRect(dst->w, dst->h, cx, cy, cw, ch)) return;

	for(int py = cy; py < cy + ch; ++py){
		int sy = ((py - y) * static_cast<int>(bitmap.height)) / h;
		if(sy < 0) sy = 0;
		if(sy >= static_cast<int>(bitmap.height)) sy = bitmap.height - 1;
		const Uint8 * srcRow = bitmap.pixels
			+ static_cast<size_t>(sy) * static_cast<size_t>(bitmap.width);
		Uint8 * dstRow = dst->pixels.data() + static_cast<size_t>(py) * dst->w;
		for(int px = cx; px < cx + cw; ++px){
			int sx = ((px - x) * static_cast<int>(bitmap.width)) / w;
			if(sx < 0) sx = 0;
			if(sx >= static_cast<int>(bitmap.width)) sx = bitmap.width - 1;
			Uint8 col = srcRow[sx];
			if(col) dstRow[px] = col;
		}
	}
}

void FillRect(Renderer& renderer,
              Surface * dst,
              const ::ui::DrawRect& rect,
              ::ui::Color color) {
	if(!dst || color.a == 0) return;
	int x = static_cast<int>(std::floor(rect.x));
	int y = static_cast<int>(std::floor(rect.y));
	int w = static_cast<int>(std::ceil(rect.x + rect.w)) - x;
	int h = static_cast<int>(std::ceil(rect.y + rect.h)) - y;
	if(w <= 0 || h <= 0) return;
	if(!ClipDrawRect(dst->w, dst->h, x, y, w, h)) return;

	Uint8 paletteIndex = (color.g != 0 || color.b != 0)
		? renderer.palette.ClosestMatch(SDL_Color{color.r, color.g, color.b, 255})
		: color.r;
	if(color.a == 255){
		Renderer::DrawFilledRectangle(dst, x, y, x + w, y + h, paletteIndex);
		return;
	}
	Uint8 src = AlphaSrcIndex(paletteIndex, color.a);
	for(int py = y; py < y + h; ++py){
		for(int px = x; px < x + w; ++px){
			Uint8 d = Renderer::GetPixel(dst, px, py);
			Renderer::SetPixel(dst, px, py, renderer.palette.Alpha(src, d));
		}
	}
}

void DrawBorder(Renderer& renderer,
                Surface * dst,
                const ::ui::DrawCommand& command) {
	const ::ui::BorderData& data = command.payload.border;
	auto width_px = [](float width) {
		if(width <= 0.0f) return 0;
		return std::max(1, static_cast<int>(std::round(width)));
	};
	auto edge = [&](int x, int y, int w, int h, ::ui::Color color) {
		if(w <= 0 || h <= 0) return;
		FillRect(renderer,
		         dst,
		         ::ui::DrawRect{static_cast<float>(x),
		                        static_cast<float>(y),
		                        static_cast<float>(w),
		                        static_cast<float>(h)},
		         color);
	};
	auto border = [&](const ::ui::DrawRect& r,
	                  const ::ui::SideWidths& widths,
	                  const ::ui::SideColors& colors) {
		const int x0 = static_cast<int>(std::floor(r.x));
		const int y0 = static_cast<int>(std::floor(r.y));
		const int x1 = static_cast<int>(std::ceil(r.x + r.w));
		const int y1 = static_cast<int>(std::ceil(r.y + r.h));
		const int w = std::max(0, x1 - x0);
		const int h = std::max(0, y1 - y0);
		const int top = width_px(widths.top);
		const int right = width_px(widths.right);
		const int bottom = width_px(widths.bottom);
		const int left = width_px(widths.left);
		if(top > 0) edge(x0, y0, w, top, colors.top);
		if(bottom > 0) edge(x0, y1 - bottom, w, bottom, colors.bottom);
		if(left > 0) edge(x0, y0, left, h, colors.left);
		if(right > 0) edge(x1 - right, y0, right, h, colors.right);
	};
	const ::ui::DrawRect& r = command.rect;
	border(r, data.border.width, data.border.color);
	if(data.has_outline && data.outline.width > 0.0f && data.outline.color.a > 0){
		const ::ui::DrawRect outline_rect{
			r.x - data.outline.offset,
			r.y - data.outline.offset,
			r.w + data.outline.offset * 2.0f,
			r.h + data.outline.offset * 2.0f,
		};
		const ::ui::SideWidths outline_width{
			data.outline.width,
			data.outline.width,
			data.outline.width,
			data.outline.width,
		};
		const ::ui::SideColors outline_color{
			data.outline.color,
			data.outline.color,
			data.outline.color,
			data.outline.color,
		};
		border(outline_rect, outline_width, outline_color);
	}
}

struct FontSpec {
	Uint8 bank = 133;
	Uint8 advance = 7;
};

FontSpec FontFor(const ::ui::TextData& text) {
	if(text.font_id != 0 && text.font_size != 0){
		return FontSpec{
			static_cast<Uint8>(text.font_id),
			static_cast<Uint8>(text.font_size),
		};
	}
	if(text.font_size >= 18){
		return FontSpec{135, 11};
	}
	if(text.font_size >= 12){
		return FontSpec{133, 7};
	}
	return FontSpec{133, 6};
}

Uint8 StraightChannel(Uint8 channel, Uint8 alpha) {
	if(alpha == 0 || alpha == 255) return channel;
	return static_cast<Uint8>(
		std::min(255, (static_cast<int>(channel) * 255 + alpha / 2) / alpha));
}

::ui::Color StraightTextColor(::ui::Color color) {
	color.r = StraightChannel(color.r, color.a);
	color.g = StraightChannel(color.g, color.a);
	color.b = StraightChannel(color.b, color.a);
	return color;
}

Uint8 TextEffectColor(Renderer& renderer, ::ui::Color color) {
	const ::ui::Color straight = StraightTextColor(color);
	if(straight.b != 0){
		return renderer.palette.ClosestMatch(
			SDL_Color{straight.r, straight.g, straight.b, 255});
	}
	return straight.r;
}

Uint8 TextBrightness(::ui::Color color) {
	const ::ui::Color straight = StraightTextColor(color);
	if(straight.b != 0) return 128;
	return straight.g == 0 ? 128 : straight.g;
}

void DrawText(Renderer& renderer,
              Surface * dst,
              const ::ui::DrawCommandList& commands,
              const ::ui::DrawCommand& command) {
	const ::ui::TextData& text = command.payload.text;
	if(!dst || text.text_len == 0 || text.color.a == 0) return;
	char buf[512];
	uint16_t n = text.text_len < sizeof(buf) - 1
		? text.text_len
		: static_cast<uint16_t>(sizeof(buf) - 1);
	std::memcpy(buf, &commands.text_arena[text.text_off], n);
	buf[n] = '\0';
	int x = static_cast<int>(command.rect.x);
	int y = static_cast<int>(command.rect.y);
	int w = static_cast<int>(command.rect.w);
	int h = static_cast<int>(command.rect.h);
	if(w > 0 && h > 0 && !ClipDrawRect(dst->w, dst->h, x, y, w, h)) return;
	FontSpec font = FontFor(text);
	Uint8 color = TextEffectColor(renderer, text.color);
	Uint8 brightness = TextBrightness(text.color);
	renderer.DrawText(dst,
	                  static_cast<Uint16>(std::max(0, x)),
	                  static_cast<Uint16>(std::max(0, y)),
	                  buf,
	                  font.bank,
	                  font.advance,
	                  text.color.a < 255,
	                  color,
	                  brightness,
	                  false);
}

void PushClip(Surface * dst, const ::ui::DrawRect& rect) {
	if(!dst) return;
	ClipRect cur;
	CurrentClip(dst->w, dst->h, cur);
	int x1 = std::max(static_cast<int>(rect.x), cur.x);
	int y1 = std::max(static_cast<int>(rect.y), cur.y);
	int x2 = std::min(static_cast<int>(rect.x + rect.w), cur.x + cur.w);
	int y2 = std::min(static_cast<int>(rect.y + rect.h), cur.y + cur.h);
	g_clipStack.push_back(ClipRect{x1, y1, std::max(0, x2 - x1),
	                               std::max(0, y2 - y1)});
}

void RenderInto(Resources& resources,
                Renderer& renderer,
                Surface * dst,
                const ::ui::DrawCommandList& commands) {
	g_clipStack.clear();
	if(!dst) return;
	for(int i = 0; i < commands.count; ++i){
		const ::ui::DrawCommand& command = commands.commands[i];
		switch(command.kind){
		case ::ui::DrawCommandKind::Rect:
			FillRect(renderer, dst, command.rect, command.payload.rect.fill);
			break;
		case ::ui::DrawCommandKind::Gradient:
			if(command.payload.gradient.stop_count > 0){
				FillRect(renderer, dst, command.rect,
				         command.payload.gradient.stop_count > 0
				             ? commands.grad_arena[command.payload.gradient.stop_off].color
				             : ::ui::Color{});
			}
			break;
		case ::ui::DrawCommandKind::Border:
			DrawBorder(renderer, dst, command);
			break;
		case ::ui::DrawCommandKind::Text:
			DrawText(renderer, dst, commands, command);
			break;
		case ::ui::DrawCommandKind::Image:
			DrawImage(resources, renderer, dst, command);
			break;
		case ::ui::DrawCommandKind::Bitmap:
			DrawBitmap(dst, command);
			break;
		case ::ui::DrawCommandKind::ClipPush:
			PushClip(dst, command.rect);
			break;
		case ::ui::DrawCommandKind::ClipPop:
			if(!g_clipStack.empty()) g_clipStack.pop_back();
			break;
		case ::ui::DrawCommandKind::None:
		case ::ui::DrawCommandKind::Shadow:
		case ::ui::DrawCommandKind::LayerPush:
		case ::ui::DrawCommandKind::LayerPop:
		case ::ui::DrawCommandKind::Custom:
			break;
		}
	}
}

}  // namespace

void Render(Resources& resources,
            Renderer& renderer,
            Surface * dst,
            const ::ui::DrawCommandList& commands,
            int virtualWidth,
            int virtualHeight,
            float scale) {
	if(!dst) return;
	if(scale <= 1.0f){
		RenderInto(resources, renderer, dst, commands);
		return;
	}
	int vw = std::max(1, virtualWidth);
	int vh = std::max(1, virtualHeight);
	static Surface scratch;
	if(scratch.w != vw || scratch.h != vh){
		scratch.Resize(vw, vh, 0);
	}else{
		scratch.Clear(0);
	}
	RenderInto(resources, renderer, &scratch, commands);

	const Uint8 * sp = scratch.pixels.data();
	Uint8 * dp = dst->pixels.data();
	int scaledW = static_cast<int>(vw * scale + 0.5f);
	int scaledH = static_cast<int>(vh * scale + 0.5f);
	int offsetX = scaledW < dst->w ? (dst->w - scaledW) / 2 : 0;
	int offsetY = scaledH < dst->h ? (dst->h - scaledH) / 2 : 0;
	int drawW = std::min(scaledW, dst->w - offsetX);
	int drawH = std::min(scaledH, dst->h - offsetY);
	for(int dy = 0; dy < drawH; ++dy){
		int sy = static_cast<int>(dy / scale);
		if(sy >= vh) sy = vh - 1;
		const Uint8 * srow = sp + sy * vw;
		Uint8 * drow = dp + (offsetY + dy) * dst->w + offsetX;
		for(int dx = 0; dx < drawW; ++dx){
			int sx = static_cast<int>(dx / scale);
			if(sx >= vw) sx = vw - 1;
			Uint8 c = srow[sx];
			if(c) drow[dx] = c;
		}
	}
}

}  // namespace retained_bridge
}  // namespace silencer
