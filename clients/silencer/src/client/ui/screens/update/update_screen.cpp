#include "update_screen.h"

#include "client/ui/screens/update/update_view.h"
#include "screen_context.h"
#include "game_state.h"
#include "updater.h"
#include "updaterstage2.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace update_screen_detail
{
constexpr const char * kActionUpdate = "update.update";
constexpr const char * kActionCancel = "update.cancel";
constexpr const char * kActionRetry = "update.retry";
constexpr const char * kActionDownload = "update.download";

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

silencer::client_ui::UpdatePrimaryAction PrimaryAction(ScreenContext & ctx)
{
	Updater::State ustate = ctx.updater.GetState();
	if(ustate == Updater::PROMPTING) return silencer::client_ui::UpdatePrimaryAction::Update;
	if(ustate == Updater::FAILED && ctx.updater.GetRetryCount() < 3){
		return silencer::client_ui::UpdatePrimaryAction::Retry;
	}
	if(ustate == Updater::FAILED) return silencer::client_ui::UpdatePrimaryAction::Download;
	return silencer::client_ui::UpdatePrimaryAction::None;
}

bool ShowCancel(ScreenContext & ctx)
{
	Updater::State ustate = ctx.updater.GetState();
	return ustate == Updater::PROMPTING || ustate == Updater::DOWNLOADING ||
	       ustate == Updater::FAILED;
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

bool UpdateScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	if(!out) return false;
	statusText_ = update_screen_detail::StatusText(ctx);
	progressText_ = update_screen_detail::ProgressText(ctx);
	*out = silencer::client_ui::UpdateView(
		silencer::client_ui::UpdateViewProps{
			.key = "update",
			.status = statusText_.c_str(),
			.progress = progressText_.c_str(),
			.primary_action = update_screen_detail::PrimaryAction(ctx),
			.show_cancel = update_screen_detail::ShowCancel(ctx),
			.on_update = [this](const ::ui::ActivationEvent&) {
				updateClicked = true;
			},
			.on_retry = [this](const ::ui::ActivationEvent&) {
				retryClicked = true;
			},
			.on_download = [this](const ::ui::ActivationEvent&) {
				downloadClicked = true;
			},
			.on_cancel = [this](const ::ui::ActivationEvent&) {
				cancelClicked = true;
			},
		});
	return true;
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
