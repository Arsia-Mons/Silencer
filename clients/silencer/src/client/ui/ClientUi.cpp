#include "client/ui/ClientUi.h"

#include "screen.h"
#include "screen_context.h"
#include "runtime/UiInputRouter.h"

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

std::vector<silencer::ui::UiAction> ClientUi::DispatchInput(
	ScreenContext& ctx,
	const silencer::ui::UiInputState& input) {
	Screen * top = screens_.Top();
	if(top){
		bool capturedRawKey = false;
		for(int keyCode : input.rawKeyDownCodes){
			if(top->CaptureRawKeyDown(ctx, keyCode)) capturedRawKey = true;
		}
		if(capturedRawKey) return std::vector<silencer::ui::UiAction>();
	}

	silencer::ui::UiInputRouter router(automation_);
	std::vector<silencer::ui::UiAction> actions = router.Route(input);
	if(!top) return actions;
	std::vector<silencer::ui::UiAction> unhandled;
	for(const silencer::ui::UiAction& action : actions){
		if(top && top->HandleUiIntent(ctx, action)) continue;
		if(automation_.DispatchAction(action)) continue;
		unhandled.push_back(action);
	}
	return unhandled;
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
