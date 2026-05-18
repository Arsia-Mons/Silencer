#include "clay_ui_compositor.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "resources.h"
#include "surface.h"
#include "ui/primitives/text.h"
#include "ui/primitives/text_internal.h"
#include "world.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace silencer::clay_bridge {

namespace {

// One Clay context for the whole process. Lazily initialized on first
// EnsureInitialized() call. Single-threaded — all callers run on the game
// thread (control dispatch and screen Tick).
bool g_initialized = false;
void * g_arenaMemory = nullptr;
::Clay_Context * g_context = nullptr;
int g_lastW = -1;
int g_lastH = -1;

// Magnification for bitmap glyph/sprite/chrome draws. 1 == native
// (no magnification). Updated via SetUiScale() before each Render().
float g_uiScale = 1.0f;

const Resources * g_textMeasureResources = nullptr;

// Fallback for adapter tests or debug text that still arrives as raw Clay
// text config instead of the Text primitive.
Uint16 FallbackLineHeightForClayFont(uint16_t fontId) {
	switch(fontId){
		case 132: return 7;
		case 133: return 11;
		case 134: return 15;
		case 135: return 19;
		case 136: return 23;
		default:  return 11;
	}
}

bool UsesInkMetrics(void * userData) {
	if(!userData) return false;
	const TextDrawData * extra =
		reinterpret_cast<const TextDrawData *>(userData);
	return extra && extra->measureInk;
}

int GlyphOffsetForBank(Uint16 bank) {
	return bank == 132 ? 34 : 33;
}

struct TextInkMetrics {
	bool hasInk = false;
	int minX = 0;
	int maxX = 0;
	int width = 0;
	int minY = 0;
	int maxY = 0;
	int height = 0;
};

TextInkMetrics MeasureInkBounds(const Resources * resources,
                                const char * chars,
                                int32_t length,
                                Uint16 bank,
                                Uint16 advance) {
	TextInkMetrics out;
	if(!resources || !chars || length <= 0 || advance == 0){
		return out;
	}
	if(bank >= resources->spritebank.size()){
		return out;
	}
	const auto& glyphs = resources->spritebank[bank];
	const int ioffset = GlyphOffsetForBank(bank);
	int pen = 0;
	for(int32_t i = 0; i < length; ++i){
		unsigned char ch = static_cast<unsigned char>(chars[i]);
		if(ch == ' ' || ch == 0xA0){
			pen += advance;
			continue;
		}
		int glyphIndex = static_cast<int>(ch) - ioffset;
		if(glyphIndex < 0 || glyphIndex >= static_cast<int>(glyphs.size())){
			pen += advance;
			continue;
		}
		Surface * glyph = glyphs[glyphIndex].get();
		if(!glyph || glyph->w <= 0 || glyph->h <= 0){
			pen += advance;
			continue;
		}
		int glyphMinX = glyph->w;
		int glyphMaxX = -1;
		int glyphMinY = glyph->h;
		int glyphMaxY = -1;
		for(int y = 0; y < glyph->h; ++y){
			for(int x = 0; x < glyph->w; ++x){
				if(Renderer::GetPixel(glyph, x, y) == 0) continue;
				if(x < glyphMinX) glyphMinX = x;
				if(x > glyphMaxX) glyphMaxX = x;
				if(y < glyphMinY) glyphMinY = y;
				if(y > glyphMaxY) glyphMaxY = y;
			}
		}
		if(glyphMaxX >= glyphMinX){
			int inkMin = pen + glyphMinX;
			int inkMax = pen + glyphMaxX + 1;
			if(!out.hasInk || inkMin < out.minX) out.minX = inkMin;
			if(!out.hasInk || inkMax > out.maxX) out.maxX = inkMax;
			if(!out.hasInk || glyphMinY < out.minY) out.minY = glyphMinY;
			if(!out.hasInk || glyphMaxY + 1 > out.maxY) out.maxY = glyphMaxY + 1;
			out.hasInk = true;
		}
		pen += advance;
	}
	if(out.hasInk){
		out.width = out.maxX - out.minX;
		out.height = out.maxY - out.minY;
	}
	return out;
}

::Clay_Dimensions MeasureTextCommand(::Clay_StringSlice text,
                                     ::Clay_TextElementConfig * config,
                                     void * /*userData*/) {
	::Clay_Dimensions out;
	const uint16_t lineHeight = config->lineHeight
		? config->lineHeight
		: FallbackLineHeightForClayFont(config->fontId);
	TextInkMetrics ink = UsesInkMetrics(config->userData)
		? MeasureInkBounds(g_textMeasureResources,
		                    text.chars,
		                    text.length,
		                    config->fontId,
		                    config->fontSize)
		: TextInkMetrics{};
	out.width  = ink.hasInk
		? static_cast<float>(ink.width)
		: static_cast<float>(text.length * config->fontSize);
	out.height = static_cast<float>(lineHeight);
	return out;
}

void HandleClayError(::Clay_ErrorData /*data*/) {
	// Don't crash the binary on Clay layout warnings — log silently.
	// (Clay's own debug view will surface these in interactive use.)
}

// Stack of clip rects established by SCISSOR_START/END. Top of stack is the
// effective clip for any subsequent draw command. Reset at every Render()
// call, so stale state from a previous frame can't leak.
struct ClipRect { int x, y, w, h; };
std::vector<ClipRect> g_clipStack;

bool CurrentClip(int dstW, int dstH, ClipRect & out) {
	if(g_clipStack.empty()){
		out = ClipRect{0, 0, dstW, dstH};
		return true;
	}
	out = g_clipStack.back();
	return out.w > 0 && out.h > 0;
}

// Intersect a draw rect with the active clip. Returns false if fully clipped.
bool ClipDrawRect(int dstW, int dstH,
                  int & x, int & y, int & w, int & h) {
	ClipRect c;
	if(!CurrentClip(dstW, dstH, c)){ return false; }
	int x1 = std::max(x, c.x);
	int y1 = std::max(y, c.y);
	int x2 = std::min(x + w, c.x + c.w);
	int y2 = std::min(y + h, c.y + c.h);
	if(x2 <= x1 || y2 <= y1){ return false; }
	x = x1; y = y1; w = x2 - x1; h = y2 - y1;
	return true;
}

enum class ImageFit : Uint8 {
	Cover,
	Contain,
	Stretch,
};

void UnpackImage(void * p, Uint8 & bank, Uint16 & index, ImageFit & fit) {
	std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
	bank    = static_cast<Uint8>((v >> 16) & 0xFFu);
	index   = static_cast<Uint16>(v & 0xFFFFu);
	fit = (v & kImageStretchBit) != 0 ? ImageFit::Stretch
	    : (v & kImageContainBit) != 0 ? ImageFit::Contain
	                                  : ImageFit::Cover;
}

// Construct a "low-nibble alpha" src palette index for the alphaed LUT.
// The palette's `alphaed[i*256+j]` table is keyed on a src index whose color
// ramp position (low 4 bits of (i-2)) encodes the desired alpha — alpha = 0
// at ramp position 0, alpha ≈ 50% at ramp position 8, fully opaque src at
// ramp positions 9..15 (the LUT collapses these to 1.0). See
// palette.cpp:159-191 (`Palette::Calculate`). For colors outside the
// standard ramp range [2, 226) — i.e., the reserved low entries 0..1 and
// the parallax band [226, 256) — the ramp construction isn't meaningful;
// fall back to the original color (caller gets fully-opaque draw, matching
// the pre-C1 behavior for those edge cases).
//
// First-cut quantization: any non-zero, non-255 opacity routes to ramp
// position 8 (≈50%). Full 0..15-level alpha is left for a future iteration
// if it becomes needed — the LUT supports it without code changes here.
Uint8 AlphaSrcIndex(Uint8 color, Uint8 /*opacity255*/) {
	if(color < 2) return color;
	if(color >= 256 - 30) return color;
	int rampBase = ((color - 2) / 16) * 16 + 2;
	return static_cast<Uint8>(rampBase + 8);
}

// ---------------------------------------------------------------------------
// Stroke corner join model (issue #176).
//
// Composed L / stepped chrome (the lobby right pane) is built from several
// adjacent `BoxStroke` boxes whose shared edges are suppressed. Each box
// insets its stroke bands from its OWN bbox, so where two boxes meet the two
// perpendicular runs of the same band never share a pixel — the elbow notches.
//
// Instead of per-box geometry hacks we render strokes in two passes:
//
//   Pass 1 (during the command walk): every active band edge a box draws
//   registers its two endpoints as corners. A corner is keyed by its snapped
//   pixel position plus the band's visual identity (color/opacity/thickness)
//   and accumulates a 4-bit mask of the cardinal directions in which that band
//   continues — across ALL boxes, so a shared junction sees connectivity from
//   both neighbours.
//
//   Pass 2 (after the walk): every collected corner is filled with one
//   band-thickness square placed by how many directions connect there. Two
//   perpendicular runs that stopped short of each other now both reach the
//   shared square, so the join is contiguous. For a plain closed rectangle the
//   square lands exactly on the existing stepped-ownership overlap, so
//   ordinary chrome is unchanged.
namespace corner_dir {
constexpr Uint8 Up    = 1 << 0;  // a vertical run goes upward from here
constexpr Uint8 Down  = 1 << 1;
constexpr Uint8 Left  = 1 << 2;  // a horizontal run goes leftward from here
constexpr Uint8 Right = 1 << 3;
}  // namespace corner_dir

struct StrokeCorner {
	int   x;          // snapped pixel position of the join centre
	int   y;
	int   thickness;  // band thickness (square side)
	Uint8 color;
	Uint8 opacity;
	Uint8 dirs;       // OR of corner_dir bits, accumulated across boxes
	ClipRect clip;    // active clip when the owning edge was emitted
};

// Per-frame corner set. Cleared at the top of RenderInto and drained by the
// pass-2 corner render before RenderInto returns.
std::vector<StrokeCorner> g_strokeCorners;

// Register one band endpoint as a corner. Corners that match position + band
// identity + clip merge so their direction masks accumulate.
void AddStrokeCorner(int x, int y, int thickness,
                     Uint8 color, Uint8 opacity, Uint8 dir,
                     const ClipRect & clip) {
	for(StrokeCorner & sc : g_strokeCorners){
		if(sc.x == x && sc.y == y && sc.thickness == thickness &&
		   sc.color == color && sc.opacity == opacity &&
		   sc.clip.x == clip.x && sc.clip.y == clip.y &&
		   sc.clip.w == clip.w && sc.clip.h == clip.h){
			sc.dirs |= dir;
			return;
		}
	}
	g_strokeCorners.push_back(
		StrokeCorner{x, y, thickness, color, opacity, dir, clip});
}

// Fill an axis-aligned stripe through `clip`. When `opacity` is 255 it is a
// solid palette fill; otherwise the colour is alpha-blended against each
// underlying pixel through the palette's alphaed LUT, with an RGB
// ClosestMatch fallback for destination indices outside the LUT's [2, 226)
// range (notably the lobby's pure-black backdrop). Shared by the stroke edge
// pass and the corner join pass so both blend identically.
void FillStrokeStripe(::Renderer & renderer, Surface * dst,
                      const ClipRect & clip,
                      int x, int y, int w, int h,
                      Uint8 color, Uint8 opacity) {
	if(w <= 0 || h <= 0 || !dst) return;
	int x1 = std::max(x, clip.x);
	int y1 = std::max(y, clip.y);
	int x2 = std::min(x + w, clip.x + clip.w);
	int y2 = std::min(y + h, clip.y + clip.h);
	if(x2 <= x1 || y2 <= y1) return;
	x = x1; y = y1; w = x2 - x1; h = y2 - y1;
	if(opacity == 255){
		Renderer::DrawFilledRectangle(dst, x, y, x + w, y + h, color);
		return;
	}
	Uint8 srcLUT = AlphaSrcIndex(color, opacity);
	SDL_Color * colors = renderer.palette.GetColors();
	SDL_Color srcRgb = colors[color];
	const float alpha = 0.5f;  // matches LUT quantization
	Uint8 fallbackCache[256];
	bool  fallbackCached[256] = {false};
	for(int py = y; py < y + h; py++){
		for(int px = x; px < x + w; px++){
			Uint8 d = Renderer::GetPixel(dst, px, py);
			Uint8 blended;
			if(d >= 2 && d < 226){
				blended = renderer.palette.Alpha(srcLUT, d);
			}else if(fallbackCached[d]){
				blended = fallbackCache[d];
			}else{
				// Inline RGB lerp — `Palette::Alpha(SDL_Color,...)` is
				// private. Mirrors palette.cpp:442-447.
				SDL_Color dstRgb = colors[d];
				SDL_Color rgb;
				rgb.r = (Uint8)(srcRgb.r * alpha + dstRgb.r * (1.0f - alpha));
				rgb.g = (Uint8)(srcRgb.g * alpha + dstRgb.g * (1.0f - alpha));
				rgb.b = (Uint8)(srcRgb.b * alpha + dstRgb.b * (1.0f - alpha));
				rgb.a = 255;
				blended = renderer.palette.ClosestMatch(rgb);
				fallbackCache[d]  = blended;
				fallbackCached[d] = true;
			}
			Renderer::SetPixel(dst, px, py, blended);
		}
	}
}

// Pass 2: render every collected stroke corner. A corner is one
// band-thickness square; the connected-direction mask only decides whether
// this corner actually needs a join (>= 2 directions) — an endpoint cap
// (1 direction) or stray point (0) does not, since the straight edge pass
// already covered it. The square is centred so its band sits flush with the
// runs that feed it, which is exactly the pixel both perpendicular runs of a
// composed elbow stopped short of.
void RenderStrokeCorners(::Renderer & renderer, Surface * dst) {
	for(const StrokeCorner & sc : g_strokeCorners){
		if(sc.thickness < 1) continue;
		// Count connected cardinal directions.
		int n = 0;
		for(int b = 0; b < 4; b++) if(sc.dirs & (1 << b)) n++;
		if(n < 2) continue;  // straight edge pass already drew it
		FillStrokeStripe(renderer, dst, sc.clip,
		                 sc.x, sc.y, sc.thickness, sc.thickness,
		                 sc.color, sc.opacity);
	}
}

void DispatchRectangle(::Renderer & renderer,
                       Surface * dst,
                       const ::Clay_BoundingBox & bb,
                       const ::Clay_RectangleRenderData & data) {
	int x = static_cast<int>(bb.x);
	int y = static_cast<int>(bb.y);
	int w = static_cast<int>(bb.width);
	int h = static_cast<int>(bb.height);
	if(w <= 0 || h <= 0) return;
	if(!ClipDrawRect(dst->w, dst->h, x, y, w, h)) return;
	Uint8 color   = static_cast<Uint8>(data.backgroundColor.r);
	Uint8 opacity = static_cast<Uint8>(data.backgroundColor.a);
	if(opacity == 255){
		Renderer::DrawFilledRectangle(dst, x, y, x + w, y + h, color);
		return;
	}
	// Translucent fill: route each pixel through the palette's alphaed LUT.
	// The LUT collapses any "alpha > 0.5" ramp position to fully opaque, so
	// the practical intermediate level is ≈50%. Document this quantization
	// in the Rectangle primitive's header.
	Uint8 src = AlphaSrcIndex(color, opacity);
	for(int py = y; py < y + h; py++){
		for(int px = x; px < x + w; px++){
			Uint8 d       = Renderer::GetPixel(dst, px, py);
			Uint8 blended = renderer.palette.Alpha(src, d);
			Renderer::SetPixel(dst, px, py, blended);
		}
	}
}

void DispatchBorder(Surface * dst,
                    const ::Clay_BoundingBox & bb,
                    const ::Clay_BorderRenderData & data) {
	int x  = static_cast<int>(bb.x);
	int y  = static_cast<int>(bb.y);
	int w  = static_cast<int>(bb.width);
	int h  = static_cast<int>(bb.height);
	if(w <= 0 || h <= 0) return;
	Uint8 color = static_cast<Uint8>(data.color.r);

	// Top edge.
	if(data.width.top > 0){
		int ex = x, ey = y, ew = w, eh = static_cast<int>(data.width.top);
		if(ClipDrawRect(dst->w, dst->h, ex, ey, ew, eh))
			Renderer::DrawFilledRectangle(dst, ex, ey, ex + ew, ey + eh, color);
	}
	// Bottom edge.
	if(data.width.bottom > 0){
		int eh = static_cast<int>(data.width.bottom);
		int ex = x, ey = y + h - eh, ew = w;
		if(ClipDrawRect(dst->w, dst->h, ex, ey, ew, eh))
			Renderer::DrawFilledRectangle(dst, ex, ey, ex + ew, ey + eh, color);
	}
	// Left edge (skip the corners already covered by top/bottom).
	if(data.width.left > 0){
		int ew = static_cast<int>(data.width.left);
		int ex = x;
		int ey = y + static_cast<int>(data.width.top);
		int eh = h - static_cast<int>(data.width.top) - static_cast<int>(data.width.bottom);
		if(eh > 0 && ClipDrawRect(dst->w, dst->h, ex, ey, ew, eh))
			Renderer::DrawFilledRectangle(dst, ex, ey, ex + ew, ey + eh, color);
	}
	// Right edge.
	if(data.width.right > 0){
		int ew = static_cast<int>(data.width.right);
		int ex = x + w - ew;
		int ey = y + static_cast<int>(data.width.top);
		int eh = h - static_cast<int>(data.width.top) - static_cast<int>(data.width.bottom);
		if(eh > 0 && ClipDrawRect(dst->w, dst->h, ex, ey, ew, eh))
			Renderer::DrawFilledRectangle(dst, ex, ey, ex + ew, ey + eh, color);
	}
}

void DispatchText(::Resources & resources,
                  ::Renderer & renderer,
                  Surface * dst,
                  const ::Clay_BoundingBox & bb,
                  const ::Clay_TextRenderData & data,
                  void * userData) {
	int x = static_cast<int>(bb.x);
	int y = static_cast<int>(bb.y);
	if(data.stringContents.length <= 0) return;
	// DrawText needs a NUL-terminated C string. Slice may not be — copy to
	// a small stack buffer (longest lobby line is well under 256 chars).
	char buf[512];
	int n = std::min<int>(data.stringContents.length, sizeof(buf) - 1);
	std::memcpy(buf, data.stringContents.chars, n);
	buf[n] = '\0';

	// Trivial scissor enforcement: drop the whole call if the bbox is fully
	// outside the clip. Per-glyph clipping isn't required for the smoke test
	// (no scissor in the scene); pixel-perfect clipped text is a P11+ concern.
	int cx = x, cy = y;
	int cw = static_cast<int>(bb.width);
	int ch = static_cast<int>(bb.height);
	if(cw > 0 && ch > 0 && !ClipDrawRect(dst->w, dst->h, cx, cy, cw, ch)) return;

	Uint8 brightness = 128;
	bool colorRamp = false;
	bool alpha = false;
	if(userData){
		const TextDrawData * extra =
			reinterpret_cast<const TextDrawData *>(userData);
		brightness = extra->brightness;
		colorRamp  = extra->colorRamp;
		alpha      = extra->drawAlpha;
	}
	Uint8 bank   = static_cast<Uint8>(data.fontId);
	Uint8 width  = static_cast<Uint8>(data.fontSize);
	Uint8 color  = static_cast<Uint8>(data.textColor.r);
	TextInkMetrics ink = UsesInkMetrics(userData)
		? MeasureInkBounds(&resources,
		                    data.stringContents.chars,
		                    data.stringContents.length,
		                    data.fontId,
		                    data.fontSize)
		: TextInkMetrics{};
	if(ink.hasInk){
		// Pull the first glyph's leading blank columns flush to bb.x and
		// optically center the visible ink within the line box instead of
		// top-aligning it (the bitmap font reserves descender space, so a
		// line of caps would otherwise sit high). Mirrors the TextInput
		// custom-payload path.
		x -= ink.minX;
		if(ink.height > 0){
			y += std::max(0, (static_cast<int>(bb.height) - ink.height) / 2)
			     - ink.minY;
		}
	}
	renderer.DrawText(dst, static_cast<Uint16>(x), static_cast<Uint16>(y),
	                  buf, bank, width, alpha, color, brightness, colorRamp);
}

void DispatchImage(::Resources & resources,
                   Surface * dst,
                   const ::Clay_BoundingBox & bb,
                   const ::Clay_ImageRenderData & data) {
	Uint8 bank;
	Uint16 index;
	ImageFit fit;
	UnpackImage(data.imageData, bank, index, fit);
	const auto & banks = resources.spritebank;
	if(bank >= banks.size()) return;
	if(index >= banks[bank].size()) return;
	Surface * src = banks[bank][index].get();
	if(!src) return;
	if(src->w <= 0 || src->h <= 0) return;
	int bx = static_cast<int>(bb.x);
	int by = static_cast<int>(bb.y);
	int bw = static_cast<int>(bb.width);
	int bh = static_cast<int>(bb.height);
	if(bw <= 0 || bh <= 0) return;
	// Fit the sprite into its element box like CSS background-size. cover:
	// scale to fill, preserve aspect, crop overflow. contain: scale to fit
	// inside, preserve aspect, letterbox. stretch: fill both axes and allow
	// aspect distortion. Nearest sampling keeps pixel art crisp; the
	// compositor's later uiScale pass applies any whole-frame menu magnify.
	float sxScale = static_cast<float>(bw) / static_cast<float>(src->w);
	float syScale = static_cast<float>(bh) / static_cast<float>(src->h);
	float scale = fit == ImageFit::Contain ? std::min(sxScale, syScale)
	            : fit == ImageFit::Cover   ? std::max(sxScale, syScale)
	                                       : 1.0f;
	if(scale <= 0.0f || sxScale <= 0.0f || syScale <= 0.0f) return;
	int drawW = fit == ImageFit::Stretch
	          ? bw
	          : std::max(1, static_cast<int>(src->w * scale + 0.5f));
	int drawH = fit == ImageFit::Stretch
	          ? bh
	          : std::max(1, static_cast<int>(src->h * scale + 0.5f));
	int ox = bx + (bw - drawW) / 2;
	int oy = by + (bh - drawH) / 2;
	// Visible region = element box clipped to the scissor stack + surface.
	int rx = bx, ry = by, rw = bw, rh = bh;
	if(!ClipDrawRect(dst->w, dst->h, rx, ry, rw, rh)) return;
	for(int py = ry; py < ry + rh; py++){
		int syi = fit == ImageFit::Stretch
		        ? static_cast<int>((py - by) / syScale)
		        : static_cast<int>((py - oy) / scale);
		if(syi < 0 || syi >= src->h) continue;
		for(int px = rx; px < rx + rw; px++){
			int sxi = fit == ImageFit::Stretch
			        ? static_cast<int>((px - bx) / sxScale)
			        : static_cast<int>((px - ox) / scale);
			if(sxi < 0 || sxi >= src->w) continue;
			Uint8 col = Renderer::GetPixel(src, sxi, syi);
			if(col) Renderer::SetPixel(dst, px, py, col);
		}
	}
}

Surface * ResolveSprite(::Resources & resources, Uint8 bank, Uint16 index) {
	const auto & banks = resources.spritebank;
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
	if(!src || !dst || srcRect.w <= 0 || srcRect.h <= 0 || w <= 0 || h <= 0) return;
	for(int ty = 0; ty < h; ty += srcRect.h){
		int tileH = std::min(srcRect.h, h - ty);
		for(int tx = 0; tx < w; tx += srcRect.w){
			int tileW = std::min(srcRect.w, w - tx);
			Renderer::Rect tileSrc{tileW, tileH, srcRect.x, srcRect.y};
			BlitClipped(src, tileSrc, dst, x + tx, y + ty);
		}
	}
}

void DispatchButtonSprite(::Resources & resources,
                          ::Renderer & renderer,
                          Surface * dst,
                          const ::Clay_BoundingBox & bb,
                          const ButtonSpritePayload& payload) {
	Surface * src = ResolveSprite(resources, payload.bank, payload.index);
	if(!src) return;
	Surface * work = src;
	if(payload.brightness != 128){
		work = renderer.CreateSurfaceCopy(src);
		renderer.EffectBrightness(work, nullptr, payload.brightness);
	}
	Renderer::Rect srcRect{work->w, work->h, 0, 0};
	BlitClipped(work, srcRect, dst,
	            static_cast<int>(bb.x),
	            static_cast<int>(bb.y));
	if(work != src) delete work;
}

void DispatchButtonNineSlice(::Resources & resources,
                             ::Renderer & renderer,
                             Surface * dst,
                             const ::Clay_BoundingBox & bb,
                             const ButtonNineSlicePayload& payload) {
	Surface * src = ResolveSprite(resources, payload.bank, payload.index);
	if(!src) return;
	int x = static_cast<int>(bb.x);
	int y = static_cast<int>(bb.y);
	int w = static_cast<int>(bb.width);
	int h = static_cast<int>(bb.height);
	if(w <= 0 || h <= 0) return;

	Surface * work = src;
	if(payload.brightness != 128){
		work = renderer.CreateSurfaceCopy(src);
		renderer.EffectBrightness(work, nullptr, payload.brightness);
	}

	const int srcLeft = std::min<int>(payload.leftCap, work->w / 2);
	const int srcRight = std::min<int>(payload.rightCap, work->w - srcLeft);
	const int srcTop = std::min<int>(payload.topCap, work->h / 2);
	const int srcBottom = std::min<int>(payload.bottomCap, work->h - srcTop);
	const int dstLeft = std::min(srcLeft, w / 2);
	const int dstRight = std::min(srcRight, w - dstLeft);
	const int dstTop = std::min(srcTop, h / 2);
	const int dstBottom = std::min(srcBottom, h - dstTop);
	const int dstMidW = w - dstLeft - dstRight;
	const int dstMidH = h - dstTop - dstBottom;
	const int srcMidW = std::max(1, work->w - srcLeft - srcRight);
	const int srcMidH = std::max(1, work->h - srcTop - srcBottom);

	// Fill and edges are tiled from the center bands; corners stay cropped.
	TileClipped(work, Renderer::Rect{srcMidW, srcMidH, srcLeft, srcTop},
	            dst, x + dstLeft, y + dstTop, dstMidW, dstMidH);
	TileClipped(work, Renderer::Rect{srcMidW, dstTop, srcLeft, 0},
	            dst, x + dstLeft, y, dstMidW, dstTop);
	TileClipped(work, Renderer::Rect{srcMidW, dstBottom, srcLeft, work->h - dstBottom},
	            dst, x + dstLeft, y + h - dstBottom, dstMidW, dstBottom);
	TileClipped(work, Renderer::Rect{dstLeft, srcMidH, 0, srcTop},
	            dst, x, y + dstTop, dstLeft, dstMidH);
	TileClipped(work, Renderer::Rect{dstRight, srcMidH, work->w - dstRight, srcTop},
	            dst, x + w - dstRight, y + dstTop, dstRight, dstMidH);

	BlitClipped(work, Renderer::Rect{dstLeft, dstTop, 0, 0},
	            dst, x, y);
	BlitClipped(work, Renderer::Rect{dstRight, dstTop, work->w - dstRight, 0},
	            dst, x + w - dstRight, y);
	BlitClipped(work, Renderer::Rect{dstLeft, dstBottom, 0, work->h - dstBottom},
	            dst, x, y + h - dstBottom);
	BlitClipped(work, Renderer::Rect{dstRight, dstBottom, work->w - dstRight, work->h - dstBottom},
	            dst, x + w - dstRight, y + h - dstBottom);

	if(work != src) delete work;
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

}  // namespace

void EnsureInitialized(int width, int height) {
	if(!g_initialized){
		uint64_t mem = ::Clay_MinMemorySize();
		g_arenaMemory = std::malloc(static_cast<size_t>(mem));
		::Clay_Arena arena =
			::Clay_CreateArenaWithCapacityAndMemory(mem, g_arenaMemory);
		::Clay_ErrorHandler handler{HandleClayError, nullptr};
		::Clay_Dimensions dims{static_cast<float>(width),
		                       static_cast<float>(height)};
		g_context = ::Clay_Initialize(arena, dims, handler);
		::Clay_SetMeasureTextFunction(MeasureTextCommand, nullptr);
		g_initialized = true;
		g_lastW = width;
		g_lastH = height;
	}else{
		::Clay_SetCurrentContext(g_context);
		if(width != g_lastW || height != g_lastH){
			::Clay_SetLayoutDimensions(::Clay_Dimensions{
				static_cast<float>(width), static_cast<float>(height)});
			g_lastW = width;
			g_lastH = height;
		}
	}
	// Input is now owned by ClientUi/ClayService. Initialization must not reset
	// pointer or scroll state; Clay needs continuity across frames to report
	// pressed, held, released, and wheel behavior correctly.
}

void SetUiScale(float scale) {
	g_uiScale = scale > 0.0f ? scale : 1.0f;
}

float UiScale() {
	return g_uiScale;
}

void SetTextMeasureResources(const Resources * resources) {
	g_textMeasureResources = resources;
}

void RenderInto(::Resources & resources, ::Renderer & renderer,
                Surface * dst, ::Clay_RenderCommandArray cmds) {
	g_clipStack.clear();
	g_strokeCorners.clear();
	if(!dst) return;
	for(int i = 0; i < cmds.length; i++){
		::Clay_RenderCommand * c = &cmds.internalArray[i];
		switch(c->commandType){
			case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
				DispatchRectangle(renderer, dst, c->boundingBox,
				                  c->renderData.rectangle);
				break;
			case CLAY_RENDER_COMMAND_TYPE_BORDER:
				DispatchBorder(dst, c->boundingBox, c->renderData.border);
				break;
			case CLAY_RENDER_COMMAND_TYPE_TEXT:
				DispatchText(resources, renderer, dst, c->boundingBox,
				             c->renderData.text, c->userData);
				break;
			case CLAY_RENDER_COMMAND_TYPE_IMAGE:
				DispatchImage(resources, dst, c->boundingBox, c->renderData.image);
				break;
			case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
				ClipRect r;
				ClipRect cur;
				CurrentClip(dst->w, dst->h, cur);
				int x1 = std::max<int>(static_cast<int>(c->boundingBox.x), cur.x);
				int y1 = std::max<int>(static_cast<int>(c->boundingBox.y), cur.y);
				int x2 = std::min<int>(
					static_cast<int>(c->boundingBox.x + c->boundingBox.width),
					cur.x + cur.w);
				int y2 = std::min<int>(
					static_cast<int>(c->boundingBox.y + c->boundingBox.height),
					cur.y + cur.h);
				r.x = x1; r.y = y1;
				r.w = std::max(0, x2 - x1);
				r.h = std::max(0, y2 - y1);
				g_clipStack.push_back(r);
				break;
			}
			case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
				if(!g_clipStack.empty()) g_clipStack.pop_back();
				break;
			case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
				const auto * ccd = reinterpret_cast<const ClayCustomData *>(
					c->renderData.custom.customData);
				if(!ccd) break;
				switch(ccd->kind){
					case CustomKind::ButtonSprite: {
						const auto * p = reinterpret_cast<const ButtonSpritePayload *>(ccd->payload);
						if(!p) break;
						DispatchButtonSprite(resources, renderer, dst, c->boundingBox, *p);
						break;
					}
					case CustomKind::ButtonNineSlice: {
						const auto * p = reinterpret_cast<const ButtonNineSlicePayload *>(ccd->payload);
						if(!p) break;
						DispatchButtonNineSlice(resources, renderer, dst, c->boundingBox, *p);
						break;
					}
					case CustomKind::ToggleSprite: {
						const auto * p = reinterpret_cast<const TogglePayload *>(ccd->payload);
						if(!p) break;
						const auto & banks = resources.spritebank;
						if(p->bank >= banks.size()) break;
						if(p->index >= banks[p->bank].size()) break;
						Surface * src = banks[p->bank][p->index].get();
						if(!src) break;
						int x = static_cast<int>(c->boundingBox.x);
						int y = static_cast<int>(c->boundingBox.y);
						int w = src->w;
						int h = src->h;
						int cx = x, cy = y, cw = w, ch = h;
						if(!ClipDrawRect(dst->w, dst->h, cx, cy, cw, ch)) break;
						Renderer::Rect dstrect{w, h, x, y};
						bool needsCopy = (p->effectColor != 0) || (p->brightness != 128);
						if(needsCopy){
							Surface * copy = renderer.CreateSurfaceCopy(src);
							if(p->effectColor != 0)
								renderer.EffectColor(copy, nullptr, p->effectColor);
							if(p->brightness != 128)
								renderer.EffectBrightness(copy, nullptr, p->brightness);
							Renderer::BlitSurface(copy, nullptr, dst, &dstrect);
							delete copy;
						}else{
							Renderer::BlitSurface(src, nullptr, dst, &dstrect);
						}
						break;
					}
					case CustomKind::Sprite: {
						const auto * p = reinterpret_cast<const SpritePayload *>(ccd->payload);
						if(!p) break;
						const auto & banks = resources.spritebank;
						if(p->bank >= banks.size()) break;
						if(p->index >= banks[p->bank].size()) break;
						Surface * src = banks[p->bank][p->index].get();
						if(!src) break;
						Renderer::Rect srcrect{
							p->srcW > 0 ? p->srcW : src->w,
							p->srcH > 0 ? p->srcH : src->h,
							p->srcX,
							p->srcY,
						};
						if(srcrect.x < 0) srcrect.x = 0;
						if(srcrect.y < 0) srcrect.y = 0;
						if(srcrect.x + srcrect.w > src->w) srcrect.w = src->w - srcrect.x;
						if(srcrect.y + srcrect.h > src->h) srcrect.h = src->h - srcrect.y;
						if(srcrect.w <= 0 || srcrect.h <= 0) break;
						Renderer::Rect dstrect{
							srcrect.w,
							srcrect.h,
							static_cast<int>(c->boundingBox.x) + p->dstOffsetX,
							static_cast<int>(c->boundingBox.y) + p->dstOffsetY,
						};
						int cx = dstrect.x, cy = dstrect.y, cw = dstrect.w, ch = dstrect.h;
						if(!ClipDrawRect(dst->w, dst->h, cx, cy, cw, ch)) break;
						bool needsCopy = (p->effectColor != 0) || (p->rampColor != 0) || (p->brightness != 128);
						if(needsCopy){
							Surface * copy = renderer.CreateSurfaceCopy(src);
							if(p->effectColor != 0)
								renderer.EffectColor(copy, nullptr, p->effectColor);
							if(p->rampColor != 0){
								if(p->rampPlus != 0){
									renderer.EffectRampColorPlus(copy, nullptr, p->rampColor, p->rampPlus);
								}else{
									renderer.EffectRampColor(copy, nullptr, p->rampColor);
								}
							}
							if(p->brightness != 128)
								renderer.EffectBrightness(copy, nullptr, p->brightness);
							Renderer::BlitSurface(copy, &srcrect, dst, &dstrect);
							delete copy;
						}else{
							Renderer::BlitSurface(src, &srcrect, dst, &dstrect);
						}
						break;
					}
					case CustomKind::TeamEmblem: {
						const auto * p = reinterpret_cast<const TeamEmblemPayload *>(ccd->payload);
						if(!p) break;
						const auto & banks = resources.spritebank;
						if(p->bank >= banks.size()) break;
						if(p->index >= banks[p->bank].size()) break;
						Surface * src = banks[p->bank][p->index].get();
						if(!src) break;
						int scale = p->scaled ? 2 : 1;
						int x = static_cast<int>(c->boundingBox.x);
						int y = static_cast<int>(c->boundingBox.y);
						int w = src->w * scale;
						int h = src->h * scale;
						int cx = x, cy = y, cw = w, ch = h;
						if(!ClipDrawRect(dst->w, dst->h, cx, cy, cw, ch)) break;
						Surface * copy = renderer.CreateSurfaceCopy(src);
						renderer.EffectTeamColor(copy, nullptr, p->teamColor, false, true);
						OutlineVisiblePixels(renderer, src, copy, p->outlineColor);
						Renderer::Rect dstrect{w, h, x, y};
						if(p->scaled){
							Renderer::DrawScaled(copy, nullptr, dst, &dstrect);
						}else{
							Renderer::BlitSurface(copy, nullptr, dst, &dstrect);
						}
						delete copy;
						break;
					}
					case CustomKind::Surface: {
						const auto * p = reinterpret_cast<const SurfacePayload *>(ccd->payload);
						if(!p || !p->pixels || p->width == 0 || p->height == 0) break;
						int x = static_cast<int>(c->boundingBox.x);
						int y = static_cast<int>(c->boundingBox.y);
						int w = static_cast<int>(c->boundingBox.width);
						int h = static_cast<int>(c->boundingBox.height);
						if(w <= 0) w = p->width;
						if(h <= 0) h = p->height;
						int cx = x, cy = y, cw = w, ch = h;
						if(!ClipDrawRect(dst->w, dst->h, cx, cy, cw, ch)) break;
						Surface srcsurface;
						srcsurface.w = p->width;
						srcsurface.h = p->height;
						srcsurface.pixels.assign(
							p->pixels, p->pixels + (static_cast<size_t>(p->width) * p->height));
						Renderer::Rect dstrect{w, h, x, y};
						Renderer::BlitSurface(&srcsurface, nullptr, dst, &dstrect);
						break;
					}
					case CustomKind::ScrollBar: {
						const auto * p = reinterpret_cast<const ScrollBarPayload *>(ccd->payload);
						if(!p) break;
						const auto & res = resources;
						const auto & banks = res.spritebank;
						if(p->bank >= banks.size()) break;
						if(p->trackIndex >= banks[p->bank].size()) break;
						Surface * track = banks[p->bank][p->trackIndex].get();
						if(!track) break;
						// Sprite-offset compensation. The scrollbar track and
						// thumb sprites carry a baked anchor offset
						// (spriteoffsetx/y[bank][index]) — the legacy renderer
						// subtracts this before BlitSurface so the visible
						// top-left lands at the object's logical position
						// (see game_create_panel.cpp:314 + renderer.cpp:382).
						// The Clay bridge takes the bbox as "where the visible
						// scrollbar should land", so we mirror the legacy
						// subtraction here. Without it, the rendered sprite
						// lands shifted from Clay's layout (visible at
						// bbox.x + spriteoffsetx instead of bbox.x).
						const int trackOffX = res.spriteoffsetx[p->bank][p->trackIndex];
						const int trackOffY = res.spriteoffsety[p->bank][p->trackIndex];
						int x  = static_cast<int>(c->boundingBox.x) - trackOffX;
						int y  = static_cast<int>(c->boundingBox.y) - trackOffY;
						int bw = static_cast<int>(c->boundingBox.width);
						int bh = static_cast<int>(c->boundingBox.height);
						if(bw <= 0 || bh <= 0) break;
						// Coarse outside-clip cull; per-pixel writes below pass
						// through the clip stack via the SetPixel calls.
						int cx = x, cy = y, cw = bw, ch = bh;
						if(!ClipDrawRect(dst->w, dst->h, cx, cy, cw, ch)) break;
						// Render 3-slice track. Mirrors renderer.cpp:867-902.
						const int cap = 16;
						int trackh = bh;
						int srcw   = track->w;
						int srch   = track->h;
						if(srch > 2 * cap){
							int srcmidtop = cap;
							int srcmidh   = srch - 2 * cap;
							int dstmidh   = trackh - 2 * cap;
							if(dstmidh < 0) dstmidh = 0;
							ClipRect clip;
							CurrentClip(dst->w, dst->h, clip);
							auto inClip = [&](int px, int py){
								return px >= clip.x && py >= clip.y &&
								       px < clip.x + clip.w && py < clip.y + clip.h;
							};
							// Top cap.
							for(int dy = 0; dy < cap && dy < trackh; dy++){
								for(int dx = 0; dx < srcw; dx++){
									Uint8 col = Renderer::GetPixel(track, dx, dy);
									if(col && inClip(x + dx, y + dy))
										Renderer::SetPixel(dst, x + dx, y + dy, col);
								}
							}
							// Middle band — tile source middle rows.
							for(int dy = 0; dy < dstmidh; dy++){
								int sy = srcmidtop + (dy % srcmidh);
								for(int dx = 0; dx < srcw; dx++){
									Uint8 col = Renderer::GetPixel(track, dx, sy);
									if(col && inClip(x + dx, y + cap + dy))
										Renderer::SetPixel(dst, x + dx, y + cap + dy, col);
								}
							}
							// Bottom cap.
							for(int dy = 0; dy < cap; dy++){
								int sy    = srch - cap + dy;
								int dyabs = trackh - cap + dy;
								if(dyabs < 0) continue;
								for(int dx = 0; dx < srcw; dx++){
									Uint8 col = Renderer::GetPixel(track, dx, sy);
									if(col && inClip(x + dx, y + dyabs))
										Renderer::SetPixel(dst, x + dx, y + dyabs, col);
								}
							}
						}
						// Thumb (renderer.cpp:908-933). Cropped from the top.
						if(p->thumbIndex >= banks[p->bank].size()) break;
						Surface * thumb = banks[p->bank][p->thumbIndex].get();
						if(thumb && p->scrollMax > 0){
							int available = trackh - 2 * cap;
							if(available < 1) available = 1;
							int thumbh = available - static_cast<int>(p->scrollMax);
							if(thumbh > available) thumbh = available;
							if(thumbh > thumb->h) thumbh = thumb->h;
							if(thumbh < 16) thumbh = 16;
							if(thumbh > available) thumbh = available;
							int travel = available - thumbh;
							if(travel < 0) travel = 0;
							float pos = static_cast<float>(p->scrollPosition) /
							            static_cast<float>(p->scrollMax);
							int dsty = static_cast<int>(travel * pos);
							Renderer::Rect srcrect{thumb->w, thumbh, 0, 0};
							Renderer::Rect thumbdst{thumb->w, thumbh,
							                        x + 1, y + cap + dsty};
							Renderer::BlitSurface(thumb, &srcrect, dst, &thumbdst);
						}
						break;
					}
					case CustomKind::BoxStroke: {
						const auto * p = reinterpret_cast<const BoxStrokePayload *>(ccd->payload);
						if(!p) break;
						int bx = static_cast<int>(c->boundingBox.x);
						int by = static_cast<int>(c->boundingBox.y);
						int bw = static_cast<int>(c->boundingBox.width);
						int bh = static_cast<int>(c->boundingBox.height);
						if(bw <= 0 || bh <= 0) break;
						// Per-side mask (1=top, 2=right, 4=bottom, 8=left).
						// 0 falls back to all four sides for back-compat.
						Uint8 sides = p->sides ? p->sides : 0xF;
						bool sTop    = (sides & 0x1) != 0;
						bool sRight  = (sides & 0x2) != 0;
						bool sBottom = (sides & 0x4) != 0;
						bool sLeft   = (sides & 0x8) != 0;
						ClipRect clip;
						if(!CurrentClip(dst->w, dst->h, clip)) break;
						// Each concentric band (outer halo / stroke / inner
						// halo) draws as up to four straight runs at the
						// box's OWN bbox -- no per-box outward extension.
						// Where two composed boxes meet, each run stops at
						// its own bbox and pass 1 records the band's four
						// corner cells with the directions the band
						// continues in. Pass 2 (RenderStrokeCorners, after
						// the whole command stream) fills one
						// band-thickness square at every corner that ended
						// up connected from two or more directions, so the
						// shared elbow is contiguous. A plain closed
						// rectangle's corner cell lands exactly on the old
						// stepped-ownership overlap, so ordinary chrome is
						// byte-identical (issue #176).
						auto drawRing = [&](int inset, int thickness, Uint8 color, Uint8 opacity){
							int t = thickness;
							if(t < 1) return;
							if(t * 2 > bw) t = bw / 2;
							if(t * 2 > bh) t = bh / 2;
							if(t < 1) return;
							// Top-left of each thickness-square corner cell.
							const int cx0 = bx + inset;           // left
							const int cx1 = bx + bw - inset - t;   // right
							const int cy0 = by + inset;            // top
							const int cy1 = by + bh - inset - t;   // bottom
							// Classic stepped ownership: a run is trimmed
							// by `t` at each end that has an active
							// perpendicular side, leaving the corner cell
							// for pass 2.
							int hLeftTrim  = sLeft   ? t : 0;
							int hRightTrim = sRight  ? t : 0;
							int vTopTrim   = sTop    ? t : 0;
							int vBotTrim   = sBottom ? t : 0;
							// Top run.
							if(sTop){
								int x = cx0 + hLeftTrim;
								int w = bw - 2 * inset - hLeftTrim - hRightTrim;
								FillStrokeStripe(renderer, dst, clip,
								                 x, cy0, w, t, color, opacity);
							}
							// Bottom run.
							if(sBottom){
								int x = cx0 + hLeftTrim;
								int w = bw - 2 * inset - hLeftTrim - hRightTrim;
								FillStrokeStripe(renderer, dst, clip,
								                 x, cy1, w, t, color, opacity);
							}
							// Left run.
							if(sLeft){
								int y = cy0 + vTopTrim;
								int h = bh - 2 * inset - vTopTrim - vBotTrim;
								FillStrokeStripe(renderer, dst, clip,
								                 cx0, y, t, h, color, opacity);
							}
							// Right run.
							if(sRight){
								int y = cy0 + vTopTrim;
								int h = bh - 2 * inset - vTopTrim - vBotTrim;
								FillStrokeStripe(renderer, dst, clip,
								                 cx1, y, t, h, color, opacity);
							}
							// Pass 1: register the band's four corner cells
							// with the directions this band continues in.
							// The direction mask accumulates across every
							// box sharing the cell, so a composed junction
							// sees both neighbours' connectivity.
							auto reg = [&](int cx, int cy, Uint8 dirs){
								if(dirs)
									AddStrokeCorner(cx, cy, t, color,
									                opacity, dirs, clip);
							};
							reg(cx0, cy0,
							    (sTop  ? corner_dir::Right : (Uint8)0) |
							    (sLeft ? corner_dir::Down  : (Uint8)0));
							reg(cx1, cy0,
							    (sTop   ? corner_dir::Left : (Uint8)0) |
							    (sRight ? corner_dir::Down : (Uint8)0));
							reg(cx0, cy1,
							    (sBottom ? corner_dir::Right : (Uint8)0) |
							    (sLeft   ? corner_dir::Up    : (Uint8)0));
							reg(cx1, cy1,
							    (sBottom ? corner_dir::Left : (Uint8)0) |
							    (sRight  ? corner_dir::Up   : (Uint8)0));
						};
						int inset = 0;
						if(p->outerHaloWidth > 0){
							drawRing(inset, p->outerHaloWidth, p->outerHaloColor, p->haloOpacity);
							inset += p->outerHaloWidth;
						}
						if(p->strokeWidth > 0){
							drawRing(inset, p->strokeWidth, p->strokeColor, 255);
							inset += p->strokeWidth;
						}
						if(p->innerHaloWidth > 0){
							drawRing(inset, p->innerHaloWidth, p->innerHaloColor, p->haloOpacity);
						}
						break;
					}
					case CustomKind::TextInput: {
						const auto * p = reinterpret_cast<const TextInputPayload *>(ccd->payload);
						if(!p) break;
						if(!p->text) break;
						const auto style = silencer::ui::primitives::text_internal::ResolveTextRenderStyle(
							static_cast<silencer::ui::primitives::TextSize>(p->textSize));
						int x = static_cast<int>(c->boundingBox.x);
						int y = static_cast<int>(c->boundingBox.y);
						int boxH = static_cast<int>(c->boundingBox.height);
						// Center on the fixed font line box, never on per-string
						// ink bounds: ink height/minY change with descenders
						// (e.g. 'y'), which made the whole string jump up a
						// pixel or two (issue #175). Legacy DrawTextInput drew
						// at a fixed y for the same reason.
						y += std::max(0, (boxH - static_cast<int>(style.lineHeight)) / 2);
						renderer.DrawText(dst,
						                  static_cast<Uint16>(x),
						                  static_cast<Uint16>(y),
						                  p->text,
						                  static_cast<Uint8>(style.bank),
						                  static_cast<Uint8>(style.advance),
						                  /*centered=*/false,
						                  p->effectColor,
						                  p->brightness,
						                  /*colorRamp=*/false);
						if(p->showCaret){
							int cx = x + static_cast<int>(p->textLen) *
							             static_cast<int>(style.advance);
							int cy = y - 1;
							int cw = 1;
							int ch = static_cast<int>(p->caretHeight);
							int rx = cx, ry = cy, rw = cw, rh = ch;
							if(ClipDrawRect(dst->w, dst->h, rx, ry, rw, rh))
								Renderer::DrawFilledRectangle(
									dst, rx, ry, rx + rw, ry + rh, p->caretColor);
						}
						break;
					}
					case CustomKind::None:
					default:
						break;
				}
				break;
			}
			default:
				break;
		}
	}
	// Pass 2: bridge every stroke corner that ended up connected from
	// two or more directions. Runs once after the whole command stream
	// so a corner shared by adjacent composed boxes has accumulated
	// connectivity from all of them (issue #176).
	RenderStrokeCorners(renderer, dst);
}

