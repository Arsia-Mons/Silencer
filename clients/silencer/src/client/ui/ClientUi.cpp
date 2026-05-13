#include "client/ui/ClientUi.h"

#include "screen.h"
#include "screen_context.h"

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

namespace {

bool MovesFocusForward(silencer::ui::UiNavAction action) {
	return action == silencer::ui::UiNavAction::FocusNext ||
	       action == silencer::ui::UiNavAction::Down ||
	       action == silencer::ui::UiNavAction::Right ||
	       action == silencer::ui::UiNavAction::NextSection;
}

bool MovesFocusBackward(silencer::ui::UiNavAction action) {
	return action == silencer::ui::UiNavAction::FocusPrevious ||
	       action == silencer::ui::UiNavAction::Up ||
	       action == silencer::ui::UiNavAction::Left ||
	       action == silencer::ui::UiNavAction::PreviousSection;
}

}  // namespace

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

void ClientUi::DispatchInput(ScreenContext& ctx, const silencer::ui::UiInputState& input) {
	Screen * top = screens_.Top();
	if(top){
		bool capturedRawKey = false;
		for(int keyCode : input.rawKeyDownCodes){
			if(top->CaptureRawKeyDown(ctx, keyCode)) capturedRawKey = true;
		}
		if(capturedRawKey) return;
	}

	if(input.pointer.pressed){
		if(!automation_.FocusTextInputAt(
			   static_cast<int>(input.pointer.x),
			   static_cast<int>(input.pointer.y))){
			automation_.ClearFocus();
		}
	}

	for(char ascii : input.textInput){
		automation_.DispatchTextInput(ascii);
	}

	for(auto action : input.navActions){
		bool handled = false;
		if(action == silencer::ui::UiNavAction::Backspace){
			handled = automation_.BackspaceFocusedText();
		}else if(MovesFocusForward(action)){
			handled = automation_.FocusNextInteractive();
		}else if(MovesFocusBackward(action)){
			handled = automation_.FocusPreviousInteractive();
		}else if(action == silencer::ui::UiNavAction::Confirm){
			handled = automation_.SubmitFocusedText();
			if(!handled) handled = automation_.ActivateFocused();
		}else if(action == silencer::ui::UiNavAction::Cancel){
			if(automation_.HasFocus()) automation_.ClearFocus();
		}

		if(!handled && top){
			handled = top->HandleUiAction(ctx, action);
		}
		(void)handled;
	}
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
