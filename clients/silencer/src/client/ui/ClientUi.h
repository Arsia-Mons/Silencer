#pragma once

#include "ui/runtime/ClayService.h"
#include "ui/runtime/UiAutomationRegistry.h"
#include "client/ui/navigation/ScreenStack.h"

#include <memory>
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
	std::vector<silencer::ui::UiRenderCommand> EndFrame();
	void DispatchInput(ScreenContext& ctx, const silencer::ui::UiInputState& input);
	std::vector<silencer::ui::UiAction> DrainActions();
	const silencer::ui::UiAutomationRegistry& Automation() const { return automation_; }
	silencer::ui::UiAutomationRegistry& Automation() { return automation_; }

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
	silencer::ui::ClayService& clay_;
	silencer::ui::UiAutomationRegistry& automation_;
	ScreenStack screens_;
};

}  // namespace client_ui
}  // namespace silencer
