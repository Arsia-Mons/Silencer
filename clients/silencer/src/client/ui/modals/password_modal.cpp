#include "password_modal.h"

#include "client/ui/ClientUi.h"
#include "screen_context.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/text.h"
#include "primitives/text_input.h"

#include <cstring>

namespace password_modal_detail
{
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextSize;

constexpr uint16_t kDialogW = 352;
constexpr uint16_t kDialogH = 148;
constexpr uint16_t kInputW = 180;
constexpr uint16_t kInputH = 14;
constexpr int kPasswordUid = 1;
constexpr const char * kActionPassword = "password_modal.password";
constexpr const char * kActionOk = "password_modal.ok";

template <typename Text>
void CopyUiText(char * dst, int dstLen, const Text & value)
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

std::function<void()> UseQueuedSubmit(ScreenContext & ctx,
                                      char * password,
                                      std::function<void(const char *)> * onSubmit)
{
	auto queueWrite = silencer::client_ui::UseUiWriteQueue();
	if(!queueWrite) return {};
	return [queueWrite, &ctx, password, onSubmit]() {
		std::string captured = password ? password : "";
		std::function<void(const char *)> cb;
		if(onSubmit) cb = std::move(*onSubmit);
		queueWrite([&ctx, captured, cb]() mutable {
			ctx.PopScreen();
			if(cb) cb(captured.c_str());
		});
	};
}
} // namespace password_modal_detail

PasswordModal::PasswordModal(std::function<void(const char *)> onSubmit_)
    : onSubmit(std::move(onSubmit_))
{
}

void PasswordModal::Build(ScreenContext & ctx)
{
	(void)ctx;
	submitQueued = false;
	submit = {};
	focusPasswordRequested = true;
	password[0] = '\0';
}

void PasswordModal::Tick(ScreenContext & ctx)
{
	(void)ctx;
}

void PasswordModal::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	using namespace silencer::clay_bridge;

	const bool requestPasswordInitialFocus = focusPasswordRequested;
	focusPasswordRequested = false;
	bool focused = interactions.IsTextInputFocused(password_modal_detail::kPasswordUid);
	bool blink = ctx.UiBlinkVisible();
	submit = password_modal_detail::UseQueuedSubmit(ctx, password, &onSubmit);

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
			password_modal_detail::Text(CLAY_STRING("This game requires a password"),
			         { .size = password_modal_detail::TextSize::Heading });
			silencer::ui::primitives::TextInput(
				CLAY_STRING("PasswordInput"),
				password,
				{ .widthPx = password_modal_detail::kInputW,
				  .heightPx = password_modal_detail::kInputH,
				  .textSize = password_modal_detail::TextSize::Title,
				  .password = true,
				  .showCaret = focused && blink },
				{ nullptr, password_modal_detail::kActionPassword, "Password",
				  &interactions, password_modal_detail::kPasswordUid, 20,
				  false, requestPasswordInitialFocus });
			password_modal_detail::Button(CLAY_STRING("PasswordModalOkButton"), CLAY_STRING("OK"),
			           password_modal_detail::ButtonOpts{ .variant = password_modal_detail::ButtonVariant::Chrome,
			                                             .size = password_modal_detail::ButtonSize::Compact },
			           password_modal_detail::ButtonHandle{ nullptr, password_modal_detail::kActionOk, &interactions });
		}
	}

	password_modal_detail::RegisterWidgets(this, password, interactions);
}

void PasswordModal::Destroy(ScreenContext & ctx)
{
	ctx.ClearUiFocus();
	submit = {};
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
		if(submit && !submitQueued){
			submitQueued = true;
			submit();
		}
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::Activate && action.id == password_modal_detail::kActionOk){
		if(submit && !submitQueued){
			submitQueued = true;
			submit();
		}
		return true;
	}
	return false;
}
