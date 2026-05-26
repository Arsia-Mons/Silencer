#pragma once

#include "ui/runtime/ClayService.h"
#include "ui/focus/UiFocus.h"
#include "ui/runtime/UiInteractionRegistry.h"
#include "ui/runtime/UiFrameContext.h"
#include "client/ui/navigation/ScreenStack.h"

#include <memory>
#include <string>
#include <vector>

class Screen;
class ScreenContext;
class Surface;

namespace silencer {
namespace client_ui {

class ClientUi {
public:
	explicit ClientUi(silencer::ui::ClayService& clay);
	~ClientUi();

	void BeginFrame(const silencer::ui::UiInputState& input);
	Clay_RenderCommandArray EndFrame();
	std::vector<silencer::ui::UiAction> DispatchInput(ScreenContext& ctx, const silencer::ui::UiInputState& input);
	std::vector<silencer::ui::UiAction> DrainActions();
	const silencer::ui::UiInteractionRegistry& Interactions() const { return interactions_; }
	silencer::ui::UiInteractionRegistry& Interactions() { return interactions_; }
	const silencer::ui::UiFocusRuntime& FocusRuntime() const { return focus_; }
	silencer::ui::UiFocusRuntime& FocusRuntime() { return focus_; }

	bool HasScreens() const { return !screens_.Empty(); }
	Screen * TopScreen() const { return screens_.Top(); }
	void PushScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void PopScreen(ScreenContext& ctx);
	void ReplaceScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx);
	void RequestClearScreens();
	void ClearScreensIfRequested(ScreenContext& ctx);
	void TickVisibleScreens(ScreenContext& ctx);
	void BuildVisibleScreens(ScreenContext& ctx, Surface& dst, float frametime);

private:
	silencer::ui::UiFrameContext frameCtx_;
	silencer::ui::ClayService& clay_;
	silencer::ui::UiInteractionRegistry interactions_;
	silencer::ui::UiFocusRuntime focus_;
	silencer::ui::UiFocusInputFrame focusInput_;
	ScreenStack screens_;
	std::string hoveredAudioInteractableId_;
};

}  // namespace client_ui
}  // namespace silencer
