#include "message_modal.h"

#include "client/ui/hooks/use_navigation.h"
#include "client/ui/modals/message_modal_frame.h"
#include "screen_context.h"
#include "surface.h"

#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"

#include <algorithm>

namespace message_modal_detail
{
	constexpr const char * kActionOk = "message_modal.ok";
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
	silencer::client_ui::use_navigation().pop_top();
	if(cb) cb();
}

void MessageModal::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)ctx;
	(void)frametime;
	const float uiScale = silencer::clay_bridge::UiScale();
	const int virtualW = std::max(1, static_cast<int>(dst.w / uiScale));
	const int virtualH = std::max(1, static_cast<int>(dst.h / uiScale));
	silencer::client_ui::MessageModalFrameProps props{
		.key = "message-modal",
		.message = message.c_str(),
		.show_ok = hasOk,
	};
	retainedFrame_.Build([&]() {
		                     return silencer::client_ui::MessageModalFrame(props);
	                     },
	                     virtualW,
	                     virtualH,
	                     interactions);
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

const ::ui::DrawCommandList * MessageModal::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
