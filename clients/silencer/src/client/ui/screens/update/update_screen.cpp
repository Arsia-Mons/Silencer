#include "update_screen.h"

#include "client/ui/screens/main_menu/main_menu_screen.h"
#include "client/ui/screens/update/update_view.h"
#include "screen_context.h"
#include "peer.h"
#include "updater.h"
#include "updaterstage2.h"
#include "world.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

namespace update_screen_detail
{
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

void StartUpdate(Updater & updater)
{
	if(updater.GetState() == Updater::PROMPTING) updater.Consent();
}

bool CancelUpdate(Updater & updater)
{
	Updater::State ustate = updater.GetState();
	if(ustate != Updater::PROMPTING && ustate != Updater::DOWNLOADING &&
	   ustate != Updater::FAILED){
		return false;
	}
	if(ustate == Updater::DOWNLOADING) updater.Cancel();
	return true;
}

void RetryUpdate(Updater & updater)
{
	if(updater.GetState() == Updater::FAILED && updater.GetRetryCount() < 3){
		updater.Retry();
	}
}

bool OpenDownload(Updater & updater)
{
	if(updater.GetState() != Updater::FAILED || updater.GetRetryCount() < 3){
		return false;
	}
	std::string url = updater.GetDownloadURL();
#ifdef _WIN32
	std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
	std::string cmd = "open '" + url + "'";
#else
	std::string cmd = "xdg-open '" + url + "' &";
#endif
	system(cmd.c_str());
	return true;
}
} // namespace update_screen_detail

void UpdateScreen::Build(ScreenContext & ctx)
{
	ctx.world.GetAuthorityPeer()->controlledlist.clear();
	ctx.world.DestroyAllObjects();
	ctx.ResetPresentation(2);
}

void UpdateScreen::Tick(ScreenContext & ctx)
{
	ctx.PlayMenuMusicIfReady();
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
		ctx.ResetToScreen(std::make_unique<MainMenuScreen>());
	}
}

bool UpdateScreen::BuildElement(ScreenContext & ctx, ::ui::UiElement * out)
{
	if(!out) return false;
	statusText_ = update_screen_detail::StatusText(ctx);
	progressText_ = update_screen_detail::ProgressText(ctx);
	const silencer::client_ui::UpdateStatus status{
		.status = statusText_.c_str(),
		.progress = progressText_.c_str(),
		.primary_action = update_screen_detail::PrimaryAction(ctx),
		.show_cancel = update_screen_detail::ShowCancel(ctx),
		.start_update = [updater = &ctx.updater]() {
			update_screen_detail::StartUpdate(*updater);
		},
		.retry = [updater = &ctx.updater]() {
			update_screen_detail::RetryUpdate(*updater);
		},
		.download = [updater = &ctx.updater]() {
			return update_screen_detail::OpenDownload(*updater);
		},
		.cancel = [updater = &ctx.updater]() {
			return update_screen_detail::CancelUpdate(*updater);
		},
	};
	const auto * stored = ::ui::copy_value(status);
	if(!stored) return false;
	*out = ::ui::component(
		"UpdateView",
		silencer::client_ui::UpdateViewProps{
			.key = "update",
			.status = stored,
		},
		silencer::client_ui::UpdateView);
	return true;
}

void UpdateScreen::Destroy(ScreenContext & ctx)
{
	(void)ctx;
}

bool UpdateScreen::HandleBack(ScreenContext & ctx)
{
	if(update_screen_detail::CancelUpdate(ctx.updater)){
		ctx.ResetToScreen(std::make_unique<MainMenuScreen>());
	}
	return true;
}
