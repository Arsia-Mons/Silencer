// Render and interaction probes for the Button primitive.

#include "clay_ui_tests/clay_ui_checks.h"
#include "clay_ui_compositor.h"
#include "clay/clay.h"
#include "primitives/text.h"
#include "primitives/button.h"
#include "runtime/UiInteractionRegistry.h"
#include "runtime/UiInputRouter.h"

#include "game.h"

#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

namespace silencer::clay_bridge {

namespace {

using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;

const silencer::ui::UiInteractable * FindButton(
	silencer::ui::UiInteractionRegistry& interactions,
	const char * id) {
	return interactions.FindInteractableById(id);
}

struct ButtonProbe {
	Uint16 spriteIndex = 0;
	Uint8 brightness = 0;
};

ButtonProbe FirstButtonProbe(::Clay_RenderCommandArray cmds) {
	ButtonProbe out;
	for(int i = 0; i < cmds.length; i++){
		::Clay_RenderCommand * c = &cmds.internalArray[i];
		if(c->commandType != CLAY_RENDER_COMMAND_TYPE_CUSTOM) continue;
		const auto * ccd = reinterpret_cast<const ClayCustomData *>(
			c->renderData.custom.customData);
		if(!ccd) continue;
		if(ccd->kind == CustomKind::ButtonSprite){
			const auto * p = reinterpret_cast<const ButtonSpritePayload *>(ccd->payload);
			if(p){
				out.spriteIndex = p->index;
				out.brightness = p->brightness;
			}
			break;
		}
		if(ccd->kind == CustomKind::ButtonNineSlice){
			const auto * p = reinterpret_cast<const ButtonNineSlicePayload *>(ccd->payload);
			if(p){
				out.spriteIndex = p->index;
				out.brightness = p->brightness;
			}
			break;
		}
	}
	return out;
}

}  // namespace

bool RunButtonCheck(::Game & game, ButtonCheckResult & out) {
	constexpr int W = 640;
	constexpr int H = 480;
	constexpr float kVisualStepSeconds = 1.0f / 24.0f;
	SetTextMeasureResources(&game.GetWorld().resources);
	EnsureInitialized(W, H);

	int actionCount = 0;
	const std::string chromeClayId = "ButtonCheckCreate";
	const std::string chromeActionId = "test.button.create";
	ButtonOpts chromeOpts{ .variant = ButtonVariant::Chrome,
	                       .size = ButtonSize::Compact };

	auto runOneFrame = [&](silencer::ui::UiInteractionRegistry& interactions,
	                       bool& wasDown,
	                       const std::string& clayId,
	                       const std::string& actionId,
	                       ButtonOpts opts,
	                       float px,
	                       float py,
	                       bool down,
	                       int * widgetWidth = nullptr,
	                       int * widgetHeight = nullptr,
	                       float animationDeltaSeconds = 1.0f / 24.0f) -> ButtonProbe {
		const bool pressed = down && !wasDown;
		::Clay_SetPointerState(::Clay_Vector2{px, py}, down);
		::Clay_UpdateScrollContainers(false, ::Clay_Vector2{0, 0}, 0.0f);
		::Clay_ResetMeasureTextCache();
		interactions.BeginFrame();
		silencer::ui::primitives::TextBeginFrame();
		silencer::ui::primitives::ButtonBeginFrame(animationDeltaSeconds,
		                                           kVisualStepSeconds);

		::Clay_BeginLayout();
		CLAY({ .id = CLAY_ID("ButtonCheckRoot"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
		           .padding = { 100, 0, 100, 0 },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			Button(Clay_String{ false, static_cast<int32_t>(clayId.size()), clayId.c_str() },
			       CLAY_STRING("Create Game"),
			       opts,
			       ButtonHandle{ nullptr, actionId.c_str(), &interactions });
		}
		::Clay_RenderCommandArray cmds = ::Clay_EndLayout();
		interactions.ResolveClayBoundsFromClay();
		const auto * widget = FindButton(interactions, actionId.c_str());
		if(widget){
			if(widgetWidth) *widgetWidth = widget->w;
			if(widgetHeight) *widgetHeight = widget->h;
		}

		std::vector<silencer::ui::UiAction> actions;
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
			if(action.kind == silencer::ui::UiActionKind::Activate &&
			   action.id == chromeActionId){
				actionCount++;
			}
		}
		wasDown = down;

		return FirstButtonProbe(cmds);
	};

	silencer::ui::UiInteractionRegistry interactions;
	bool wasDown = false;
	ButtonProbe idleProbe = runOneFrame(interactions,
	                                    wasDown,
	                                    chromeClayId,
	                                    chromeActionId,
	                                    chromeOpts,
	                                    -1.0f,
	                                    -1.0f,
	                                    false,
	                                    &out.compactWidth,
	                                    &out.compactHeight);
	const float px = 178.0f;
	const float py = 110.0f;
	ButtonProbe hoverProbe;
	for(int i = 0; i < 5; ++i){
		hoverProbe = runOneFrame(interactions,
		                         wasDown,
		                         chromeClayId,
		                         chromeActionId,
		                         chromeOpts,
		                         px,
		                         py,
		                         false);
	}
	int clicksBeforePress = actionCount;
	runOneFrame(interactions, wasDown, chromeClayId, chromeActionId, chromeOpts, px, py, true);
	runOneFrame(interactions, wasDown, chromeClayId, chromeActionId, chromeOpts, px, py, true);
	int clicksAfterPressWindow = actionCount;
	runOneFrame(interactions, wasDown, chromeClayId, chromeActionId, chromeOpts, px, py, true);
	int clicksAfterHeld = actionCount;

	const std::string ovalClayId = "ButtonCheckOvalHover";
	const std::string ovalActionId = "test.button.oval_hover";
	ButtonOpts ovalOpts{ .variant = ButtonVariant::Oval,
	                     .size = ButtonSize::Md };
	silencer::ui::UiInteractionRegistry ovalInteractions;
	bool ovalWasDown = false;
	runOneFrame(ovalInteractions,
	            ovalWasDown,
	            ovalClayId,
	            ovalActionId,
	            ovalOpts,
	            -1.0f,
	            -1.0f,
	            false);
	for(int i = 0; i < 5; ++i){
		ButtonProbe probe = runOneFrame(ovalInteractions,
		                                ovalWasDown,
		                                ovalClayId,
		                                ovalActionId,
		                                ovalOpts,
		                                px,
		                                py,
		                                false);
		out.ovalHoverSpriteIndices[i] = probe.spriteIndex;
		out.ovalHoverBrightness[i] = probe.brightness;
	}
	for(int i = 0; i < 5; ++i){
		ButtonProbe probe = runOneFrame(ovalInteractions,
		                                ovalWasDown,
		                                ovalClayId,
		                                ovalActionId,
		                                ovalOpts,
		                                -1.0f,
		                                -1.0f,
		                                false);
		out.ovalUnhoverSpriteIndices[i] = probe.spriteIndex;
		out.ovalUnhoverBrightness[i] = probe.brightness;
	}

	const std::string focusClayId = "ButtonCheckOvalFocus";
	const std::string focusActionId = "test.button.oval_focus";
	silencer::ui::UiInteractionRegistry focusInteractions;
	bool focusWasDown = false;
	runOneFrame(focusInteractions,
	            focusWasDown,
	            focusClayId,
	            focusActionId,
	            ovalOpts,
	            -1.0f,
	            -1.0f,
	            false);
	silencer::ui::UiInputState focusInput;
	focusInput.width = W;
	focusInput.height = H;
	focusInput.navActions.push_back(silencer::ui::UiNavAction::FocusNext);
	silencer::ui::UiInputRouter focusRouter(focusInteractions);
	focusRouter.Route(focusInput);
	ButtonProbe focusProbe;
	for(int i = 0; i < 5; ++i){
		focusProbe = runOneFrame(focusInteractions,
		                         focusWasDown,
		                         focusClayId,
		                         focusActionId,
		                         ovalOpts,
		                         -1.0f,
		                         -1.0f,
		                         false);
	}

	const std::string wallClockClayId = "ButtonCheckOvalWallClock";
	const std::string wallClockActionId = "test.button.oval_wall_clock";
	silencer::ui::UiInteractionRegistry wallClockInteractions;
	bool wallClockWasDown = false;
	runOneFrame(wallClockInteractions,
	            wallClockWasDown,
	            wallClockClayId,
	            wallClockActionId,
	            ovalOpts,
	            -1.0f,
	            -1.0f,
	            false);
	ButtonProbe partialClockProbe = runOneFrame(wallClockInteractions,
	                                           wallClockWasDown,
	                                           wallClockClayId,
	                                           wallClockActionId,
	                                           ovalOpts,
	                                           px,
	                                           py,
	                                           false,
	                                           nullptr,
	                                           nullptr,
	                                           kVisualStepSeconds * 0.5f);
	runOneFrame(wallClockInteractions,
	            wallClockWasDown,
	            wallClockClayId,
	            wallClockActionId,
	            ovalOpts,
	            px,
	            py,
	            false,
	            nullptr,
	            nullptr,
	            kVisualStepSeconds * 0.5f);
	ButtonProbe nextClockProbe = runOneFrame(wallClockInteractions,
	                                        wallClockWasDown,
	                                        wallClockClayId,
	                                        wallClockActionId,
	                                        ovalOpts,
	                                        px,
	                                        py,
	                                        false,
	                                        nullptr,
	                                        nullptr,
	                                        kVisualStepSeconds);

	const std::string legacySelectedClayId = "ButtonCheckLegacySelectedSuppressed";
	const std::string legacySelectedActionId = "test.button.legacy_selected";
	ButtonOpts legacySelectedOpts{ .variant = ButtonVariant::LegacyRow,
	                               .size = ButtonSize::Auto,
	                               .selected = true,
	                               .selectedVisual = false };
	silencer::ui::UiInteractionRegistry legacySelectedInteractions;
	bool legacySelectedWasDown = false;
	ButtonProbe legacySelectedProbe;
	for(int i = 0; i < 5; ++i){
		legacySelectedProbe = runOneFrame(legacySelectedInteractions,
		                                  legacySelectedWasDown,
		                                  legacySelectedClayId,
		                                  legacySelectedActionId,
		                                  legacySelectedOpts,
		                                  -1.0f,
		                                  -1.0f,
		                                  false);
	}
	const auto * legacySelectedWidget =
		FindButton(legacySelectedInteractions, legacySelectedActionId.c_str());
	out.legacySelectedSuppressedSpriteIndex = legacySelectedProbe.spriteIndex;
	out.legacySelectedSuppressedBrightness = legacySelectedProbe.brightness;
	out.legacySelectedSuppressedMetadata =
		legacySelectedWidget && legacySelectedWidget->selected ? 1 : 0;

	auto probeButton = [&](const char * id,
	                       Clay_String label,
	                       ButtonOpts opts,
	                       int& w,
	                       int& h) {
		::Clay_SetPointerState(::Clay_Vector2{-1.0f, -1.0f}, false);
		::Clay_UpdateScrollContainers(false, ::Clay_Vector2{0, 0}, 0.0f);
		interactions.BeginFrame();
		silencer::ui::primitives::TextBeginFrame();
		silencer::ui::primitives::ButtonBeginFrame();
		::Clay_BeginLayout();
		CLAY({ .id = CLAY_ID("ButtonAutoProbeRoot"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
		           .padding = { 40, 0, 40, 0 },
		       } }) {
			Button(Clay_String{ false, static_cast<int32_t>(std::strlen(id)), id },
			       label,
			       opts,
			       ButtonHandle{ nullptr, id, &interactions });
		}
		::Clay_EndLayout();
		interactions.ResolveClayBoundsFromClay();
		const auto * widget = FindButton(interactions, id);
		if(widget){
			w = widget->w;
			h = widget->h;
		}
	};

	int autoShortH = 0;
	int autoLongH = 0;
	probeButton("test.button.auto_short",
	            CLAY_STRING("OK"),
	            ButtonOpts{ .variant = ButtonVariant::Oval,
	                        .size = ButtonSize::Auto,
	                        .minWidth = 64,
	                        .paddingX = 24,
	                        .paddingY = 7 },
	            out.autoShortWidth,
	            autoShortH);
	probeButton("test.button.auto_long",
	            CLAY_STRING("Connect To Lobby"),
	            ButtonOpts{ .variant = ButtonVariant::Oval,
	                        .size = ButtonSize::Auto,
	                        .minWidth = 64,
	                        .paddingX = 24,
	                        .paddingY = 7 },
	            out.autoLongWidth,
	            autoLongH);
	int multilineW = 0;
	probeButton("test.button.auto_multiline",
	            CLAY_STRING("Invite Friends\nTo Game"),
	            ButtonOpts{ .variant = ButtonVariant::Oval,
	                        .size = ButtonSize::Auto,
	                        .minWidth = 64,
	                        .maxWidth = 180,
	                        .paddingX = 24,
	                        .paddingY = 7,
	                        .wrapText = true },
	            multilineW,
	            out.autoMultilineHeight);
	probeButton("test.button.chrome_auto_wide",
	            CLAY_STRING("Create Game"),
	            ButtonOpts{ .variant = ButtonVariant::Chrome,
	                        .size = ButtonSize::Auto,
	                        .minWidth = 212,
	                        .maxWidth = 212 },
	            out.chromeAutoWidth,
	            out.chromeAutoHeight);

	auto probeTextCompactButton = [&](const char * id,
	                                  Clay_String label,
	                                  int& buttonW,
	                                  int& buttonH,
	                                  int& textXOffset,
	                                  int& textWidth,
	                                  int& textYOffset,
	                                  bool& textOffsetMatches) {
		int textRows = 0;
		::Clay_SetPointerState(::Clay_Vector2{-1.0f, -1.0f}, false);
		::Clay_UpdateScrollContainers(false, ::Clay_Vector2{0, 0}, 0.0f);
		interactions.BeginFrame();
		silencer::ui::primitives::TextBeginFrame();
		silencer::ui::primitives::ButtonBeginFrame();
		::Clay_BeginLayout();
		CLAY({ .id = CLAY_ID("ButtonTextCompactProbeRoot"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
		           .padding = { 40, 0, 40, 0 },
		       } }) {
			Button(Clay_String{ false, static_cast<int32_t>(std::strlen(id)), id },
			       label,
			       ButtonOpts{ .variant = ButtonVariant::Text,
			                   .size = ButtonSize::Compact },
			       ButtonHandle{ nullptr, id, &interactions });
		}
		::Clay_RenderCommandArray cmds = ::Clay_EndLayout();
		interactions.ResolveClayBoundsFromClay();
		const auto * widget = FindButton(interactions, id);
		if(widget){
			buttonW = widget->w;
			buttonH = widget->h;
			for(int i = 0; i < cmds.length; i++){
				::Clay_RenderCommand * c = &cmds.internalArray[i];
				if(c->commandType != CLAY_RENDER_COMMAND_TYPE_TEXT) continue;
				textRows++;
				textXOffset = static_cast<int>(c->boundingBox.x) - widget->x;
				textWidth = static_cast<int>(c->boundingBox.width);
				textYOffset = static_cast<int>(c->boundingBox.y) - widget->y;
			}
			const int textRightGap = widget->w - textXOffset - textWidth;
			textOffsetMatches =
				textRows == 1 &&
				std::abs(textXOffset - textRightGap) <= 1 &&
				textYOffset == 8;
		}
	};

	bool textCompactTextOffsetMatches = false;
	probeTextCompactButton("test.button.text_compact",
	                       CLAY_STRING("Login"),
	                       out.textCompactWidth,
	                       out.textCompactHeight,
	                       out.textCompactTextXOffset,
	                       out.textCompactTextWidth,
	                       out.textCompactTextYOffset,
	                       textCompactTextOffsetMatches);

	int textCompactLongWidth = 0;
	int textCompactLongHeight = 0;
	int textCompactLongXOffset = 0;
	int textCompactLongTextWidth = 0;
	int textCompactLongYOffset = 0;
	bool textCompactLongTextOffsetMatches = false;
	probeTextCompactButton("test.button.text_compact_long",
	                       CLAY_STRING("Login/Create"),
	                       textCompactLongWidth,
	                       textCompactLongHeight,
	                       textCompactLongXOffset,
	                       textCompactLongTextWidth,
	                       textCompactLongYOffset,
	                       textCompactLongTextOffsetMatches);

	int ovalMdLongWidth = 0;
	int ovalMdLongHeight = 0;
	int ovalMdLongTextRows = 0;
	bool ovalMdLongTextWithinBounds = false;
	::Clay_SetPointerState(::Clay_Vector2{-1.0f, -1.0f}, false);
	::Clay_UpdateScrollContainers(false, ::Clay_Vector2{0, 0}, 0.0f);
	interactions.BeginFrame();
	silencer::ui::primitives::TextBeginFrame();
	silencer::ui::primitives::ButtonBeginFrame();
	::Clay_BeginLayout();
	CLAY({ .id = CLAY_ID("ButtonFixedProbeRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { 40, 0, 40, 0 },
	       } }) {
		Button(CLAY_STRING("test.button.oval_md_long"),
		       CLAY_STRING("Connect To Lobby"),
		       ButtonOpts{ .variant = ButtonVariant::Oval,
		                   .size = ButtonSize::Md },
		       ButtonHandle{ nullptr, "test.button.oval_md_long", &interactions });
	}
	::Clay_RenderCommandArray fixedCmds = ::Clay_EndLayout();
	interactions.ResolveClayBoundsFromClay();
	const auto * fixedWidget = FindButton(interactions, "test.button.oval_md_long");
	if(fixedWidget){
		ovalMdLongWidth = fixedWidget->w;
		ovalMdLongHeight = fixedWidget->h;
		ovalMdLongTextWithinBounds = true;
		for(int i = 0; i < fixedCmds.length; i++){
			::Clay_RenderCommand * c = &fixedCmds.internalArray[i];
			if(c->commandType != CLAY_RENDER_COMMAND_TYPE_TEXT) continue;
			ovalMdLongTextRows++;
			const ::Clay_BoundingBox& bb = c->boundingBox;
			if(bb.x < fixedWidget->x ||
			   bb.y < fixedWidget->y ||
			   bb.x + bb.width > fixedWidget->x + fixedWidget->w ||
			   bb.y + bb.height > fixedWidget->y + fixedWidget->h){
				ovalMdLongTextWithinBounds = false;
			}
		}
	}

	out.chromeBrightnessIdle = idleProbe.brightness;
	out.chromeBrightnessHover = hoverProbe.brightness;
	out.chromeSpriteIndexHover = hoverProbe.spriteIndex;
	out.clicksFiredOnPress  = clicksAfterPressWindow - clicksBeforePress;
	out.clicksFiredWhenHeld = clicksAfterHeld - clicksAfterPressWindow;
	out.ovalFocusSpriteIndex = focusProbe.spriteIndex;
	out.ovalFocusBrightness = focusProbe.brightness;
	out.ovalWallClockPartialSpriteIndex = partialClockProbe.spriteIndex;
	out.ovalWallClockPartialBrightness = partialClockProbe.brightness;
	out.ovalWallClockNextSpriteIndex = nextClockProbe.spriteIndex;
	out.ovalWallClockNextBrightness = nextClockProbe.brightness;

	const int expectHoverIndices[5] = {7, 8, 9, 10, 11};
	const int expectHoverBrightness[5] = {128, 130, 132, 134, 136};
	const int expectUnhoverIndices[5] = {11, 10, 9, 8, 7};
	const int expectUnhoverBrightness[5] = {136, 134, 132, 130, 128};
	for(int i = 0; i < 5; ++i){
		if(out.ovalHoverSpriteIndices[i] != expectHoverIndices[i] ||
		   out.ovalHoverBrightness[i] != expectHoverBrightness[i] ||
		   out.ovalUnhoverSpriteIndices[i] != expectUnhoverIndices[i] ||
		   out.ovalUnhoverBrightness[i] != expectUnhoverBrightness[i]){
			return false;
		}
	}
	if(out.chromeBrightnessIdle != 128 ||
	   out.chromeBrightnessHover != 136 ||
	   out.chromeSpriteIndexHover != 24 ||
	   out.ovalFocusSpriteIndex != 11 ||
	   out.ovalFocusBrightness != 136 ||
	   out.ovalWallClockPartialSpriteIndex != 7 ||
	   out.ovalWallClockPartialBrightness != 128 ||
	   out.ovalWallClockNextSpriteIndex != 8 ||
	   out.ovalWallClockNextBrightness != 130 ||
	   out.legacySelectedSuppressedSpriteIndex != 2 ||
	   out.legacySelectedSuppressedBrightness != 128 ||
	   out.legacySelectedSuppressedMetadata != 1){
		return false;
	}
	if(ovalMdLongWidth != 196 ||
	   ovalMdLongHeight != 33 ||
	   ovalMdLongTextRows != 1 ||
	   !ovalMdLongTextWithinBounds){
		return false;
	}
	if(out.textCompactWidth != 52 ||
	   out.textCompactHeight != 21 ||
	   textCompactLongWidth != 84 ||
	   textCompactLongHeight != 21 ||
	   !textCompactTextOffsetMatches ||
	   !textCompactLongTextOffsetMatches){
		return false;
	}
	return true;
}

}  // namespace silencer::clay_bridge
