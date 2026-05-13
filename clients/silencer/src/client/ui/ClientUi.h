#pragma once

#include "ui/runtime/ClayService.h"
#include "ui/runtime/UiAutomationRegistry.h"

namespace silencer {
namespace client_ui {

class ClientUi {
public:
	explicit ClientUi(silencer::ui::ClayService& clay);

	void BeginFrame(const silencer::ui::UiInputState& input);
	std::vector<silencer::ui::UiRenderCommand> EndFrame();
	std::vector<silencer::ui::UiAction> DrainActions();
	const silencer::ui::UiAutomationRegistry& Automation() const { return automation_; }
	silencer::ui::UiAutomationRegistry& Automation() { return automation_; }

private:
	silencer::ui::ClayService& clay_;
	silencer::ui::UiAutomationRegistry& automation_;
};

}  // namespace client_ui
}  // namespace silencer
