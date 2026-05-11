#ifndef SILENCER_UI_CLAY_BRIDGE_H
#define SILENCER_UI_CLAY_BRIDGE_H

// Clay → Silencer Surface render bridge.
//
// Translates a Clay_RenderCommandArray into draw calls against the existing
// 8-bit paletted Surface + sprite-bank pipeline. Designed so screens never
// touch SDL or Surface directly: they emit Clay trees, this bridge dispatches
// to Renderer::DrawText / Renderer::DrawFilledRectangle / Renderer::BlitSurface.
//
// Conventions assumed by all primitives downstream:
//
//   • RECTANGLE color: render command's backgroundColor.r is interpreted as a
//     palette index (Uint8). Other channels ignored.
//   • BORDER color: same. Width applied per side.
//   • TEXT: textConfig.fontId  → bank (135 Title, 134 Heading, 133 Body, 132 Tiny).
//           textConfig.fontSize → cell width (monospaced bank fonts).
//           textColor.r        → palette index for the EffectColor tint.
//           userData (optional) → BankTextDrawData* with brightness / colorRamp.
//   • IMAGE: imageConfig.imageData = PackImage(bank, index). The bridge looks
//     up world.resources.spritebank[bank][index] and BlitSurfaces it at the
//     bbox top-left using the sprite's natural width/height.
//   • SCISSOR_START/END: maintained as a clip-rect stack. Subsequent draws
//     are intersected with the top of the stack before being submitted.
//   • CUSTOM: customData = ClayCustomData* with a kind enum the bridge
//     switches on. Reserved for things that don't map cleanly to the other
//     command types (button chrome, scrollbar track, toggle face).

#include "clay/clay.h"
#include "shared.h"
#include <cstdint>

class Game;
class Surface;

namespace silencer::clay_bridge {

// Lazily allocates Clay's arena and registers the bank-text MeasureText
// callback on first call. Subsequent calls only update layout dimensions.
// Safe to call from any per-frame Tick before BeginLayout.
void EnsureInitialized(int width, int height);

// Walks `cmds` and dispatches each command to the matching Renderer
// primitive, writing into `dst`. Game gives the bridge both the Renderer
// instance (for DrawText / DrawFilledRectangle / BlitSurface) and the World's
// Resources (for sprite-bank lookups).
void Render(::Game & game, Surface * dst, ::Clay_RenderCommandArray cmds);

// Runs the P3 smoke scene (one rect + one border + one text + one image)
// through the bridge into a fresh 640x480 Surface and writes it to `outPath`
// as a PNG. Returns false only on PNG-write failure. Implementation lives in
// clay_smoke.cpp and is invoked by the `clay_bridge_smoke` control op.
bool RunSmoke(::Game & game, const char * outPath);

// Pack a (bank, index) pair into a void* for Clay_ImageElementConfig.imageData.
// Bank fits in the high 16 bits, index in the low 16. Round-trip via
// UnpackImage(); decoder lives in clay_bridge.cpp.
inline void * PackImage(Uint8 bank, Uint16 index) {
	std::uintptr_t v = (static_cast<std::uintptr_t>(bank) << 16) |
	                   static_cast<std::uintptr_t>(index);
	return reinterpret_cast<void *>(v);
}

// Optional userData payload for CLAY_TEXT — extra bank-text effects beyond
// the textColor tint Clay already passes through. NULL is fine; defaults
// match the legacy Overlay text path (brightness=128, no ramp).
struct BankTextDrawData {
	Uint8 brightness;     // 128 = neutral (legacy default).
	bool colorRamp;       // True → EffectRampColor instead of EffectColor.
	bool drawAlpha;       // True → DrawAlphaed glyph blit.
};

// CUSTOM render command payload. Tag the kind so the bridge can dispatch.
// Kinds added by primitives that need it (button chrome, scrollbar, toggle).
enum class CustomKind : Uint8 {
	None = 0,
	// Reserved for primitives in P5+ — bridge only switches on these.
};

struct ClayCustomData {
	CustomKind kind;
	void * payload;
};

}  // namespace silencer::clay_bridge

#endif
