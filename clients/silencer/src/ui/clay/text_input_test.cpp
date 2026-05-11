// P9 unit test scenes for the TextInput primitive.
//
// `RunTextInputTest(outPath)` renders one focused TextInput (text
// "Player1", caret visible, bank 135 fontWidth 9, brightness 128) into
// a 640x480 Surface and writes a PNG. The committed reference at
// tests/lobby-clay/text_input_test/reference.png is the pinned baseline.
//
// `RunTextInputCheck(out)` verifies the screen-side dispatch helper:
//   • '\n' fires onEnter exactly once.
//   • 'x' (a non-routing character) does not fire onEnter.
//   • The password variant masks rendered glyphs (asserted via the
//     emitted CUSTOM payload's textLen).
//
// Invoked via the JSON-lines control ops `clay_text_input_test` and
// `clay_text_input_check`.

#include "clay_bridge.h"
#include "clay/clay.h"
#include "primitives/text_input.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

#include <cstring>

namespace silencer::clay_bridge {

namespace {

int g_enterCount = 0;

void OnEnter(void * /*user*/) { g_enterCount++; }

}  // namespace

bool RunTextInputTest(::Game & game, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);
	silencer::ui::primitives::TextInputBeginFrame();

	::Clay_BeginLayout();

	CLAY({ .id = CLAY_ID("TextInputTestRoot"),
	       .layout = {
	           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { 50, 0, 50, 0 },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		silencer::ui::primitives::TextInput(
			CLAY_STRING("name_input"),
			"Player1",
			{ .widthPx = 90,
			  .heightPx = 19,
			  .fontBank = 135,
			  .fontWidth = 9,
			  .showCaret = true });
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

bool RunTextInputCheck(::Game & game, TextInputCheckResult & out) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);

	using silencer::ui::primitives::TextInputDispatchKey;
	using silencer::ui::primitives::TextInputHandle;

	// Dispatch-key routing — verify '\n' fires onEnter, 'x' does not.
	g_enterCount = 0;
	TextInputHandle handle{};
	handle.onEnter = OnEnter;
	TextInputDispatchKey(handle, '\n');
	out.onEnterFiredForNewline = g_enterCount;

	g_enterCount = 0;
	TextInputDispatchKey(handle, 'x');
	out.onEnterFiredForLetter = g_enterCount;

	// Password masking — emit a password variant, run a layout pass,
	// and probe the CUSTOM payload for the masked text length.
	silencer::ui::primitives::TextInputBeginFrame();
	::Clay_BeginLayout();
	CLAY({ .id = CLAY_ID("TextInputCheckRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	       } }) {
		silencer::ui::primitives::TextInput(
			CLAY_STRING("pw_input"),
			"hunter12",
			{ .fontBank = 135, .fontWidth = 9, .password = true });
	}
	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	out.passwordMaskAppliedLen = 0;
	for(int i = 0; i < cmds.length; i++){
		::Clay_RenderCommand * c = &cmds.internalArray[i];
		if(c->commandType != CLAY_RENDER_COMMAND_TYPE_CUSTOM) continue;
		const auto * ccd = reinterpret_cast<const ClayCustomData *>(
			c->renderData.custom.customData);
		if(!ccd || ccd->kind != CustomKind::TextInput) continue;
		const auto * p = reinterpret_cast<const TextInputPayload *>(ccd->payload);
		if(!p || !p->text) continue;
		out.passwordMaskAppliedLen = static_cast<int>(p->textLen);
		// Verify the rendered chars are masked.
		for(int j = 0; j < p->textLen; j++){
			if(p->text[j] != '*'){
				out.passwordMaskAppliedLen = -1;
				break;
			}
		}
		break;
	}

	(void)game;
	return true;
}

}  // namespace silencer::clay_bridge
