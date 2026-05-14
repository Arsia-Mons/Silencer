#include "ui/runtime/UiFrameContext.h"

#include "ui/primitives/bank_button.h"
#include "ui/primitives/bank_text.h"
#include "ui/primitives/box.h"
#include "ui/primitives/scroll_list.h"
#include "ui/primitives/scroll_text_box.h"
#include "ui/primitives/text_input.h"
#include "ui/primitives/toggle.h"
#include "client/ui/hud/HudPayloadArena.h"

namespace silencer {
namespace ui {

UiFrameContext::UiFrameContext() = default;
UiFrameContext::~UiFrameContext() = default;

void UiFrameContext::BeginFrame() {
	silencer::ui::primitives::BankButtonBeginFrame();
	silencer::ui::primitives::BankTextBeginFrame();
	silencer::ui::primitives::BoxBeginFrame();
	silencer::ui::primitives::ScrollListBeginFrame();
	silencer::ui::primitives::ScrollTextBoxBeginFrame();
	silencer::ui::primitives::TextInputBeginFrame();
	silencer::ui::primitives::ToggleBeginFrame();
	silencer::client_ui::HudPayloadBeginFrame();
}

}  // namespace ui
}  // namespace silencer
