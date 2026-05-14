#include "message_modal.h"

#include "screen_context.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiAutomationRegistry.h"
#include "primitives/bank_button.h"
#include "primitives/bank_text.h"

#include <SDL3/SDL.h>

namespace
{
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonBeginFrame;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextBeginFrame;
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

void RegisterWidgets(MessageModal * modal, int surfaceW, int surfaceH, bool hasOk)
{
	(void)modal;
	if(!hasOk) return;
	const int dialogX = (surfaceW - kDialogW) / 2;
	const int dialogY = (surfaceH - kDialogH) / 2;
	silencer::ui::automation::Widget w;
	w.id = kActionOk;
	w.labelText = "OK";
	w.kind = silencer::ui::automation::WidgetKind::Button;
	w.x = dialogX + (kDialogW - 156) / 2;
	w.y = dialogY + kDialogH - kDialogPadY - 21;
	w.w = 156; w.h = 21;
	silencer::ui::automation::Register(w);
}
}

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
	const Surface& surface = ctx.game.GetScreenBuffer();
	RegisterWidgets(this, surface.w, surface.h, hasOk);
}

void MessageModal::Tick(ScreenContext & ctx)
{
	if(!hasOk || !okClicked) return;
	okClicked = false;
	auto cb = std::move(onClose);
	ctx.PopScreen();
	if(cb) cb();
}

void MessageModal::BuildUi(ScreenContext & ctx, Surface & dst, float frametime)
{
	(void)frametime;
	using namespace silencer::clay_bridge;



	CLAY({ .id = CLAY_ID("MessageModalRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED((float)dst.w),
	                       CLAY_SIZING_FIXED((float)dst.h) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       } }) {
		CLAY({ .id = CLAY_ID("MessageModalDialog"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kDialogW),
		                       CLAY_SIZING_FIXED(kDialogH) },
		           .padding = { kDialogPadX, kDialogPadX,
		                        kDialogPadY, kDialogPadY },
		           .childGap = 18,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImage(40, 4) } }) {
			BankText(FromStd(message), BankTextVariant::Heading, {});
			if(hasOk){
				BankButton(CLAY_STRING("OK"), BankButtonVariant::Chrome, {},
				           BankButtonHandle{ nullptr, kActionOk });
			}
		}
	}
	RegisterWidgets(this, dst.w, dst.h, hasOk);
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
	   (action.kind == silencer::ui::UiActionKind::Activate && action.id == kActionOk)){
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
