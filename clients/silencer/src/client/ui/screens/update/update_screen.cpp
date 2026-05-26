#include "update_screen.h"

#include "client/ui/ClientUi.h"
#include "screen_context.h"
#include "game_state.h"
#include "surface.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/text.h"

#include <SDL3/SDL.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace update_screen_detail
{
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextSize;
using UpdateState = ScreenContext::UpdateState;

constexpr uint16_t kDialogW = 352;
constexpr uint16_t kDialogH = 178;
constexpr uint16_t kDialogPadX = 34;
constexpr uint16_t kDialogPadY = 42;
constexpr uint16_t kButtonGap = 6;
constexpr const char * kActionUpdate = "update.update";
constexpr const char * kActionCancel = "update.cancel";
constexpr const char * kActionRetry = "update.retry";
constexpr const char * kActionDownload = "update.download";

Clay_String FromStd(const std::string & s)
{
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

std::string StatusText(ScreenContext & ctx)
{
	switch(ctx.CurrentUpdateState()){
		case UpdateState::Prompting:
			return "An update is required to play online.";
		case UpdateState::Downloading:{
			char buf[32];
			snprintf(buf, sizeof(buf), "%d%%", int(ctx.UpdateProgress() * 100));
			return buf;
		}
		case UpdateState::Verifying:
			return "Verifying...";
		case UpdateState::Staging:
			return "Restarting...";
		case UpdateState::Failed:
			return ctx.UpdateErrorMessage();
		case UpdateState::Idle:
		case UpdateState::Done:
			return "";
	}
	assert(false && "Unhandled screen update state");
	std::abort();
}

std::string ProgressText(ScreenContext & ctx)
{
	if(ctx.CurrentUpdateState() != UpdateState::Downloading) return "";
	int width = int(ctx.UpdateProgress() * 20.0f);
	std::string bar = "[";
	for(int i = 0; i < 20; i++) bar += (i < width) ? "=" : " ";
	bar += "]";
	return bar;
}

std::function<void()> UseQueuedAction(std::function<void()> write)
{
	auto queueWrite = silencer::client_ui::UseUiWriteQueue();
	if(!queueWrite) return {};
	return [queueWrite, write]() {
		queueWrite(write);
	};
}

void Invoke(const std::function<void()> & action)
{
	if(action) action();
}
} // namespace update_screen_detail

void UpdateScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(2);
	consentUpdate = {};
	cancelUpdate = {};
	retryUpdate = {};
	openDownload = {};
}

void UpdateScreen::Tick(ScreenContext & ctx)
{
	using update_screen_detail::UpdateState;
	if(ctx.CurrentUpdateState() == UpdateState::Staging){
		if(ctx.LaunchStagedUpdate()) return;
		ctx.GoToState(GameState::MAINMENU);
	}
}

void UpdateScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	(void)dst;
	using update_screen_detail::UpdateState;
	using namespace silencer::clay_bridge;



	consentUpdate = update_screen_detail::UseQueuedAction([&ctx]() {
		if(ctx.CurrentUpdateState() == UpdateState::Prompting) ctx.ConsentUpdate();
	});
	cancelUpdate = update_screen_detail::UseQueuedAction([&ctx]() {
		UpdateState state = ctx.CurrentUpdateState();
		if(state == UpdateState::Prompting ||
		   state == UpdateState::Downloading ||
		   state == UpdateState::Failed){
			if(state == UpdateState::Downloading) ctx.CancelUpdate();
			ctx.GoToState(GameState::MAINMENU);
		}
	});
	retryUpdate = update_screen_detail::UseQueuedAction([&ctx]() {
		if(ctx.CurrentUpdateState() == UpdateState::Failed && ctx.UpdateRetryCount() < 3){
			ctx.RetryUpdate();
		}
	});
	openDownload = update_screen_detail::UseQueuedAction([&ctx]() {
		if(ctx.CurrentUpdateState() == UpdateState::Failed && ctx.UpdateRetryCount() >= 3){
			ctx.OpenUpdateDownloadPage();
			ctx.GoToState(GameState::MAINMENU);
		}
	});

	std::string status = update_screen_detail::StatusText(ctx);
	std::string progress = update_screen_detail::ProgressText(ctx);
	UpdateState ustate = ctx.CurrentUpdateState();

	CLAY({ .id = CLAY_ID("UpdateRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       } }) {
		CLAY({ .id = CLAY_ID("UpdateDialog"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(update_screen_detail::kDialogW),
		                       CLAY_SIZING_FIXED(update_screen_detail::kDialogH) },
		           .padding = { update_screen_detail::kDialogPadX, update_screen_detail::kDialogPadX,
		                        update_screen_detail::kDialogPadY, update_screen_detail::kDialogPadY },
		           .childGap = 12,
		           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .image = { .imageData = PackImage(40, 4) } }) {
			update_screen_detail::Text(update_screen_detail::FromStd(status),
			                           { .size = update_screen_detail::TextSize::Heading });
			if(!progress.empty()){
				update_screen_detail::Text(update_screen_detail::FromStd(progress),
				                           { .size = update_screen_detail::TextSize::Heading });
			}
			CLAY({ .id = CLAY_ID("UpdateActions"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			           .childGap = update_screen_detail::kButtonGap,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       } }) {
				if(ustate == UpdateState::Prompting){
					update_screen_detail::Button(CLAY_STRING("UpdateConsentButton"), CLAY_STRING("Update"),
					           update_screen_detail::ButtonOpts{ .variant = update_screen_detail::ButtonVariant::Chrome,
					                                             .size = update_screen_detail::ButtonSize::Compact },
					           update_screen_detail::ButtonHandle{ nullptr, update_screen_detail::kActionUpdate, &interactions });
				}else if(ustate == UpdateState::Failed && ctx.UpdateRetryCount() < 3){
					update_screen_detail::Button(CLAY_STRING("UpdateRetryButton"), CLAY_STRING("Retry"),
					           update_screen_detail::ButtonOpts{ .variant = update_screen_detail::ButtonVariant::Chrome,
					                                             .size = update_screen_detail::ButtonSize::Compact },
					           update_screen_detail::ButtonHandle{ nullptr, update_screen_detail::kActionRetry, &interactions });
				}else if(ustate == UpdateState::Failed){
					update_screen_detail::Button(CLAY_STRING("UpdateDownloadButton"), CLAY_STRING("Download"),
					           update_screen_detail::ButtonOpts{ .variant = update_screen_detail::ButtonVariant::Chrome,
					                                             .size = update_screen_detail::ButtonSize::Compact },
					           update_screen_detail::ButtonHandle{ nullptr, update_screen_detail::kActionDownload, &interactions });
				}
				if(ustate == UpdateState::Prompting || ustate == UpdateState::Downloading || ustate == UpdateState::Failed){
					update_screen_detail::Button(CLAY_STRING("UpdateCancelButton"), CLAY_STRING("Cancel"),
					           update_screen_detail::ButtonOpts{ .variant = update_screen_detail::ButtonVariant::Chrome,
					                                             .size = update_screen_detail::ButtonSize::Compact },
					           update_screen_detail::ButtonHandle{ nullptr, update_screen_detail::kActionCancel, &interactions });
				}
			}
		}
	}
}

void UpdateScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
	consentUpdate = {};
	cancelUpdate = {};
	retryUpdate = {};
	openDownload = {};
}

bool UpdateScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	(void)ctx;
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		update_screen_detail::Invoke(cancelUpdate);
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == update_screen_detail::kActionUpdate){
		update_screen_detail::Invoke(consentUpdate);
		return true;
	}
	if(action.id == update_screen_detail::kActionCancel){
		update_screen_detail::Invoke(cancelUpdate);
		return true;
	}
	if(action.id == update_screen_detail::kActionRetry){
		update_screen_detail::Invoke(retryUpdate);
		return true;
	}
	if(action.id == update_screen_detail::kActionDownload){
		update_screen_detail::Invoke(openDownload);
		return true;
	}
	return false;
}
