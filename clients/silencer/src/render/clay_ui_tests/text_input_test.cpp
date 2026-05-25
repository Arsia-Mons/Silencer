// Assertion probes for the TextInput primitive.
//
// `RunTextInputCheck(out)` verifies the registry/action-drain path:
//   • Enter queues SubmitText exactly once.
//   • text input queues SetText but not SubmitText.
//   • The password variant masks rendered glyphs (asserted via the
//     emitted CUSTOM payload's textLen).
//   • A 14px body input centers uppercase text by default.

#include "clay_ui_tests/clay_ui_checks.h"
#include "clay_ui_compositor.h"
#include "clay/clay.h"
#include "primitives/text_input.h"
#include "runtime/UiInteractionRegistry.h"
#include "surface.h"

#include <algorithm>
#include <cstring>

namespace silencer::clay_bridge {

bool RunTextInputCheck(::Game & game, TextInputCheckResult & out) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);

	// Registry routing — verify submit queues a typed action and text input
	// does not submit.
	silencer::ui::UiInteractionRegistry registry;
	registry.BeginFrame();
	silencer::ui::UiInteractable widget;
	widget.id = "test.text_input.name";
	widget.labelText = "TextInputCheck";
	widget.kind = silencer::ui::UiInteractableKind::TextInput;
	widget.uid = 9;
	widget.maxLength = 15;
	registry.RegisterInteractable(widget);
	registry.FocusTextInputByUid(9);

	registry.SubmitFocusedText();
	int submitActions = 0;
	for(const auto & action : registry.DrainActions()){
		if(action.kind == silencer::ui::UiActionKind::SubmitText &&
		   action.id == "test.text_input.name"){
			submitActions++;
		}
	}
	out.submitActionsForEnter = submitActions;

	registry.DispatchTextInput('x');
	submitActions = 0;
	for(const auto & action : registry.DrainActions()){
		if(action.kind == silencer::ui::UiActionKind::SubmitText){
			submitActions++;
		}
	}
	out.submitActionsForText = submitActions;

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
			{ .textSize = silencer::ui::primitives::TextSize::FieldLarge,
			  .password = true });
	}
	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	out.passwordMaskAppliedLen = 0;
	out.overflowTailAppliedLen = 0;
	out.overflowTailMatches = 0;
	out.body14TopMargin = -1;
	out.body14BottomMargin = -1;
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

	// Vertical metrics — a 14px body input should center uppercase input
	// text by default. The check renders through the real compositor so it
	// catches regressions in field text placement, not just payload shape.
	silencer::ui::primitives::TextInputBeginFrame();
	::Clay_BeginLayout();
	CLAY({ .id = CLAY_ID("TextInputVerticalRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	       } }) {
		silencer::ui::primitives::TextInput(
			CLAY_STRING("vertical_input"),
			"WWYYYYYY",
			{ .widthPx = 80,
			  .heightPx = 14,
			  .textSize = silencer::ui::primitives::TextSize::Body });
	}
	cmds = ::Clay_EndLayout();

	Surface verticalSurface(W, H, 0);
	Render(game, &verticalSurface, cmds);
	int minY = H;
	int maxY = -1;
	for(int y = 0; y < 14; y++){
		for(int x = 0; x < 80; x++){
			if(verticalSurface.pixels[static_cast<size_t>(y) * W + x] == 0){
				continue;
			}
			minY = std::min(minY, y);
			maxY = std::max(maxY, y);
		}
	}
	if(maxY >= minY){
		out.body14TopMargin = minY;
		out.body14BottomMargin = 13 - maxY;
	}

	// Overflow tailing — a narrow field should keep the rightmost visible
	// chars instead of rendering from column 0 and spilling to the right.
	silencer::ui::primitives::TextInputBeginFrame();
	::Clay_BeginLayout();
	CLAY({ .id = CLAY_ID("TextInputOverflowRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	       } }) {
		silencer::ui::primitives::TextInput(
			CLAY_STRING("overflow_input"),
			"abcdefghijklmnopqrstuvwxyz",
			{ .widthPx = 24,
			  .heightPx = 14,
			  .textSize = silencer::ui::primitives::TextSize::Body });
	}
	cmds = ::Clay_EndLayout();

	for(int i = 0; i < cmds.length; i++){
		::Clay_RenderCommand * c = &cmds.internalArray[i];
		if(c->commandType != CLAY_RENDER_COMMAND_TYPE_CUSTOM) continue;
		const auto * ccd = reinterpret_cast<const ClayCustomData *>(
			c->renderData.custom.customData);
		if(!ccd || ccd->kind != CustomKind::TextInput) continue;
		const auto * p = reinterpret_cast<const TextInputPayload *>(ccd->payload);
		if(!p || !p->text) continue;
		out.overflowTailAppliedLen = static_cast<int>(p->textLen);
		out.overflowTailMatches =
			(out.overflowTailAppliedLen == 4
			 && std::strncmp(p->text, "wxyz", 4) == 0)
				? 1
				: 0;
		break;
	}

	(void)game;
	return true;
}

}  // namespace silencer::clay_bridge
