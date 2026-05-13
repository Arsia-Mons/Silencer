#pragma once

#include "client/ui/ClientUiState.h"
#include "ui/primitives/Button.h"
#include "ui/runtime/ClayService.h"
#include "ui/runtime/UiActionQueue.h"
#include "ui/runtime/UiAutomationRegistry.h"

namespace silencer {
namespace client_ui {

class ClientUi {
public:
	ClientUi(silencer::ui::ClayService& clay, ClientUiState& state);

	std::vector<silencer::ui::UiRenderCommand> BuildFrame(const silencer::ui::UiInputState& input);
	std::vector<silencer::ui::UiAction> DrainActions();
	const silencer::ui::UiAutomationRegistry& Automation() const { return automation_; }

private:
	void BuildMainMenu();

	silencer::ui::ClayService& clay_;
	ClientUiState& state_;
	silencer::ui::UiAutomationRegistry automation_;
	silencer::ui::UiActionQueue actions_;
};

}  // namespace client_ui
}  // namespace silencer
