#include "client/ui/modals/password_modal_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext PasswordModalContext = {};
const PasswordModalContextValue kEmptyPasswordModal = {};
}  // namespace

const PasswordModalContextValue& UsePasswordModal() {
	const auto * value = static_cast<const PasswordModalContextValue *>(
		::use_context(&PasswordModalContext));
	return value ? *value : kEmptyPasswordModal;
}

::ui::UiElement PasswordModalView(const PasswordModalViewProps& props) {
	const PasswordModalContextValue * stored = ::ui::copy_value(
		props.value ? *props.value : kEmptyPasswordModal);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"PasswordModalProvider",
		&PasswordModalContext,
		const_cast<PasswordModalContextValue *>(stored),
		::ui::children({
			::ui::component("PasswordModalFrame",
			                PasswordModalFrameProps{ .key = "frame" },
			                PasswordModalFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
