#ifndef SILENCER_RENDER_CLAY_UI_COMPOSITOR_H
#define SILENCER_RENDER_CLAY_UI_COMPOSITOR_H

// Clay-to-Silencer Surface compositor.
//
// Translates a Clay_RenderCommandArray into draw calls against the existing
// 8-bit paletted Surface + sprite-bank pipeline. Designed so screens never
// touch SDL or Surface directly: they emit Clay trees, this compositor dispatches
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
//   • IMAGE: imageConfig.imageData = PackImage*(). The bridge looks up
//     world.resources.spritebank[bank][index] and fits it into the bbox using
//     the packed cover / contain / stretch mode.
//   • SCISSOR_START/END: maintained as a clip-rect stack. Subsequent draws
//     are intersected with the top of the stack before being submitted.
//   • CUSTOM: customData = ClayCustomData* with a kind enum the bridge
//     switches on. Reserved for things that don't map cleanly to the other
//     command types (button chrome, scrollbar track, toggle face).

#include "clay/clay.h"
#include "clay_ui_payloads.h"
#include "shared.h"

class Game;
class Surface;
class Resources;
class Renderer;

namespace silencer::clay_bridge {

// Lazily allocates Clay's arena and registers the bank-text MeasureText
// callback on first call. Subsequent calls only update layout dimensions.
// Safe to call from any per-frame Tick before BeginLayout.
void EnsureInitialized(int width, int height);

// Integer magnification applied to bitmap glyph/sprite/chrome draws so a
// virtual-resolution Clay layout fills a larger native surface. Default 1
// (no magnification). Set once per frame before Render(); persists until
// changed. UiScale() exposes the current value to the dispatch paths.
void SetUiScale(int scale);
int  UiScale();

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

// Button primitive render test. Renders one variant+size combination into a
// fresh 640x480 Surface and writes it to `outPath`. Invoked by the
// `clay_button_test` control op. Implementation in button_test.cpp.
bool RunButtonTest(::Game & game,
                   const char * variant,
                   const char * size,
                   const char * label,
                   const char * outPath);

// Button hover/click parity check. Runs click + hover logic against a
// deterministic pointer-state timeline and reports raw counters plus a few
// sizing probes in the output struct. No PNG is produced.
struct ButtonCheckResult {
	int clicksFiredOnPress;     // Expect 1 action on the press edge.
	int clicksFiredWhenHeld;    // Expect 0 — held frames don't re-fire the proxy.
	int chromeBrightnessHover;  // Expect 136 — the CUSTOM payload's brightness when hovered.
	int chromeBrightnessIdle;   // Expect 128 — the CUSTOM payload's brightness when not hovered.
	int chromeSpriteIndexHover; // Expect 24 — Chrome keeps its legacy static face.
	int ovalHoverSpriteIndices[5];    // Expect 7,8,9,10,11 while pointer-hover activates.
	int ovalHoverBrightness[5];       // Expect 128,130,132,134,136 while activating.
	int ovalUnhoverSpriteIndices[5];  // Expect 11,10,9,8,7 while deactivating.
	int ovalUnhoverBrightness[5];     // Expect 136,134,132,130,128 while deactivating.
	int ovalFocusSpriteIndex;  // Expect 11 after keyboard focus reaches the button.
	int ovalFocusBrightness;   // Expect 136 after keyboard focus reaches the button.
	int ovalWallClockPartialSpriteIndex; // Expect 7 before one legacy tick has elapsed.
	int ovalWallClockPartialBrightness;  // Expect 128 before one legacy tick has elapsed.
	int ovalWallClockNextSpriteIndex;    // Expect 8 after the next legacy tick.
	int ovalWallClockNextBrightness;     // Expect 130 after the next legacy tick.
	int compactWidth;
	int compactHeight;
	int autoShortWidth;
	int autoLongWidth;
	int autoMultilineHeight;
};
bool RunButtonCheck(::Game & game, ButtonCheckResult & out);

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

// P7 ScrollList primitive unit test. Renders a 30-item list scrolled to
// a fixed position (scrollPosition=3, selectedIndex=8) into a 640x480
// Surface and writes it to `outPath`. Invoked by the
// `clay_scroll_list_test` control op. Implementation in
// scroll_list_test.cpp.
bool RunScrollListTest(::Game & game, const char * outPath);

// P7 ScrollList action-routing check. Lays out a 30-item list and drives
// a press timeline over the bbox of row index 5 (visible because
// scrollPosition=3). Reports which row index's Select action was emitted.
struct ScrollListCheckResult {
	int selectActions;      // Total number of Select actions across all rows.
	int lastSelectedIndex;  // Index reported by the most recent Select action. -1 if none.
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

// P9 TextInput action-drain check. Verifies that registry Enter queues a
// SubmitText action and normal text input does not.
// No PNG produced.
struct TextInputCheckResult {
	int submitActionsForEnter;  // Expect 1.
	int submitActionsForText;   // Expect 0.
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

// C0d Box vs legacy chat-box parity scene. Renders a single Box at the
// chat-box's visual stroke dimensions (378x260) inside a 6 px black
// margin (final 390x272). Output aligns byte-for-byte with the legacy
// crop produced by `/tmp/crop_lobby_chat_box.py` for the side-by-side
// DM. Halo params hard-coded from sampling the legacy stroke. Invoked
// by the `clay_box_parity_chat` control op. Implementation in
// box_parity_chat_test.cpp.
bool RunBoxParityChatTest(::Game & game, const char * outPath);

// C1 Box alpha-blend smoke test. Renders a fully-opaque rect with a
// 50%-opaque rect overlapping its right half into a 640x480 Surface and
// writes it to `outPath`. Exercises the bridge's palette alphaed-LUT path.
// Invoked by the `clay_rectangle_alpha_test` control op. Implementation in
// rectangle_alpha_test.cpp.
bool RunRectangleAlphaTest(::Game & game, const char * outPath);

}  // namespace silencer::clay_bridge

#endif
