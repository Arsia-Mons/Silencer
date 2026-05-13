#include "client/ui/ClientUi.h"

namespace silencer {
namespace client_ui {

ClientUi::ClientUi(silencer::ui::ClayService& clay, ClientUiState& state)
	: clay_(clay), state_(state), automation_(silencer::ui::ActiveUiAutomationRegistry()) {}

void ClientUi::BeginFrame(const silencer::ui::UiInputState& input) {
	clay_.BeginFrame(input, automation_);
}

std::vector<silencer::ui::UiRenderCommand> ClientUi::EndFrame() {
	return clay_.EndFrame();
}

std::vector<silencer::ui::UiRenderCommand> ClientUi::BuildFrame(const silencer::ui::UiInputState& input) {
	BeginFrame(input);
	if(state_.activeScreen == ScreenId::MainMenu) {
		BuildMainMenu();
	}
	return EndFrame();
}

std::vector<silencer::ui::UiAction> ClientUi::DrainActions() {
	return automation_.DrainActions();
}

void ClientUi::BuildMainMenu() {
	silencer::ui::ButtonProps connect;
	connect.id = "main_menu.connect";
	connect.label = "Connect";
	connect.bounds = silencer::ui::UiRect{ 24.0f, 96.0f, 176.0f, 28.0f };
	silencer::ui::RegisterButton(connect, automation_);

	silencer::ui::ButtonProps options;
	options.id = "main_menu.options";
	options.label = "Options";
	options.bounds = silencer::ui::UiRect{ 24.0f, 132.0f, 176.0f, 28.0f };
	silencer::ui::RegisterButton(options, automation_);
}

}  // namespace client_ui
}  // namespace silencer
