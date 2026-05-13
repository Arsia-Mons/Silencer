#include "password_modal.h"

#include "screen_context.h"
#include "game.h"
#include "renderer.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_bridge.h"
#include "clay_inspector.h"
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

void OkClicked(void * user)
{
	auto * modal = static_cast<PasswordModal *>(user);
	if(modal) modal->NotifyOkClicked();
}

void RegisterWidgets(PasswordModal * modal, char * buffer, int surfaceW, int surfaceH)
{
	const int dialogX = (surfaceW - kDialogW) / 2;
	const int dialogY = (surfaceH - kDialogH) / 2;
	silencer::ui::clay_inspector::Widget input;
	input.label = "Password";
	input.kind = silencer::ui::clay_inspector::WidgetKind::TextInput;
	input.uid = kPasswordUid;
	input.x = dialogX + (kDialogW - kInputW) / 2;
	input.y = dialogY + 64;
	input.w = kInputW;
	input.h = kInputH;
	input.textBuffer = buffer;
	input.textBufferLen = 21;
	input.isPassword = true;
	input.onEnter = &OkClicked;
	input.enterUser = modal;
	silencer::ui::clay_inspector::Register(input);

	silencer::ui::clay_inspector::Widget ok;
	ok.label = "OK";
	ok.kind = silencer::ui::clay_inspector::WidgetKind::Button;
	ok.x = dialogX + (kDialogW - 156) / 2;
	ok.y = dialogY + 92;
	ok.w = 156; ok.h = 21;
	ok.onClick = &OkClicked;
	ok.clickUser = modal;
	silencer::ui::clay_inspector::Register(ok);
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
	silencer::ui::clay_inspector::BeginFrame();
	RegisterWidgets(this, password, 640, 480);
	silencer::ui::clay_inspector::FocusTextInputByUid(kPasswordUid);
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

void PasswordModal::Draw(ScreenContext & ctx, Surface & dst, float frametime)
{
	(void)frametime;
	using namespace silencer::clay_bridge;

	EnsureInitialized(dst.w, dst.h);
	float mx = 0.f, my = 0.f;
	Uint32 buttons = SDL_GetMouseState(&mx, &my);
	Clay_SetPointerState({ mx, my },
	                      (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0);

	BankButtonBeginFrame();
	BankTextBeginFrame();
	TextInputBeginFrame();
	silencer::ui::clay_inspector::BeginFrame();

	bool focused = silencer::ui::clay_inspector::IsTextInputFocused(kPasswordUid);
	bool blink = ((SDL_GetTicks() / 250) % 2) == 0;

	Clay_BeginLayout();
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
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
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
			           BankButtonHandle{ nullptr, &OkClicked, this });
		}
	}
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	Render(ctx.game, &dst, cmds);
	RegisterWidgets(this, password, dst.w, dst.h);
}

void PasswordModal::Destroy(ScreenContext & ctx)
{
	(void)ctx;
	silencer::ui::clay_inspector::ClearFocus();
}

bool PasswordModal::HandleTextInput(ScreenContext & ctx, char ascii)
{
	(void)ctx;
	return silencer::ui::clay_inspector::DispatchTextInput(ascii);
}

bool PasswordModal::HandleKeyPress(ScreenContext & ctx, char ascii)
{
	(void)ctx;
	if(silencer::ui::clay_inspector::DispatchKeyPress(ascii)) return true;
	if(ascii == '\n'){
		okClicked = true;
		return true;
	}
	return false;
}
