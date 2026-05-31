#include "message_modal.h"

#include "client/ui/modals/message_modal_view.h"
#include "screen_context.h"

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
	ctx.PopScreen();
	if(cb) cb();
}

bool MessageModal::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	(void)ctx;
	if(!out) return false;
	*out = silencer::client_ui::MessageModalView(
		silencer::client_ui::MessageModalViewProps{
			.key = "message-modal",
			.message = message.c_str(),
			.show_ok = hasOk,
			.on_ok = [this](const ::ui::ActivationEvent&) {
				okClicked = true;
			},
		});
	return true;
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
