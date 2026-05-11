#include "clay_bridge.h"

#include "game.h"
#include "renderer.h"
#include "resources.h"
#include "surface.h"
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

// Per-bank text height. Mirrors the Overlay::MouseInside switch
// (overlay.cpp:99). Banks 132 (TinyText), 133 (Body), 134 (Heading),
// 135 (Title), 136 reserved.
Uint16 BankTextHeight(uint16_t bank) {
	switch(bank){
		case 132: return 7;
		case 133: return 11;
		case 134: return 15;
		case 135: return 19;
		case 136: return 23;
		default:  return 11;
	}
}

::Clay_Dimensions MeasureBankText(::Clay_StringSlice text,
                                  ::Clay_TextElementConfig * config,
                                  void * /*userData*/) {
	// Bank fonts are monospaced — the existing DrawText path advances by a
	// fixed `width` per character, regardless of glyph. So we just scale by
	// length here. fontSize == cell width in pixels.
	::Clay_Dimensions out;
	out.width  = static_cast<float>(text.length * config->fontSize);
	out.height = static_cast<float>(BankTextHeight(config->fontId));
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

void UnpackImage(void * p, Uint8 & bank, Uint16 & index) {
	std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
	bank  = static_cast<Uint8>((v >> 16) & 0xFFu);
	index = static_cast<Uint16>(v & 0xFFFFu);
}

void DispatchRectangle(Surface * dst,
                       const ::Clay_BoundingBox & bb,
                       const ::Clay_RectangleRenderData & data) {
	int x = static_cast<int>(bb.x);
	int y = static_cast<int>(bb.y);
	int w = static_cast<int>(bb.width);
	int h = static_cast<int>(bb.height);
	if(w <= 0 || h <= 0) return;
	if(!ClipDrawRect(dst->w, dst->h, x, y, w, h)) return;
	Uint8 color = static_cast<Uint8>(data.backgroundColor.r);
	Renderer::DrawFilledRectangle(dst, x, y, x + w, y + h, color);
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

void DispatchText(::Renderer & renderer,
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
		const BankTextDrawData * extra =
			reinterpret_cast<const BankTextDrawData *>(userData);
		brightness = extra->brightness;
		colorRamp  = extra->colorRamp;
		alpha      = extra->drawAlpha;
	}
	Uint8 bank   = static_cast<Uint8>(data.fontId);
	Uint8 width  = static_cast<Uint8>(data.fontSize);
	Uint8 color  = static_cast<Uint8>(data.textColor.r);
	renderer.DrawText(dst, static_cast<Uint16>(x), static_cast<Uint16>(y),
	                  buf, bank, width, alpha, color, brightness, colorRamp);
}

void DispatchImage(::Game & game,
                   Surface * dst,
                   const ::Clay_BoundingBox & bb,
                   const ::Clay_ImageRenderData & data) {
	Uint8 bank;
	Uint16 index;
	UnpackImage(data.imageData, bank, index);
	const auto & banks = game.GetWorld().resources.spritebank;
	if(bank >= banks.size()) return;
	if(index >= banks[bank].size()) return;
	Surface * src = banks[bank][index].get();
	if(!src) return;
	// Blit at the bbox top-left using the sprite's natural size. Layout is
	// expected to size the element to match the sprite; mismatches would
	// require a scaled blit which the existing pipeline doesn't support.
	int x = static_cast<int>(bb.x);
	int y = static_cast<int>(bb.y);
	int w = src->w;
	int h = src->h;
	// Only do a coarse outside-clip cull — BlitSurfaceUpper already clamps
	// per-pixel against `dst`'s bounds. Sub-rect clipping during scissor
	// is a P11+ concern (smoke test has no scissor).
	int cx = x, cy = y, cw = w, ch = h;
	if(!ClipDrawRect(dst->w, dst->h, cx, cy, cw, ch)) return;
	Renderer::Rect dstrect{w, h, x, y};
	Renderer::BlitSurface(src, nullptr, dst, &dstrect);
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
		::Clay_SetMeasureTextFunction(MeasureBankText, nullptr);
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
	// Reset per-frame inputs so two back-to-back layout passes from the same
	// process produce byte-identical render commands. Clay's internal pointer
	// + scroll + text-cache state is otherwise carried forward and produces
	// non-deterministic geometry on repeated calls (observed: 1.2% pixdiff
	// between consecutive smoke renders without these resets).
	::Clay_SetPointerState(::Clay_Vector2{-1.0f, -1.0f}, false);
	::Clay_UpdateScrollContainers(false, ::Clay_Vector2{0, 0}, 0.0f);
	::Clay_ResetMeasureTextCache();
}

void Render(::Game & game, Surface * dst, ::Clay_RenderCommandArray cmds) {
	g_clipStack.clear();
	if(!dst) return;
	::Renderer & renderer = game.GetRenderer();
	for(int i = 0; i < cmds.length; i++){
		::Clay_RenderCommand * c = &cmds.internalArray[i];
		switch(c->commandType){
			case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
				DispatchRectangle(dst, c->boundingBox, c->renderData.rectangle);
				break;
			case CLAY_RENDER_COMMAND_TYPE_BORDER:
				DispatchBorder(dst, c->boundingBox, c->renderData.border);
				break;
			case CLAY_RENDER_COMMAND_TYPE_TEXT:
				DispatchText(renderer, dst, c->boundingBox,
				             c->renderData.text, c->userData);
				break;
			case CLAY_RENDER_COMMAND_TYPE_IMAGE:
				DispatchImage(game, dst, c->boundingBox, c->renderData.image);
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
					case CustomKind::BankButtonChrome: {
						const auto * p = reinterpret_cast<const BankButtonChromePayload *>(ccd->payload);
						if(!p) break;
						const auto & banks = game.GetWorld().resources.spritebank;
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
						if(p->brightness != 128){
							Surface * copy = renderer.CreateSurfaceCopy(src);
							renderer.EffectBrightness(copy, nullptr, p->brightness);
							Renderer::BlitSurface(copy, nullptr, dst, &dstrect);
							delete copy;
						}else{
							Renderer::BlitSurface(src, nullptr, dst, &dstrect);
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
}

}  // namespace silencer::clay_bridge
