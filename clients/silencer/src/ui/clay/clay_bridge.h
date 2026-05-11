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

// P4 BankText primitive unit test. Renders the lobby title (the literal
// string "Silencer") via the BankText primitive at (15, 32) using the
// Title variant + effectColor=152, through the bridge into a fresh 640x480
// Surface, and writes it to `outPath`. Invoked by the `clay_bank_text_test`
// control op. Implementation in bank_text_test.cpp.
bool RunBankTextTest(::Game & game, const char * outPath);

// P5 BankButton primitive unit test. Renders one of three variants
// (Chrome / Inline / Checkbox) into a fresh 640x480 Surface and writes it
// to `outPath`. `variant` is one of "chrome", "inline", "checkbox" — any
// other value selects "chrome". Invoked by the `clay_bank_button_test`
// control op. Implementation in bank_button_test.cpp.
bool RunBankButtonTest(::Game & game, const char * variant, const char * outPath);

// P5 BankButton hover/click parity check. Runs the click + hover logic in
// the BankButton::Chrome variant against a deterministic pointer-state
// timeline and reports the results as raw counters in the output struct.
// No PNG is produced — this complements the render-parity test op.
struct BankButtonCheckResult {
	int clicksFiredOnPress;     // Expect 1 — Clay_OnHover fires the proxy on PRESSED_THIS_FRAME.
	int clicksFiredWhenHeld;    // Expect 0 — held frames don't re-fire the proxy.
	int chromeBrightnessHover;  // Expect 136 — the CUSTOM payload's brightness when hovered.
	int chromeBrightnessIdle;   // Expect 128 — the CUSTOM payload's brightness when not hovered.
};
bool RunBankButtonCheck(::Game & game, BankButtonCheckResult & out);

// P6 Toggle primitive unit test. Renders one Toggle (bank 181 idx 0,
// effectColor=112) with the given selected state into a 640x480 Surface
// and writes it to `outPath`. `state` must be "selected" or "unselected"
// (any other value picks "unselected"). Invoked by the `clay_toggle_test`
// control op. Implementation in toggle_test.cpp.
bool RunToggleTest(::Game & game, const char * state, const char * outPath);

// P6 Toggle click-routing check. Lays out three Toggles in a row, each
// with a distinct `user` pointer. Drives the pointer through a press
// timeline over the middle toggle and reports which toggle's callback
// fired and how many times. No PNG produced.
struct ToggleCheckResult {
	int clicksToggle0;       // Expect 0.
	int clicksToggle1;       // Expect 1.
	int clicksToggle2;       // Expect 0.
	int selectedBrightness;  // Expect 128 — the selected toggle's payload brightness.
	int unselectedBrightness;// Expect 32  — an unselected toggle's payload brightness.
};
bool RunToggleCheck(::Game & game, ToggleCheckResult & out);

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
	BankButtonChrome,  // P5: sprite-faced button (B156x21) with brightness effect.
	ToggleSprite,      // P6: sprite-faced radio with effectColor + brightness.
};

struct ClayCustomData {
	CustomKind kind;
	void * payload;
};

// P5 — payload for CustomKind::BankButtonChrome. The bridge blits
// world.resources.spritebank[bank][index] at the bbox top-left, applying
// Renderer::EffectBrightness when brightness != 128 (matches the legacy
// Button effectbrightness path).
struct BankButtonChromePayload {
	Uint8  bank;        // 7 for the B156x21 sprite bank.
	Uint16 index;       // 24 = B156x21 idle face.
	Uint8  brightness;  // 128 = neutral; 136 = hovered (legacy ACTIVE state).
};

// P6 — payload for CustomKind::ToggleSprite. The bridge blits
// world.resources.spritebank[bank][index] at the bbox top-left. Mirrors
// the legacy Toggle render path: EffectColor applied first when non-zero,
// then EffectBrightness when != 128. Both steps allocate a per-call
// surface copy so the source sprite is not mutated.
struct TogglePayload {
	Uint8  bank;
	Uint16 index;
	Uint8  effectColor;  // 0 = no tint.
	Uint8  brightness;   // 128 = neutral.
};

}  // namespace silencer::clay_bridge

#endif
