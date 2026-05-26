// Assertion probes for the Toggle primitive.
//
// `RunToggleCheck(out)` drives a click timeline against a row of three
// Toggles and reports which one's callback fired plus the brightness
// embedded in the selected vs unselected payloads (proves the callback
// routes by user pointer + the bridge sees the right brightness per
// state).

#include "clay_ui_tests/clay_ui_checks.h"
#include "clay_ui_compositor.h"
#include "clay/clay.h"
#include "primitives/toggle.h"
#include "runtime/UiInteractionRegistry.h"
#include "runtime/UiInputRouter.h"

namespace silencer::clay_bridge {

// Click-routing check. Lays out three toggles in a horizontal row. Only
// toggle 1 is "selected" (so its payload brightness should be 128, the
// other two should be 32). Presses inside toggle 1's bbox and verifies
// exactly its typed action is emitted.
//
// Timeline mirrors RunButtonCheck: 5 frames, hover -> press -> press
// dispatch -> held. The registry/router contract is exactly one action per
// press edge.
bool RunToggleCheck(::Game & game, ToggleCheckResult & out) {
	(void)game;
	constexpr int W = 640;
	constexpr int H = 480;
	EnsureInitialized(W, H);

	int counts[3] = {0, 0, 0};
	silencer::ui::UiInteractionRegistry interactions;

	// Per-frame: drives pointer state, lays out the row, returns the
	// (selectedBrightness, unselectedBrightness) seen in the emitted
	// CUSTOM payloads. Brightnesses are returned via out-params so the
	// caller only consults them on a "settled" frame.
	bool wasDown = false;
	auto runFrame = [&](float px, float py, bool down,
	                    int & selBr, int & unselBr) {
		const bool pressed = down && !wasDown;
		::Clay_SetPointerState(::Clay_Vector2{px, py}, down);
		::Clay_UpdateScrollContainers(false, ::Clay_Vector2{0, 0}, 0.0f);
		::Clay_ResetMeasureTextCache();
		interactions.BeginFrame();
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
				{ /*hoveredOut=*/nullptr,
				  /*actionId=*/"test.toggle.0",
				  /*interactions=*/&interactions });
			silencer::ui::primitives::Toggle(
				CLAY_STRING("t1"), 181, 1, /*selected=*/true,
				{ .width = 24, .height = 24, .effectColor = 112,
				  .selectedBrightness = 128, .unselectedBrightness = 32 },
				{ nullptr, "test.toggle.1", &interactions });
			silencer::ui::primitives::Toggle(
				CLAY_STRING("t2"), 181, 2, /*selected=*/false,
				{ .width = 24, .height = 24, .effectColor = 112,
				  .selectedBrightness = 128, .unselectedBrightness = 32 },
				{ nullptr, "test.toggle.2", &interactions });
		}
		::Clay_RenderCommandArray cmds = ::Clay_EndLayout();
		interactions.ResolveClayBoundsFromClay();
		silencer::ui::UiActionList actions;
		if(pressed){
			silencer::ui::UiInputState input;
			input.width = W;
			input.height = H;
			input.pointer.x = px;
			input.pointer.y = py;
			input.pointer.down = down;
			input.pointer.pressed = true;
			silencer::ui::UiInputRouter router(interactions);
			actions = router.Route(input);
		}else{
			actions = interactions.DrainActions();
		}
		for(const auto & action : actions){
			if(action.kind != silencer::ui::UiActionKind::Activate) continue;
			if(action.id == "test.toggle.0") counts[0]++;
			else if(action.id == "test.toggle.1") counts[1]++;
			else if(action.id == "test.toggle.2") counts[2]++;
		}
		wasDown = down;

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
