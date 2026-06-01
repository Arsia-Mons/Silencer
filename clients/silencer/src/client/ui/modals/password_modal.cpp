#include "password_modal.h"

#include "client/ui/modals/password_modal_view.h"
#include "screen_context.h"
#include "game.h"

#include <cstring>

namespace password_modal_detail
{
constexpr const char * kActionPassword = "password_modal.password";

void CopyUiText(char * dst, int dstLen, const std::string & value)
{
	if(!dst || dstLen <= 0) return;
	int n = static_cast<int>(value.size());
	if(n > dstLen - 1) n = dstLen - 1;
	std::memcpy(dst, value.data(), n);
	dst[n] = '\0';
}
} // namespace password_modal_detail

PasswordModal::PasswordModal(std::function<void(const char *)> onSubmit_)
    : onSubmit(std::move(onSubmit_))
{
}

void PasswordModal::Build(ScreenContext & ctx)
{
	(void)ctx;
	password[0] = '\0';
}

void PasswordModal::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

bool PasswordModal::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	if(!out) return false;
	const silencer::client_ui::PasswordModalContextValue context{
		.state = silencer::client_ui::PasswordModalState{
			.password = password,
		},
		.actions = silencer::client_ui::PasswordModalActions{
			.set_password = [this](const std::string& value) {
				password_modal_detail::CopyUiText(password, static_cast<int>(sizeof(password)), value);
			},
			.submit = [this, screenContext = &ctx]() {
				Submit(*screenContext);
			},
		},
	};
	const auto * stored = ::ui::copy_value(context);
	if(!stored) return false;
	*out = ::ui::component(
		"PasswordModalView",
		silencer::client_ui::PasswordModalViewProps{
			.key = "password-modal",
			.value = stored,
		},
		silencer::client_ui::PasswordModalView);
	return true;
}

void PasswordModal::Destroy(ScreenContext & ctx)
{
	ctx.game.ClearUiFocus();
}

bool PasswordModal::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	(void)ctx;
	if(action.kind == silencer::ui::UiActionKind::SetText &&
	   action.id == password_modal_detail::kActionPassword){
		password_modal_detail::CopyUiText(password, static_cast<int>(sizeof(password)), action.value);
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::SubmitText && action.id == password_modal_detail::kActionPassword){
		password_modal_detail::CopyUiText(password, static_cast<int>(sizeof(password)), action.value);
		Submit(ctx);
		return true;
	}
	return false;
}

void PasswordModal::Submit(ScreenContext & ctx)
{
	std::string captured = password;
	auto cb = std::move(onSubmit);
	ctx.PopScreen();
	if(cb) cb(captured.c_str());
}
