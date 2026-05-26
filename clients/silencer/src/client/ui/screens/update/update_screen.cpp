#include "update_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "renderer.h"
#include "surface.h"
#include "updater.h"
#include "updaterstage2.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/text.h"

#include <SDL3/SDL.h>

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
	switch(ctx.updater.GetState()){
		case Updater::PROMPTING:
			return "An update is required to play online.";
		case Updater::DOWNLOADING:{
			char buf[32];
			snprintf(buf, sizeof(buf), "%d%%", int(ctx.updater.GetProgress() * 100));
			return buf;
		}
		case Updater::VERIFYING:
			return "Verifying...";
		case Updater::STAGING:
			return "Restarting...";
		case Updater::FAILED:
			return ctx.updater.GetErrorMessage();
		case Updater::IDLE:
		case Updater::DONE:
		default:
			return "";
	}
}

std::string ProgressText(ScreenContext & ctx)
{
	if(ctx.updater.GetState() != Updater::DOWNLOADING) return "";
	int width = int(ctx.updater.GetProgress() * 20.0f);
	std::string bar = "[";
	for(int i = 0; i < 20; i++) bar += (i < width) ? "=" : " ";
	bar += "]";
	return bar;
}
} // namespace update_screen_detail

void UpdateScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(2);
	updateClicked = false;
	cancelClicked = false;
	retryClicked = false;
	downloadClicked = false;
}

void UpdateScreen::Tick(ScreenContext & ctx)
{
	Updater::State ustate = ctx.updater.GetState();
	if(updateClicked){
		updateClicked = false;
		if(ustate == Updater::PROMPTING) ctx.updater.Consent();
	}
	if(cancelClicked){
		cancelClicked = false;
		if(ustate == Updater::PROMPTING || ustate == Updater::DOWNLOADING || ustate == Updater::FAILED){
			if(ustate == Updater::DOWNLOADING) ctx.updater.Cancel();
			ctx.GoToState(GameState::MAINMENU);
			return;
		}
	}
	if(retryClicked){
		retryClicked = false;
		if(ustate == Updater::FAILED && ctx.updater.GetRetryCount() < 3){
			ctx.updater.Retry();
		}
	}
	if(downloadClicked){
		downloadClicked = false;
		if(ustate == Updater::FAILED && ctx.updater.GetRetryCount() >= 3){
			std::string url = ctx.updater.GetDownloadURL();
#ifdef _WIN32
			std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
			std::string cmd = "open '" + url + "'";
#else
			std::string cmd = "xdg-open '" + url + "' &";
#endif
			system(cmd.c_str());
			ctx.GoToState(GameState::MAINMENU);
			return;
		}
	}
	if(ctx.updater.GetState() == Updater::STAGING){
		std::string zippath =
#ifdef _WIN32
			std::string(getenv("TEMP") ? getenv("TEMP") : ".") + "\\silencer-update.zip";
#else
			"/tmp/silencer-update.zip";
#endif
		fprintf(stderr, "[updater] UpdateScreen invoking UpdaterStage2::Launch with zip=%s\n",
		        zippath.c_str());
		if(UpdaterStage2::Launch(zippath)){
			ctx.updater.MarkStage2Spawned();
			return;
		}
		fprintf(stderr, "[updater] UpdaterStage2::Launch failed; returning to main menu\n");
		ctx.GoToState(GameState::MAINMENU);
	}
}

void UpdateScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	(void)dst;
	using namespace silencer::clay_bridge;



	std::string status = update_screen_detail::StatusText(ctx);
	std::string progress = update_screen_detail::ProgressText(ctx);
	Updater::State ustate = ctx.updater.GetState();

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
				if(ustate == Updater::PROMPTING){
					update_screen_detail::Button(CLAY_STRING("UpdateConsentButton"), CLAY_STRING("Update"),
					           update_screen_detail::ButtonOpts{ .variant = update_screen_detail::ButtonVariant::Chrome,
					                                             .size = update_screen_detail::ButtonSize::Compact },
					           update_screen_detail::ButtonHandle{ nullptr, update_screen_detail::kActionUpdate, &interactions });
				}else if(ustate == Updater::FAILED && ctx.updater.GetRetryCount() < 3){
					update_screen_detail::Button(CLAY_STRING("UpdateRetryButton"), CLAY_STRING("Retry"),
					           update_screen_detail::ButtonOpts{ .variant = update_screen_detail::ButtonVariant::Chrome,
					                                             .size = update_screen_detail::ButtonSize::Compact },
					           update_screen_detail::ButtonHandle{ nullptr, update_screen_detail::kActionRetry, &interactions });
				}else if(ustate == Updater::FAILED){
					update_screen_detail::Button(CLAY_STRING("UpdateDownloadButton"), CLAY_STRING("Download"),
					           update_screen_detail::ButtonOpts{ .variant = update_screen_detail::ButtonVariant::Chrome,
					                                             .size = update_screen_detail::ButtonSize::Compact },
					           update_screen_detail::ButtonHandle{ nullptr, update_screen_detail::kActionDownload, &interactions });
				}
				if(ustate == Updater::PROMPTING || ustate == Updater::DOWNLOADING || ustate == Updater::FAILED){
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
}

bool UpdateScreen::HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action)
{
	(void)ctx;
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		cancelClicked = true;
		return true;
	}
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == update_screen_detail::kActionUpdate){
		updateClicked = true;
		return true;
	}
	if(action.id == update_screen_detail::kActionCancel){
		cancelClicked = true;
		return true;
	}
	if(action.id == update_screen_detail::kActionRetry){
		retryClicked = true;
		return true;
	}
	if(action.id == update_screen_detail::kActionDownload){
		downloadClicked = true;
		return true;
	}
	return false;
}
