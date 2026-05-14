#include "password_modal.h"

#include "screen_context.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiAutomationRegistry.h"
#include "primitives/bank_button.h"
#include "primitives/bank_text.h"
#include "primitives/text_input.h"

#include <SDL3/SDL.h>

#include <cstring>

namespace
{
using silencer::ui::primitives::BankButton;
using silencer::ui::primitives::BankButtonBeginFrame;
using silencer::ui::primitives::BankButtonHandle;
using silencer::ui::primitives::BankButtonVariant;
using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextBeginFrame;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::TextInputBeginFrame;

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

void RegisterWidgets(PasswordModal * modal, char * buffer, int surfaceW, int surfaceH)
{
	(void)modal;
	const int dialogX = (surfaceW - kDialogW) / 2;
	const int dialogY = (surfaceH - kDialogH) / 2;
	silencer::ui::automation::Widget input;
	input.id = kActionPassword;
	input.labelText = "Password";
	input.kind = silencer::ui::automation::WidgetKind::TextInput;
	input.uid = kPasswordUid;
	input.x = dialogX + (kDialogW - kInputW) / 2;
	input.y = dialogY + 64;
	input.w = kInputW;
	input.h = kInputH;
	input.value = buffer ? buffer : "";
	input.maxLength = 20;
	input.isPassword = true;
	silencer::ui::automation::Register(input);

	silencer::ui::automation::Widget ok;
	ok.id = kActionOk;
	ok.labelText = "OK";
	ok.kind = silencer::ui::automation::WidgetKind::Button;
	ok.x = dialogX + (kDialogW - 156) / 2;
	ok.y = dialogY + 92;
	ok.w = 156; ok.h = 21;
	silencer::ui::automation::Register(ok);
	(void)surfaceH;
}
}

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
	RegisterWidgets(this, password, surface.w, surface.h);
	silencer::ui::automation::FocusTextInputByUid(kPasswordUid);
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

void PasswordModal::BuildUi(ScreenContext & ctx, Surface & dst, float frametime)
{
	(void)frametime;
	using namespace silencer::clay_bridge;



	bool focused = silencer::ui::automation::IsTextInputFocused(kPasswordUid);
	bool blink = ((SDL_GetTicks() / 250) % 2) == 0;

	CLAY({ .id = CLAY_ID("PasswordModalRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED((float)dst.w),
	                       CLAY_SIZING_FIXED((float)dst.h) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       } }) {
		CLAY({ .id = CLAY_ID("PasswordModalDialog"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kDialogW),
		                       CLAY_SIZING_FIXED(kDialogH) },
		           .padding = { 34, 34, 30, 24 },
		           .childGap = 16,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImage(40, 2) } }) {
			BankText(CLAY_STRING("This game requires a password"),
			         BankTextVariant::Heading, {});
			silencer::ui::primitives::TextInput(
				CLAY_STRING("PasswordInput"),
				password,
				{ .widthPx = kInputW,
				  .heightPx = kInputH,
				  .fontBank = 135,
				  .fontWidth = 11,
				  .password = true,
				  .showCaret = focused && blink });
			BankButton(CLAY_STRING("OK"), BankButtonVariant::Chrome, {},
			           BankButtonHandle{ nullptr, kActionOk });
		}
	}
	RegisterWidgets(this, password, dst.w, dst.h);
}

void PasswordModal::Destroy(ScreenContext & ctx)
{
	(void)ctx;
	silencer::ui::automation::ClearFocus();
}

bool PasswordModal::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	(void)ctx;
	if(action.kind == silencer::ui::UiActionKind::SetText &&
	   action.id == kActionPassword){
		CopyUiText(password, static_cast<int>(sizeof(password)), action.value);
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::SubmitText && action.id == kActionPassword){
		CopyUiText(password, static_cast<int>(sizeof(password)), action.value);
		okClicked = true;
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Activate && action.id == kActionOk){
		okClicked = true;
		return true;
	}
	return false;
}
