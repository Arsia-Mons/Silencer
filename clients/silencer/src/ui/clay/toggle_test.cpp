// P6 unit test scene for the Toggle primitive.
//
// `RunToggleTest(state, outPath)` renders ONE Toggle (bank 181 idx 0 —
// the legacy noxis agency icon) with the requested selected state into a
// fresh 640x480 Surface and writes a PNG. Two committed references under
// tests/lobby-clay/toggle_test/ are the pinned baselines.
//
// `RunToggleCheck(out)` drives a click timeline against a row of three
// Toggles and reports which one's callback fired plus the brightness
// embedded in the selected vs unselected payloads (proves the callback
// routes by user pointer + the bridge sees the right brightness per
// state).
//
// Invoked via the JSON-lines control ops `clay_toggle_test` and
// `clay_toggle_check` — see HandleImmediate in net/controldispatch.cpp.

#include "clay_bridge.h"
#include "clay/clay.h"
#include "primitives/toggle.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

#include <cstring>

namespace silencer::clay_bridge {

namespace {

bool ParseSelected(const char * v) {
	return v && std::strcmp(v, "selected") == 0;
}

}  // namespace

bool RunToggleTest(::Game & game, const char * stateName, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);
	silencer::ui::primitives::ToggleBeginFrame();

	const bool selected = ParseSelected(stateName);

	::Clay_BeginLayout();

	// Root: full-screen container positioning the toggle at (20, 90) via
	// padding — the legacy noxis-agency coords from character_panel.cpp:88-90.
	// Bank 181 idx 0 is the noxis icon; effectColor=112 matches the legacy
	// agency-toggle render path. Selected → brightness 128 (full); unselected
	// → brightness 32 (dimmed).
	CLAY({ .id = CLAY_ID("ToggleTestRoot"),
	       .layout = {
	           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { 20, 0, 90, 0 },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		silencer::ui::primitives::Toggle(
			CLAY_STRING("noxis"),
			/*spriteBank=*/181,
			/*spriteIndex=*/0,
			selected,
			{ .width = 24,
			  .height = 24,
			  .effectColor = 112,
			  .selectedBrightness = 128,
			  .unselectedBrightness = 32 });
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

// Click-routing check. Lays out three toggles in a horizontal row. Only
// toggle 1 is "selected" (so its payload brightness should be 128, the
// other two should be 32). Presses inside toggle 1's bbox and verifies
// exactly its onClick fires.
//
// Timeline mirrors RunBankButtonCheck: 5 frames, hover → press → press
// dispatch → held. The dispatch frame is where Clay_OnHover fires the
// proxy (Clay 0.14 invokes the registered hover callbacks during
// Clay_SetPointerState using the PREVIOUS frame's pointer state).
bool RunToggleCheck(::Game & game, ToggleCheckResult & out) {
	constexpr int W = 640;
	constexpr int H = 480;
	EnsureInitialized(W, H);

	int counts[3] = {0, 0, 0};
	auto onClick = +[](void * user) {
		(*static_cast<int *>(user))++;
	};

	// Per-frame: drives pointer state, lays out the row, returns the
	// (selectedBrightness, unselectedBrightness) seen in the emitted
	// CUSTOM payloads. Brightnesses are returned via out-params so the
	// caller only consults them on a "settled" frame.
	auto runFrame = [&](float px, float py, bool down,
	                    int & selBr, int & unselBr) {
		::Clay_SetPointerState(::Clay_Vector2{px, py}, down);
		::Clay_UpdateScrollContainers(false, ::Clay_Vector2{0, 0}, 0.0f);
		::Clay_ResetMeasureTextCache();
		silencer::ui::primitives::ToggleBeginFrame();

		::Clay_BeginLayout();
		CLAY({ .id = CLAY_ID("ToggleCheckRoot"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
		           .padding = { 100, 0, 100, 0 },
		           .childGap = 18,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			silencer::ui::primitives::Toggle(
				CLAY_STRING("t0"), 181, 0, /*selected=*/false,
				{ .width = 24, .height = 24, .effectColor = 112,
				  .selectedBrightness = 128, .unselectedBrightness = 32 },
				{ /*hoveredOut=*/nullptr, onClick, &counts[0] });
			silencer::ui::primitives::Toggle(
				CLAY_STRING("t1"), 181, 1, /*selected=*/true,
				{ .width = 24, .height = 24, .effectColor = 112,
				  .selectedBrightness = 128, .unselectedBrightness = 32 },
				{ nullptr, onClick, &counts[1] });
			silencer::ui::primitives::Toggle(
				CLAY_STRING("t2"), 181, 2, /*selected=*/false,
				{ .width = 24, .height = 24, .effectColor = 112,
				  .selectedBrightness = 128, .unselectedBrightness = 32 },
				{ nullptr, onClick, &counts[2] });
		}
		::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

		selBr = 0;
		unselBr = 0;
		for(int i = 0; i < cmds.length; i++){
			::Clay_RenderCommand * c = &cmds.internalArray[i];
			if(c->commandType != CLAY_RENDER_COMMAND_TYPE_CUSTOM) continue;
			const auto * ccd = reinterpret_cast<const ClayCustomData *>(
				c->renderData.custom.customData);
			if(!ccd || ccd->kind != CustomKind::ToggleSprite) continue;
			const auto * p = reinterpret_cast<const TogglePayload *>(ccd->payload);
			if(!p) continue;
			if(p->brightness == 128 && selBr == 0) selBr = 128;
			else if(p->brightness == 32 && unselBr == 0) unselBr = 32;
		}
	};

	int selBr = 0, unselBr = 0;
	// Frame 1: warm hit-test cache, no pointer.
	runFrame(-1.0f, -1.0f, false, selBr, unselBr);

	// Aim at the middle toggle. The row's first toggle starts at
	// (padding.left=100, padding.top=100); each toggle is 24 px wide;
	// childGap is 18. So toggle 1 spans x=[142, 166]; pick x=154, y=112.
	const float px = 154.0f;
	const float py = 112.0f;

	// Frame 2: hover only — also our "settled" frame for brightness probe.
	runFrame(px, py, false, selBr, unselBr);

	// Frames 3-4: press transition + dispatch.
	int dummySel, dummyUnsel;
	runFrame(px, py, true, dummySel, dummyUnsel);
	runFrame(px, py, true, dummySel, dummyUnsel);
	// Frame 5: still held — no additional fire.
	runFrame(px, py, true, dummySel, dummyUnsel);

	out.clicksToggle0 = counts[0];
	out.clicksToggle1 = counts[1];
	out.clicksToggle2 = counts[2];
	out.selectedBrightness = selBr;
	out.unselectedBrightness = unselBr;
	return true;
}

}  // namespace silencer::clay_bridge
