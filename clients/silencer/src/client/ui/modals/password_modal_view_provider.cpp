#include "client/ui/modals/password_modal_view.h"

#include "ui/runtime/react.h"

namespace silencer {
namespace client_ui {

namespace {
::ReactContext PasswordModalContext = {};
const PasswordModalCredentials kEmptyPasswordModal = {};
}  // namespace

const PasswordModalCredentials& UsePasswordModalCredentials() {
	const auto * value = static_cast<const PasswordModalCredentials *>(
		::use_context(&PasswordModalContext));
	if(value) return *value;
	::react_report_error("client/ui/modal: missing PasswordModalProvider for UsePasswordModalCredentials\n");
	return kEmptyPasswordModal;
}

::ui::UiElement PasswordModalView(const PasswordModalViewProps& props) {
	const PasswordModalCredentials * stored = ::ui::copy_value(
		props.credentials ? *props.credentials : kEmptyPasswordModal);
	if(!stored){
		return ::ui::empty();
	}
	return ::ui::provider(
		"PasswordModalProvider",
		&PasswordModalContext,
		const_cast<PasswordModalCredentials *>(stored),
		::ui::children({
			::ui::component("PasswordModalFrame",
			                PasswordModalFrameProps{ .key = "frame" },
			                PasswordModalFrame),
		}),
		props.key);
}

}  // namespace client_ui
}  // namespace silencer
