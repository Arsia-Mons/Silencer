#include "client/ui/ClientUi.h"

#include "screen.h"

#include <utility>

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

ClientUi::~ClientUi() = default;

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

void ClientUi::PushScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	screens_.Push(std::move(screen), ctx);
}

void ClientUi::PopScreen(ScreenContext& ctx) {
	screens_.Pop(ctx);
}

void ClientUi::ReplaceScreen(std::unique_ptr<Screen> screen, ScreenContext& ctx) {
	screens_.Replace(std::move(screen), ctx);
}

void ClientUi::RequestClearScreens() {
	screens_.RequestClear();
}

void ClientUi::ClearScreensIfRequested(ScreenContext& ctx) {
	screens_.ClearIfRequested(ctx);
}

void ClientUi::TickVisibleScreens(ScreenContext& ctx) {
	screens_.TickVisible(ctx);
}

void ClientUi::BuildVisibleScreens(ScreenContext& ctx, Surface& dst, float frametime) {
	screens_.BuildVisible(ctx, dst, frametime, automation_);
}

}  // namespace client_ui
}  // namespace silencer
