#include "message_modal.h"

#include "client/ui/modals/message_modal_view.h"
#include "screen_context.h"

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

bool MessageModal::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	if(!out) return false;
	const silencer::client_ui::MessageModalContextValue context{
		.state = silencer::client_ui::MessageModalState{
			.message = message.c_str(),
			.show_ok = hasOk,
		},
		.actions = silencer::client_ui::MessageModalActions{
			.close = [this, screenContext = &ctx]() {
				Close(*screenContext);
			},
		},
	};
	const auto * stored = ::ui::copy_value(context);
	if(!stored) return false;
	*out = ::ui::component(
		"MessageModalView",
		silencer::client_ui::MessageModalViewProps{
			.key = "message-modal",
			.value = stored,
		},
		silencer::client_ui::MessageModalView);
	return true;
}

void MessageModal::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool MessageModal::HandleBack(ScreenContext & ctx)
{
	if(!hasOk) return false;
	Close(ctx);
	return true;
}

void MessageModal::SetText(ScreenContext & ctx, const std::string & text)
{
	(void)ctx;
	message = text;
}

void MessageModal::Close(ScreenContext & ctx)
{
	if(!hasOk) return;
	auto cb = std::move(onClose);
	ctx.PopScreen();
	if(cb) cb();
}
