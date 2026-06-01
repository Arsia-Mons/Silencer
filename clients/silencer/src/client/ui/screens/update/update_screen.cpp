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

void UpdateScreen::Build(ScreenContext & ctx)
{
	ctx.ResetPresentation(2);
}

void UpdateScreen::Tick(ScreenContext & ctx)
{
	silencer::client_ui::UpdateModel update =
		silencer::client_ui::use_update(
			silencer::client_ui::MakeUpdateProvider(ctx));
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

void UpdateScreen::BuildUi(ScreenContext & ctx, Surface & dst, float frametime, const silencer::ui::UiInputState&, Uint8, silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;
	silencer::client_ui::UpdateSnapshot update =
		silencer::client_ui::use_update(
			silencer::client_ui::MakeUpdateProvider(ctx)).snapshot();
	silencer::client_ui::UpdateModel updateModel =
		silencer::client_ui::use_update(
			silencer::client_ui::MakeUpdateProvider(ctx));
	silencer::client_ui::Navigation navigation =
		silencer::client_ui::use_navigation();

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
		.start_update = [updateModel]() {
			updateModel.consent();
		},
		.cancel = [updateModel, navigation]() {
			if(updateModel.cancel()){
				navigation.reset_to(std::make_unique<MainMenuScreen>());
			}
		},
		.retry = [updateModel]() {
			updateModel.retry();
		},
		.download = [updateModel, navigation]() {
			if(updateModel.open_download()){
				navigation.reset_to(std::make_unique<MainMenuScreen>());
			}
		},
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
	if(action.kind == silencer::ui::UiActionKind::Cancel){
		silencer::client_ui::UpdateModel update =
			silencer::client_ui::use_update(
				silencer::client_ui::MakeUpdateProvider(ctx));
		CancelUpdate(update, silencer::client_ui::use_navigation());
		return true;
	}
	return retainedFrame_.HandleUiIntent(action);
}

void UpdateScreen::CancelUpdate(
	const silencer::client_ui::UpdateModel & update,
	const silencer::client_ui::Navigation & navigation) const {
	if(update.cancel()){
		navigation.reset_to(std::make_unique<MainMenuScreen>());
	}
}

const ::ui::DrawCommandList * UpdateScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
