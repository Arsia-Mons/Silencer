#include "update_screen.h"

#include "client/ui/hooks/use_navigation.h"
#include "client/ui/hooks/use_update.h"
#include "client/ui/screens/update/update_frame.h"
#include "main_menu_screen.h"
#include "screen_context.h"
#include "clay_ui_compositor.h"
#include "renderer.h"
#include "surface.h"

#include "runtime/UiInteractionRegistry.h"

#include <algorithm>
#include <memory>
#include <string>

namespace update_screen_detail
{
constexpr const char * kActionUpdate = "update.update";
constexpr const char * kActionCancel = "update.cancel";
constexpr const char * kActionRetry = "update.retry";
constexpr const char * kActionDownload = "update.download";
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
	silencer::client_ui::UpdateModel update =
		silencer::client_ui::use_update(
			silencer::client_ui::MakeUpdateProvider(ctx));
	if(updateClicked){
		updateClicked = false;
		update.consent();
	}
	if(cancelClicked){
		cancelClicked = false;
		if(update.cancel()){
			silencer::client_ui::use_navigation()
				.reset_to(std::make_unique<MainMenuScreen>());
			return;
		}
	}
	if(retryClicked){
		retryClicked = false;
		update.retry();
	}
	if(downloadClicked){
		downloadClicked = false;
		if(update.open_download()){
			silencer::client_ui::use_navigation()
				.reset_to(std::make_unique<MainMenuScreen>());
			return;
		}
	}
	silencer::client_ui::UpdateStage2Result stage2 =
		update.launch_stage2_if_ready();
	if(stage2 == silencer::client_ui::UpdateStage2Result::Spawned){
		return;
	}
	if(stage2 == silencer::client_ui::UpdateStage2Result::Failed){
		silencer::client_ui::use_navigation()
			.reset_to(std::make_unique<MainMenuScreen>());
	}
}

void UpdateScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	silencer::client_ui::UpdateSnapshot update =
		silencer::client_ui::use_update(
			silencer::client_ui::MakeUpdateProvider(ctx)).snapshot();

	const float uiScale = silencer::clay_bridge::UiScale();
	const int virtualW = std::max(1, static_cast<int>(dst.w / uiScale));
	const int virtualH = std::max(1, static_cast<int>(dst.h / uiScale));
	silencer::client_ui::UpdateFrameProps props{
		.key = "update-screen",
		.status_text = update.status_text.c_str(),
		.progress_text = update.progress_text.c_str(),
		.can_update = update.can_update,
		.can_cancel = update.can_cancel,
		.can_retry = update.can_retry,
		.can_download = update.can_download,
	};
	retainedFrame_.Build([&]() {
		                     return silencer::client_ui::UpdateFrame(props);
	                     },
	                     virtualW,
	                     virtualH,
	                     interactions);
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

const ::ui::DrawCommandList * UpdateScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
