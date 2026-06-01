#include "client/ui/modals/message_modal_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext MessageModalContext = {};
const MessageModalContextValue kEmptyMessageModal = {};
}  // namespace

const MessageModalContextValue& UseMessageModal() {
	const auto * value = static_cast<const MessageModalContextValue *>(
		::use_context(&MessageModalContext));
	if(value) return *value;
	::react_report_error("client/ui/modal: missing MessageModalProvider for UseMessageModal\n");
	return kEmptyMessageModal;
}

::ui::UiElement MessageModalView(const MessageModalViewProps& props) {
	const MessageModalContextValue * stored = ::ui::copy_value(
		props.value ? *props.value : kEmptyMessageModal);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"MessageModalProvider",
		&MessageModalContext,
		const_cast<MessageModalContextValue *>(stored),
		::ui::children({
			::ui::component("MessageModalFrame",
			                MessageModalFrameProps{ .key = "frame" },
			                MessageModalFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
