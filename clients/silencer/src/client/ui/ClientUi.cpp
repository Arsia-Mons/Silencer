#include "client/ui/ClientUi.h"

namespace silencer {
namespace ui {
namespace primitives {
void BankButtonBeginFrame();
void BankTextBeginFrame();
void BoxBeginFrame();
void ScrollListBeginFrame();
void ScrollTextBoxBeginFrame();
void TextInputBeginFrame();
void ToggleBeginFrame();
}  // namespace primitives
}  // namespace ui
}  // namespace silencer

namespace silencer {
namespace client_ui {

ClientUi::ClientUi(silencer::ui::ClayService& clay)
	: clay_(clay), automation_(silencer::ui::ActiveUiAutomationRegistry()) {}

void ClientUi::BeginFrame(const silencer::ui::UiInputState& input) {
	silencer::ui::primitives::BankButtonBeginFrame();
	silencer::ui::primitives::BankTextBeginFrame();
	silencer::ui::primitives::BoxBeginFrame();
	silencer::ui::primitives::ScrollListBeginFrame();
	silencer::ui::primitives::ScrollTextBoxBeginFrame();
	silencer::ui::primitives::TextInputBeginFrame();
	silencer::ui::primitives::ToggleBeginFrame();
	clay_.BeginFrame(input, automation_);
}

std::vector<silencer::ui::UiRenderCommand> ClientUi::EndFrame() {
	return clay_.EndFrame();
}

std::vector<silencer::ui::UiAction> ClientUi::DrainActions() {
	return automation_.DrainActions();
}

}  // namespace client_ui
}  // namespace silencer
