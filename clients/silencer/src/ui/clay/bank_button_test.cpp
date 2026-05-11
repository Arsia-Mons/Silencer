// P5 unit test scene for the BankButton primitive.
//
// Renders ONE variant (Chrome / Inline / Checkbox) into a fresh 640x480
// Surface and writes the result as PNG. Three committed references under
// tests/lobby-clay/bank_button_test/ are the pinned baselines; the run.sh
// harness pixdiffs each variant separately.
//
// Invoked via the JSON-lines control op `clay_bank_button_test` (immediate
// phase) — see HandleImmediate in net/controldispatch.cpp.

#include "clay_bridge.h"
#include "clay/clay.h"
#include "primitives/bank_button.h"
#include "primitives/bank_text.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

#include <cstring>

namespace silencer::clay_bridge {

namespace {

silencer::ui::primitives::BankButtonVariant ParseVariant(const char * v) {
	if(v && std::strcmp(v, "inline") == 0)
		return silencer::ui::primitives::BankButtonVariant::Inline;
	if(v && std::strcmp(v, "checkbox") == 0)
		return silencer::ui::primitives::BankButtonVariant::Checkbox;
	return silencer::ui::primitives::BankButtonVariant::Chrome;
}

}  // namespace

bool RunBankButtonTest(::Game & game, const char * variantName, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);
	silencer::ui::primitives::BankTextBeginFrame();
	silencer::ui::primitives::BankButtonBeginFrame();

	const auto variant = ParseVariant(variantName);

	::Clay_BeginLayout();

	// Root: full-screen container positioning its sole BankButton at
	// (242, 68) via padding — the lobby's Create-Game button coords from
	// game_select_panel.cpp:124. Each variant renders the same label
	// "Create Game" so identical text gives a clean signal that only the
	// chrome / hit-rect / brightness differs across variants.
	CLAY({ .id = CLAY_ID("BankButtonTestRoot"),
	       .layout = {
	           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { 242, 0, 68, 0 },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		silencer::ui::primitives::BankButton(
			CLAY_STRING("Create Game"),
			variant,
			{ /* default opts */ });
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

// Programmatic hover + click parity check. No PNG produced — this validates
// the behavioral half of the P5 pass_check (hover changes chrome brightness,
// click callback fires exactly once per press).
//
// Timeline (all on the Chrome variant):
//   Frame 1: pointer = (-1, -1), down=false → idle (warm hit-test cache).
//   Frame 2: pointer = inside button, down=false → hovered.
//   Frame 3: pointer = inside button, down=true  → press transition.
//   Frame 4: pointer = inside button, down=true  → press dispatched here.
//   Frame 5: pointer = inside button, down=true  → held; no additional fire.
//
// Why three "down" frames are needed for one click: Clay 0.14 invokes
// Clay_OnHover during Clay_SetPointerState using the *previous* frame's
// pointer state against the *previous* frame's element hashmap. So the
// PRESSED_THIS_FRAME state set at the end of frame 3 isn't read by the
// onHoverFunction dispatch until frame 4's SetPointerState call. After
// that, frame 5 reads PRESSED (steady held state) and does not re-fire.
//
// clicksFiredOnPress reports total clicks across frames 3-5 (the entire
// "button held down" window) — the API contract is "exactly once per
// press", not "fires on the same frame as the press input edge".
bool RunBankButtonCheck(::Game & game, BankButtonCheckResult & out) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);

	int clickCount = 0;
	auto onClick = +[](void * user) {
		(*static_cast<int *>(user))++;
	};

	// Helper: one layout pass with a fixed pointer state. Returns the
	// brightness embedded in the (only) BankButtonChrome custom payload, or
	// 0 if none was found.
	auto runOneFrame = [&](float px, float py, bool down) -> Uint8 {
		::Clay_SetPointerState(::Clay_Vector2{px, py}, down);
		::Clay_UpdateScrollContainers(false, ::Clay_Vector2{0, 0}, 0.0f);
		::Clay_ResetMeasureTextCache();
		silencer::ui::primitives::BankTextBeginFrame();
		silencer::ui::primitives::BankButtonBeginFrame();

		::Clay_BeginLayout();
		CLAY({ .id = CLAY_ID("BankButtonCheckRoot"),
		       .layout = {
		           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
		           .padding = { 100, 0, 100, 0 },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			silencer::ui::primitives::BankButton(
				CLAY_STRING("Create Game"),
				silencer::ui::primitives::BankButtonVariant::Chrome,
				{},
				{ /*hoveredOut=*/nullptr, /*onClick=*/onClick, /*user=*/&clickCount });
		}
		::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

		Uint8 brightness = 0;
		for(int i = 0; i < cmds.length; i++){
			::Clay_RenderCommand * c = &cmds.internalArray[i];
			if(c->commandType != CLAY_RENDER_COMMAND_TYPE_CUSTOM) continue;
			const auto * ccd = reinterpret_cast<const ClayCustomData *>(
				c->renderData.custom.customData);
			if(!ccd || ccd->kind != CustomKind::BankButtonChrome) continue;
			const auto * p = reinterpret_cast<const BankButtonChromePayload *>(ccd->payload);
			if(p) brightness = p->brightness;
			break;
		}
		return brightness;
	};

	// Frame 1: warm the hit-test cache. brightness here is the idle reading.
	Uint8 idleBrightness = runOneFrame(-1.0f, -1.0f, false);

	// Button is at root padding (100, 100) sized 156x21 → center ≈ (178, 110).
	const float px = 178.0f;
	const float py = 110.0f;

	// Frame 2: hover only.
	Uint8 hoverBrightness = runOneFrame(px, py, false);
	int clicksBeforePress = clickCount;

	// Frames 3-4: press transition + dispatch frame.
	runOneFrame(px, py, true);
	runOneFrame(px, py, true);
	int clicksAfterPressWindow = clickCount;

	// Frame 5: still held — no additional fire.
	runOneFrame(px, py, true);
	int clicksAfterHeld = clickCount;

	out.chromeBrightnessIdle = idleBrightness;
	out.chromeBrightnessHover = hoverBrightness;
	out.clicksFiredOnPress  = clicksAfterPressWindow - clicksBeforePress;
	out.clicksFiredWhenHeld = clicksAfterHeld - clicksAfterPressWindow;
	return true;
}

}  // namespace silencer::clay_bridge
