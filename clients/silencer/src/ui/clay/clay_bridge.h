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
class Resources;
class Renderer;

namespace silencer::clay_bridge {

// Lazily allocates Clay's arena and registers the bank-text MeasureText
// callback on first call. Subsequent calls only update layout dimensions.
// Safe to call from any per-frame Tick before BeginLayout.
void EnsureInitialized(int width, int height);

// Walks `cmds` and dispatches each command to the matching Renderer
// primitive, writing into `dst`. Resources supplies sprite-bank lookups
// (IMAGE / CUSTOM dispatch); Renderer supplies the actual draw routines
// (DrawText / EffectBrightness / etc.). Game-free entry point — used by
// the standalone clay-demo tool. Inside the silencer binary, callers
// typically use the `Render(Game&, ...)` thin wrapper below.
void Render(::Resources & resources, ::Renderer & renderer,
            Surface * dst, ::Clay_RenderCommandArray cmds);

// Convenience wrapper inside the silencer binary — pulls Resources +
// Renderer out of the Game and forwards. Identical behavior to the
// primary overload above.
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
	ScrollBar,         // P7: sprite-faced vertical scrollbar (3-slice track + thumb).
	TextInput,         // P9: bank-text field with optional blinking caret.
	BoxStroke,         // C0c: multi-tone halo stroke (concentric rings) for the Box primitive.
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

// P7 — payload for CustomKind::ScrollBar. The bridge renders the
// scrollbar over the CUSTOM element's bounding box: bank 7 idx 9 (default
// track) is 3-sliced — top 16 px and bottom 16 px cap the track, the
// middle band tiles the source's middle rows to fill `bbox.height`. The
// thumb (bank 7 idx 10 by default) is cropped from the top to fit between
// the caps; its travel within the track is `scrollPosition / scrollMax`
// (clamped). Mirrors `Renderer::renderer.cpp` lines 858-933.
struct ScrollBarPayload {
	Uint8  bank;
	Uint16 trackIndex;
	Uint16 thumbIndex;
	Uint16 scrollPosition;  // in same units as scrollMax.
	Uint16 scrollMax;       // 0 = thumb at top, no travel.
};

// P9 — payload for CustomKind::TextInput. The bridge calls
// Renderer::DrawText with the supplied text/bank/fontWidth and, when
// `showCaret` is true, draws a 1-px-wide vertical bar at
// (bbox.x + textLen*fontWidth, bbox.y - 1) with height `caretHeight` and
// palette index `caretColor`. Mirrors `Renderer::DrawTextInput` in
// renderer.cpp:1835. Caller is responsible for password-masking the text
// (the primitive does this) and for resolving the blink phase + focus
// state into the single `showCaret` bool.
struct TextInputPayload {
	const char * text;        // NUL-terminated displayable text.
	Uint16       textLen;     // Pre-computed length (avoids strlen in bridge).
	Uint8        bank;        // 135 by default.
	Uint8        fontWidth;   // Cell width in px.
	Uint8        effectColor; // Text tint palette index. 0 = no tint.
	Uint8        brightness;  // Resolved by primitive (64 when inactive).
	Uint8        caretColor;  // Palette index for the caret bar.
	Uint8        caretHeight; // Pre-computed (height * 0.8).
	bool         showCaret;
};

// C0c — payload for CustomKind::BoxStroke. The bridge renders concentric
// inset rectangles around the box's bounding box: outer halo first (at the
// outer edge), then the primary stroke, then the inner halo. Each "ring"
// is `<width>` pixels thick (0 = skip that band). Total stroke thickness
// (and content inset) = outerHaloWidth + strokeWidth + innerHaloWidth.
// Mirrors the legacy lobby BG's bright-primary-bracketed-by-dark-greens
// stroke look without re-baking texture.
struct BoxStrokePayload {
	Uint8 strokeColor;
	Uint8 strokeWidth;
	Uint8 outerHaloColor;
	Uint8 outerHaloWidth;
	Uint8 innerHaloColor;
	Uint8 innerHaloWidth;
};

// P7 ScrollList primitive unit test. Renders a 30-item list scrolled to
// a fixed position (scrollPosition=3, selectedIndex=8) into a 640x480
// Surface and writes it to `outPath`. Invoked by the
// `clay_scroll_list_test` control op. Implementation in
// scroll_list_test.cpp.
bool RunScrollListTest(::Game & game, const char * outPath);

// P7 ScrollList click-routing check. Lays out a 30-item list and drives
// a press timeline over the bbox of row index 5 (visible because
// scrollPosition=3). Reports which row index's onSelect callback fired.
struct ScrollListCheckResult {
	int onSelectFired;      // Total number of onSelect invocations across all rows.
	int lastSelectedIndex;  // Index reported by the most recent onSelect call. -1 if none.
	// P7b — conditional-scrollbar emission. Two extra layout passes (no
	// overflow + overflow) count the number of CUSTOM ScrollBar render
	// commands emitted. Confirms the primitive auto-suppresses the
	// scrollbar when items fit AND emits exactly one when they don't.
	int noOverflowScrollbarCount;  // itemCount=3, visibleLines=10  → expect 0.
	int overflowScrollbarCount;    // itemCount=50, visibleLines=10 → expect 1.
	// Bbox geometry of the single scrollbar render command in the
	// overflow scenario. The post-blit visible track on the Surface
	// must equal this rect (i.e., the bridge applies the sprite-offset
	// compensation that makes visible-top-left == bbox.x/y).
	int overflowScrollbarBboxX;
	int overflowScrollbarBboxY;
	int overflowScrollbarBboxW;
	int overflowScrollbarBboxH;
};
bool RunScrollListCheck(::Game & game, ScrollListCheckResult & out);

// P8 ScrollTextBox primitive unit test. Renders a 6-line text box (no
// scrollbar, top-down origin) into a 640x480 Surface and writes it to
// `outPath`. Invoked by the `clay_scroll_text_box_test` control op.
// Implementation in scroll_text_box_test.cpp.
bool RunScrollTextBoxTest(::Game & game, const char * outPath);

// P8 ScrollTextBox auto-scroll helper check. Verifies the canonical
// "stay pinned to the bottom on append" computation across three
// scenarios. No PNG produced.
struct ScrollTextBoxCheckResult {
	Uint16 atBottom_prevPos;          // Expect 1: prev 5 lines (max=0), now 6 lines (max=1).
	Uint16 notAtBottom_prevPos;       // Expect 0: prev 10 lines pos=0 (not at max=5), now 11 lines — stays put.
	Uint16 atBottomOverflow_prevPos;  // Expect 6: prev 10 lines pos=5 (at max=5), now 11 lines (max=6).
};
bool RunScrollTextBoxCheck(::Game & game, ScrollTextBoxCheckResult & out);

// P9 TextInput primitive unit test. Renders one focused TextInput
// (text "Player1", caret visible, bank 135 fontWidth 9) into a 640x480
// Surface and writes it to `outPath`. Invoked by the
// `clay_text_input_test` control op. Implementation in
// text_input_test.cpp.
bool RunTextInputTest(::Game & game, const char * outPath);

// P9 TextInput dispatch-key check. Verifies that
// `TextInputDispatchKey(handle, '\n')` invokes onEnter exactly once and
// `TextInputDispatchKey(handle, 'x')` does not. No PNG produced.
struct TextInputCheckResult {
	int onEnterFiredForNewline;  // Expect 1.
	int onEnterFiredForLetter;   // Expect 0.
	int passwordMaskAppliedLen;  // Expect 8 — emitted text length for password variant.
};
bool RunTextInputCheck(::Game & game, TextInputCheckResult & out);

// P10 FormBorder primitive unit test. Renders a single 1-px-bordered box
// (color palette idx 220, sized 156x93) into a 640x480 Surface and writes
// it to `outPath`. Invoked by the `clay_form_border_test` control op.
// Implementation in form_border_test.cpp.
bool RunFormBorderTest(::Game & game, const char * outPath);

// P10 LabelValueRow primitive unit test. Renders three label/value rows
// stacked vertically (matching the legacy gamecreate form rhythm) into a
// 640x480 Surface and writes it to `outPath`. Invoked by the
// `clay_label_value_row_test` control op. Implementation in
// label_value_row_test.cpp.
bool RunLabelValueRowTest(::Game & game, const char * outPath);

// P10 Panel primitive unit test. Renders one of two variants
// (RightChrome with bank 7 idx 8 + title, or LeftBare with title only)
// into a 640x480 Surface and writes it to `outPath`. `variant` is one of
// "right" or "bare" — any other value selects "right". Invoked by the
// `clay_panel_test` control op. Implementation in panel_test.cpp.
bool RunPanelTest(::Game & game, const char * variant, const char * outPath);

// C0 Box primitive unit test. Renders four variants in a 2x2 grid
// (stroke-only, fill-only, fill+stroke, fill-with-opacity over an existing
// color) into a 640x480 Surface and writes it to `outPath`. Invoked by the
// `clay_box_test` control op. Implementation in box_test.cpp.
bool RunBoxTest(::Game & game, const char * outPath);

// C0c Box halo render test. Renders a single Box with the lobby's
// canonical halo params (primary stroke + outer + inner halos) against a
// flat black background into a 640x480 Surface and writes it to `outPath`.
// Invoked by the `clay_box_halo_test` control op. Implementation in
// box_halo_test.cpp.
bool RunBoxHaloTest(::Game & game, const char * outPath);

// C1 Box alpha-blend smoke test. Renders a fully-opaque rect with a
// 50%-opaque rect overlapping its right half into a 640x480 Surface and
// writes it to `outPath`. Exercises the bridge's palette alphaed-LUT path.
// Invoked by the `clay_rectangle_alpha_test` control op. Implementation in
// rectangle_alpha_test.cpp.
bool RunRectangleAlphaTest(::Game & game, const char * outPath);

}  // namespace silencer::clay_bridge

#endif
