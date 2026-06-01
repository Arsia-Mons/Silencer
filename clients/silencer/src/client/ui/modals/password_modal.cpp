#include "password_modal.h"

#include "client/ui/modals/password_modal_frame.h"
#include "client/ui/hooks/use_navigation.h"
#include "screen_context.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"

#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"

#include <algorithm>
#include <cstring>

namespace password_modal_detail
{
constexpr int kPasswordUid = 1;
constexpr const char * kActionPassword = "password_modal.password";

void CopyUiText(char * dst, int dstLen, const std::string & value)
{
	if(!dst || dstLen <= 0) return;
	int n = static_cast<int>(value.size());
	if(n > dstLen - 1) n = dstLen - 1;
	std::memcpy(dst, value.data(), n);
	dst[n] = '\0';
}

void RegisterWidgets(PasswordModal * modal,
                     char * buffer,
                     silencer::ui::UiInteractionRegistry& interactions)
{
	(void)modal;
	silencer::ui::UiInteractable input;
	input.id = kActionPassword;
	input.labelText = "Password";
	input.kind = silencer::ui::UiInteractableKind::TextInput;
	input.uid = kPasswordUid;
	input.value = buffer ? buffer : "";
	input.maxLength = 20;
	input.isPassword = true;
	interactions.RegisterInteractable(input);
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
	password_modal_detail::RegisterWidgets(this, password, ctx.game.UiInteractions());
	ctx.game.UiInteractions().FocusTextInputByUid(password_modal_detail::kPasswordUid);
}

void PasswordModal::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

void PasswordModal::Submit()
{
	std::string captured = password;
	auto cb = std::move(onSubmit);
	silencer::client_ui::use_navigation().pop_top();
	if(cb) cb(captured.c_str());
}

void PasswordModal::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	password_modal_detail::RegisterWidgets(this, password, interactions);
	if(!interactions.HasFocus()){
		interactions.FocusTextInputByUid(password_modal_detail::kPasswordUid);
	}
	bool focused = interactions.IsTextInputFocused(password_modal_detail::kPasswordUid);
	bool blink = (ctx.renderer.GetHudAnimationPhase() % 32) < 16;

	passwordDisplay_.assign(std::strlen(password), '*');
	if(focused && blink) passwordDisplay_ += "|";

	const float uiScale = silencer::clay_bridge::UiScale();
	const int virtualW = std::max(1, static_cast<int>(dst.w / uiScale));
	const int virtualH = std::max(1, static_cast<int>(dst.h / uiScale));
	silencer::client_ui::PasswordModalFrameProps props{
		.key = "password-modal",
		.password_display = passwordDisplay_.c_str(),
		.submit = [this]() {
			Submit();
		},
	};
	retainedFrame_.Build([&]() {
		                     return silencer::client_ui::PasswordModalFrame(props);
	                     },
	                     virtualW,
	                     virtualH,
	                     interactions);
	password_modal_detail::RegisterWidgets(this, password, interactions);
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
		Submit();
		return true;
	}
	return retainedFrame_.HandleUiIntent(action);
}

const ::ui::DrawCommandList * PasswordModal::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
