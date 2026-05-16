// Render and interaction probes for the Button primitive.

#include "clay_ui_compositor.h"
#include "clay/clay.h"
#include "primitives/bank_text.h"
#include "primitives/button.h"
#include "runtime/UiInteractionRegistry.h"
#include "runtime/UiInputRouter.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

#include <cstring>
#include <vector>

namespace silencer::clay_bridge {

namespace {

using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;

ButtonVariant ParseVariant(const char * v) {
	if(v && std::strcmp(v, "oval") == 0) return ButtonVariant::Oval;
	if(v && std::strcmp(v, "text") == 0) return ButtonVariant::Text;
	if(v && std::strcmp(v, "ghost") == 0) return ButtonVariant::Ghost;
	return ButtonVariant::Chrome;
}

ButtonSize ParseSize(const char * s) {
	if(s && std::strcmp(s, "sm") == 0) return ButtonSize::Sm;
	if(s && std::strcmp(s, "lg") == 0) return ButtonSize::Lg;
	if(s && std::strcmp(s, "compact") == 0) return ButtonSize::Compact;
	if(s && std::strcmp(s, "auto") == 0) return ButtonSize::Auto;
	return ButtonSize::Md;
}

Clay_String LabelFor(const char * name) {
	if(name && std::strcmp(name, "short") == 0) return CLAY_STRING("OK");
	if(name && std::strcmp(name, "long") == 0) return CLAY_STRING("Connect To Lobby");
	if(name && std::strcmp(name, "multiline") == 0) return CLAY_STRING("Invite Friends\nTo Game");
	return CLAY_STRING("Create Game");
}

ButtonOpts OptsFor(ButtonVariant variant, ButtonSize size, const char * labelName) {
	ButtonOpts opts;
	opts.variant = variant;
	opts.size = size;
	if(size == ButtonSize::Auto){
		opts.paddingX = (variant == ButtonVariant::Oval) ? 24 : 0;
		opts.paddingY = (variant == ButtonVariant::Oval) ? 7 : 4;
		if(variant == ButtonVariant::Oval) opts.minWidth = 64;
	}
	if(labelName && std::strcmp(labelName, "multiline") == 0){
		opts.wrapText = true;
		opts.maxWidth = 180;
	}
	return opts;
}

const silencer::ui::UiInteractable * FindButton(
	silencer::ui::UiInteractionRegistry& interactions,
	const char * id) {
	return interactions.FindInteractableById(id);
}

}  // namespace

bool RunButtonTest(::Game & game,
                   const char * variantName,
                   const char * sizeName,
                   const char * labelName,
                   const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);
	silencer::ui::primitives::BankTextBeginFrame();
	silencer::ui::primitives::ButtonBeginFrame();

	const ButtonVariant variant = ParseVariant(variantName);
	const ButtonSize size = ParseSize(sizeName);
	const Clay_String label = LabelFor(labelName);
	ButtonOpts opts = OptsFor(variant, size, labelName);

	::Clay_BeginLayout();
	CLAY({ .id = CLAY_ID("ButtonTestRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { 242, 0, 68, 0 },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		Button(CLAY_STRING("ButtonTestSubject"), label, opts);
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

bool RunButtonCheck(::Game & game, ButtonCheckResult & out) {
	constexpr int W = 640;
	constexpr int H = 480;
	EnsureInitialized(W, H);

	int actionCount = 0;
	silencer::ui::UiInteractionRegistry interactions;

	bool wasDown = false;
	auto runOneFrame = [&](float px, float py, bool down) -> Uint8 {
		const bool pressed = down && !wasDown;
		::Clay_SetPointerState(::Clay_Vector2{px, py}, down);
		::Clay_UpdateScrollContainers(false, ::Clay_Vector2{0, 0}, 0.0f);
		::Clay_ResetMeasureTextCache();
		interactions.BeginFrame();
		silencer::ui::primitives::BankTextBeginFrame();
		silencer::ui::primitives::ButtonBeginFrame();

		::Clay_BeginLayout();
		CLAY({ .id = CLAY_ID("ButtonCheckRoot"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
		           .padding = { 100, 0, 100, 0 },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			Button(CLAY_STRING("ButtonCheckCreate"),
			       CLAY_STRING("Create Game"),
			       ButtonOpts{ .variant = ButtonVariant::Chrome,
			                   .size = ButtonSize::Compact },
			       ButtonHandle{ nullptr, "test.button.create", &interactions });
		}
		::Clay_RenderCommandArray cmds = ::Clay_EndLayout();
		interactions.ResolveClayBoundsFromClay();
		const auto * widget = FindButton(interactions, "test.button.create");
		if(widget){
			out.compactWidth = widget->w;
			out.compactHeight = widget->h;
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
			   action.id == "test.button.create"){
				actionCount++;
			}
		}
		wasDown = down;

		Uint8 brightness = 0;
		for(int i = 0; i < cmds.length; i++){
			::Clay_RenderCommand * c = &cmds.internalArray[i];
			if(c->commandType != CLAY_RENDER_COMMAND_TYPE_CUSTOM) continue;
			const auto * ccd = reinterpret_cast<const ClayCustomData *>(
				c->renderData.custom.customData);
			if(!ccd || ccd->kind != CustomKind::ButtonSprite) continue;
			const auto * p = reinterpret_cast<const ButtonSpritePayload *>(ccd->payload);
			if(p) brightness = p->brightness;
			break;
		}
		return brightness;
	};

	Uint8 idleBrightness = runOneFrame(-1.0f, -1.0f, false);
	const float px = 178.0f;
	const float py = 110.0f;
	Uint8 hoverBrightness = runOneFrame(px, py, false);
	int clicksBeforePress = actionCount;
	runOneFrame(px, py, true);
	runOneFrame(px, py, true);
	int clicksAfterPressWindow = actionCount;
	runOneFrame(px, py, true);
	int clicksAfterHeld = actionCount;

	auto probeButton = [&](const char * id,
	                       Clay_String label,
	                       ButtonOpts opts,
	                       int& w,
	                       int& h) {
		::Clay_SetPointerState(::Clay_Vector2{-1.0f, -1.0f}, false);
		::Clay_UpdateScrollContainers(false, ::Clay_Vector2{0, 0}, 0.0f);
		interactions.BeginFrame();
		silencer::ui::primitives::BankTextBeginFrame();
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

	int ovalMdLongWidth = 0;
	int ovalMdLongHeight = 0;
	int ovalMdLongTextRows = 0;
	bool ovalMdLongTextWithinBounds = false;
	::Clay_SetPointerState(::Clay_Vector2{-1.0f, -1.0f}, false);
	::Clay_UpdateScrollContainers(false, ::Clay_Vector2{0, 0}, 0.0f);
	interactions.BeginFrame();
	silencer::ui::primitives::BankTextBeginFrame();
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

	out.chromeBrightnessIdle = idleBrightness;
	out.chromeBrightnessHover = hoverBrightness;
	out.clicksFiredOnPress  = clicksAfterPressWindow - clicksBeforePress;
	out.clicksFiredWhenHeld = clicksAfterHeld - clicksAfterPressWindow;
	if(ovalMdLongWidth != 196 ||
	   ovalMdLongHeight != 33 ||
	   ovalMdLongTextRows != 1 ||
	   !ovalMdLongTextWithinBounds){
		return false;
	}
	return true;
}

}  // namespace silencer::clay_bridge
