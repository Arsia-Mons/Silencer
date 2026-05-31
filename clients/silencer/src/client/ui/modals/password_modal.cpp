#include "password_modal.h"

#include "client/ui/modals/password_modal_view.h"
#include "screen_context.h"
#include "game.h"

#include <cstring>

namespace password_modal_detail
{
constexpr const char * kActionPassword = "password_modal.password";
constexpr const char * kActionOk = "password_modal.ok";

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
	okClicked = false;
	password[0] = '\0';
}

void PasswordModal::Tick(ScreenContext & ctx)
{
	if(!okClicked) return;
	okClicked = false;
	std::string captured = password;
	auto cb = std::move(onSubmit);
	ctx.PopScreen();
	if(cb) cb(captured.c_str());
}

bool PasswordModal::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	(void)ctx;
	if(!out) return false;
	*out = silencer::client_ui::PasswordModalView(
		silencer::client_ui::PasswordModalViewProps{
			.key = "password-modal",
			.password = password,
			.on_password_change = [this](const std::string& value) {
				password_modal_detail::CopyUiText(password, static_cast<int>(sizeof(password)), value);
			},
			.on_submit = [this](const ::ui::ActivationEvent&) {
				okClicked = true;
			},
		});
	return true;
}

void PasswordModal::Destroy(ScreenContext & ctx)
{
	ctx.game.UiInteractions().ClearFocus();
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
		okClicked = true;
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Activate && action.id == password_modal_detail::kActionOk){
		okClicked = true;
		return true;
	}
	return false;
}
