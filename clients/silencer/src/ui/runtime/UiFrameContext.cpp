#include "ui/runtime/UiFrameContext.h"

#include "ui/primitives/button.h"
#include "ui/primitives/box.h"
#include "ui/primitives/scroll_list.h"
#include "ui/primitives/scroll_text_box.h"
#include "ui/primitives/text.h"
#include "ui/primitives/text_input.h"
#include "ui/primitives/toggle.h"

namespace silencer {
namespace ui {

UiFrameContext::UiFrameContext() = default;
UiFrameContext::~UiFrameContext() = default;

void UiFrameContext::BeginFrame(float animationDeltaSeconds, float animationStepSeconds) {
	silencer::ui::primitives::ButtonBeginFrame(animationDeltaSeconds, animationStepSeconds);
	silencer::ui::primitives::BoxBeginFrame();
	silencer::ui::primitives::ScrollListBeginFrame();
	silencer::ui::primitives::ScrollTextBoxBeginFrame();
	silencer::ui::primitives::TextBeginFrame();
	silencer::ui::primitives::TextInputBeginFrame();
	silencer::ui::primitives::ToggleBeginFrame();
}

}  // namespace ui
}  // namespace silencer