// Clay lays out in a virtual coordinate space (the native surface size
// divided by uiScale). At uiScale 1 we draw straight into `dst` — byte-
// identical to the pre-scale path. At uiScale > 1 we render the command
// stream into a virtual-size scratch surface (every dispatch path, scissor,
// sprite-offset and alpha-LUT computation stays in virtual units exactly as
// authored) and then nearest-scale that scratch into a centered region of
// the native surface. Integer menu sizes still land on exact pixel repeats;
// non-integer menu sizes trade some pixel regularity for smooth resize
// behavior instead of abrupt 1x/2x jumps.
void Render(::Resources & resources, ::Renderer & renderer,
            Surface * dst, ::Clay_RenderCommandArray cmds) {
	if(!dst) return;
	float s = g_uiScale;
	if(s <= 1.0f){
		RenderInto(resources, renderer, dst, cmds);
		return;
	}
	// Scratch is sized to the dimensions Clay actually laid out at (set by
	// EnsureInitialized). For menus that equals dst/s; in-game the HUD lays
	// out in a fixed 640x480 space that does NOT equal dst/s, so derive the
	// scratch size from the layout, never from dst/s.
	int vw = (g_lastW > 0) ? g_lastW : std::max(1, static_cast<int>(dst->w / s));
	int vh = (g_lastH > 0) ? g_lastH : std::max(1, static_cast<int>(dst->h / s));
	if(vw < 1) vw = 1;
	if(vh < 1) vh = 1;
	// Reused across frames; only reallocates when the virtual size changes.
	static Surface scratch;
	if(scratch.w != vw || scratch.h != vh){
		scratch.Resize(vw, vh, 0);
	}else{
		scratch.Clear(0);
	}
	RenderInto(resources, renderer, &scratch, cmds);
	const Uint8 * sp = scratch.pixels.data();
	Uint8 * dp = dst->pixels.data();
	int scaledW = static_cast<int>(vw * s + 0.5f);
	int scaledH = static_cast<int>(vh * s + 0.5f);
	int offsetX = scaledW < dst->w ? (dst->w - scaledW) / 2 : 0;
	int offsetY = scaledH < dst->h ? (dst->h - scaledH) / 2 : 0;
	int drawW = std::min(scaledW, dst->w - offsetX);
	int drawH = std::min(scaledH, dst->h - offsetY);
	for(int dy = 0; dy < drawH; dy++){
		int sy = static_cast<int>(dy / s);
		if(sy >= vh) sy = vh - 1;
		const Uint8 * srow = sp + sy * vw;
		Uint8 * drow = dp + (offsetY + dy) * dst->w + offsetX;
		for(int dx = 0; dx < drawW; dx++){
			int sx = static_cast<int>(dx / s);
			if(sx >= vw) sx = vw - 1;
			// Skip transparent (index 0): compose the UI layer over whatever
			// is already in dst (the upscaled world in-game, black in menus)
			// instead of overwriting it.
			Uint8 c = srow[sx];
			if(c) drow[dx] = c;
		}
	}
}

void Render(::Game & game, Surface * dst, ::Clay_RenderCommandArray cmds) {
	Render(game.GetWorld().resources, game.GetRenderer(), dst, cmds);
}

}  // namespace silencer::clay_bridge
