#include "password_modal.h"

#include "screen_context.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/bank_button.h"
#include "primitives/bank_text.h"
#include "primitives/text_input.h"

#include <SDL3/SDL.h>

#include <cstring>

namespace password_modal_detail
{
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;

constexpr uint16_t kDialogW = 352;
constexpr uint16_t kDialogH = 148;
constexpr uint16_t kInputW = 180;
constexpr uint16_t kInputH = 14;
constexpr int kPasswordUid = 1;
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

// The TextInput primitive does not self-register an interactable (unlike
// BankButton), so the password field's registration must stay — it carries
// the uid/value/isPassword wiring used for focus, typing, submit and CLI
// inspect. (Absolute geometry is imperfect at uiScale > 1; a follow-up is
// to make TextInput self-register its Clay bounds like BankButton.)
void RegisterWidgets(PasswordModal * modal,
                     char * buffer,
                     int surfaceW,
                     int surfaceH,
                     silencer::ui::UiInteractionRegistry& interactions)
{
	(void)modal;
	const int dialogX = (surfaceW - kDialogW) / 2;
	const int dialogY = (surfaceH - kDialogH) / 2;
	silencer::ui::UiInteractable input;
	input.id = kActionPassword;
	input.labelText = "Password";
	input.kind = silencer::ui::UiInteractableKind::TextInput;
	input.uid = kPasswordUid;
	input.x = dialogX + (kDialogW - kInputW) / 2;
	input.y = dialogY + 64;
	input.w = kInputW;
	input.h = kInputH;
	input.value = buffer ? buffer : "";
	input.maxLength = 20;
	input.isPassword = true;
	interactions.RegisterInteractable(input);
	(void)surfaceH;
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
	const Surface& surface = ctx.game.GetScreenBuffer();
	password_modal_detail::RegisterWidgets(this, password, surface.w, surface.h, ctx.game.UiInteractions());
	ctx.game.UiInteractions().FocusTextInputByUid(password_modal_detail::kPasswordUid);
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

void PasswordModal::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	using namespace silencer::clay_bridge;



	bool focused = interactions.IsTextInputFocused(password_modal_detail::kPasswordUid);
	bool blink = ((SDL_GetTicks() / 250) % 2) == 0;

	CLAY({ .id = CLAY_ID("PasswordModalRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       } }) {
		CLAY({ .id = CLAY_ID("PasswordModalDialog"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(password_modal_detail::kDialogW),
		                       CLAY_SIZING_FIXED(password_modal_detail::kDialogH) },
		           .padding = { 34, 34, 30, 24 },
		           .childGap = 16,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImage(40, 2) } }) {
			password_modal_detail::BankText(CLAY_STRING("This game requires a password"),
			         password_modal_detail::BankTextVariant::Heading, {});
			silencer::ui::primitives::TextInput(
				CLAY_STRING("PasswordInput"),
				password,
				{ .widthPx = password_modal_detail::kInputW,
				  .heightPx = password_modal_detail::kInputH,
				  .fontBank = 135,
				  .fontWidth = 11,
				  .password = true,
				  .showCaret = focused && blink });
			password_modal_detail::BankButton(CLAY_STRING("OK"), password_modal_detail::BankButtonVariant::Chrome, {},
			           password_modal_detail::BankButtonHandle{ nullptr, password_modal_detail::kActionOk, &interactions });
		}
	}

	password_modal_detail::RegisterWidgets(this, password, dst.w, dst.h, interactions);
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
