#include "message_modal.h"

#include "client/ui/hooks/use_navigation.h"
#include "client/ui/modals/message_modal_frame.h"
#include "screen_context.h"

#include "runtime/UiInteractionRegistry.h"

#include <algorithm>

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
}

void MessageModal::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

void MessageModal::Close()
{
	auto cb = std::move(onClose);
	silencer::client_ui::use_navigation().pop_top();
	if(cb) cb();
}

void MessageModal::BuildUi(ScreenContext & ctx, float frametime, const silencer::ui::UiInputState& input, Uint8, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)ctx;
	(void)frametime;
	const float uiScale = input.uiScale;
	const int virtualW = std::max(1, input.width);
	const int virtualH = std::max(1, input.height);
	silencer::client_ui::MessageModalFrameProps props{
		.key = "message-modal",
		.message = message.c_str(),
		.show_ok = hasOk,
		.ok = [this]() {
			Close();
		},
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
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		Close();
		return true;
	}
	return retainedFrame_.HandleUiIntent(action);
}

void MessageModal::SetText(const std::string & text)
{
	message = text;
}

const ::ui::DrawCommandList * MessageModal::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
