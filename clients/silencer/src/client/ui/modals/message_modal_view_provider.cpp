#include "client/ui/modals/message_modal_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext MessageModalContext = {};
const MessageModalDialog kEmptyMessageModal = {};
}  // namespace

const MessageModalDialog& UseMessageModalDialog() {
	const auto * value = static_cast<const MessageModalDialog *>(
		::use_context(&MessageModalContext));
	if(value) return *value;
	::react_report_error("client/ui/modal: missing MessageModalProvider for UseMessageModalDialog\n");
	return kEmptyMessageModal;
}

::ui::UiElement MessageModalView(const MessageModalViewProps& props) {
	const MessageModalDialog * stored = ::ui::copy_value(
		props.dialog ? *props.dialog : kEmptyMessageModal);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"MessageModalProvider",
		&MessageModalContext,
		const_cast<MessageModalDialog *>(stored),
		::ui::children({
			::ui::component("MessageModalFrame",
			                MessageModalFrameProps{ .key = "frame" },
			                MessageModalFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
