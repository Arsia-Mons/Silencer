#include "message_modal.h"

#include "screen_context.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/bank_text.h"

#include <SDL3/SDL.h>

namespace message_modal_detail
{
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;

constexpr uint16_t kDialogW = 352;
constexpr uint16_t kDialogH = 178;
constexpr uint16_t kDialogPadX = 34;
constexpr uint16_t kDialogPadY = 44;
constexpr const char * kActionOk = "message_modal.ok";

Clay_String FromStd(const std::string & s)
{
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}
} // namespace message_modal_detail

MessageModal::MessageModal(std::string message_, std::function<void()> onClose_)
    : message(std::move(message_)), hasOk(true), onClose(std::move(onClose_))
{
}

MessageModal::MessageModal(std::string message_, bool ok, std::function<void()> onClose_)
    : message(std::move(message_)), hasOk(ok), onClose(std::move(onClose_))
{
}

std::unique_ptr<MessageModal> MessageModal::Progress(std::string message)
{
	return std::unique_ptr<MessageModal>(new MessageModal(std::move(message), false, nullptr));
}

void MessageModal::Build(ScreenContext & ctx)
{
	(void)ctx;
	okClicked = false;
}

void MessageModal::Tick(ScreenContext & ctx)
{
	if(!hasOk || !okClicked) return;
	okClicked = false;
	auto cb = std::move(onClose);
	ctx.PopScreen();
	if(cb) cb();
}

void MessageModal::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	(void)dst;
	using namespace silencer::clay_bridge;



	CLAY({ .id = CLAY_ID("MessageModalRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       } }) {
		CLAY({ .id = CLAY_ID("MessageModalDialog"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(message_modal_detail::kDialogW),
		                       CLAY_SIZING_FIXED(message_modal_detail::kDialogH) },
		           .padding = { message_modal_detail::kDialogPadX, message_modal_detail::kDialogPadX,
		                        message_modal_detail::kDialogPadY, message_modal_detail::kDialogPadY },
		           .childGap = 18,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImage(40, 4) } }) {
			message_modal_detail::BankText(message_modal_detail::FromStd(message), message_modal_detail::BankTextVariant::Heading, {});
			if(hasOk){
				message_modal_detail::Button(CLAY_STRING("MessageModalOkButton"), CLAY_STRING("OK"),
				           message_modal_detail::ButtonOpts{ .variant = message_modal_detail::ButtonVariant::Chrome,
				                                             .size = message_modal_detail::ButtonSize::Compact },
				           message_modal_detail::ButtonHandle{ nullptr, message_modal_detail::kActionOk, &interactions });
			}
		}
	}
}

void MessageModal::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool MessageModal::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	(void)ctx;
	if(!hasOk) return false;
	if(action.kind == silencer::ui::UiActionKind::Cancel ||
	   (action.kind == silencer::ui::UiActionKind::Activate && action.id == message_modal_detail::kActionOk)){
		okClicked = true;
		return true;
	}
	return false;
}

void MessageModal::SetText(ScreenContext & ctx, const std::string & text)
{
	(void)ctx;
	message = text;
}
