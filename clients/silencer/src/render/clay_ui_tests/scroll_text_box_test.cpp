// Assertion probes for the ScrollTextBox primitive.

#include "clay_ui_tests/clay_ui_checks.h"
#include "primitives/scroll_text_box.h"

namespace silencer::clay_bridge {

bool RunScrollTextBoxCheck(::Game & game, ScrollTextBoxCheckResult & out) {
	(void)game;

	using silencer::ui::primitives::ScrollTextBoxAutoScroll;

	out.atBottom_prevPos = ScrollTextBoxAutoScroll(
		/*prevPos=*/0, /*prevLines=*/5, /*newLines=*/6,
		/*lineHeight=*/11, /*height=*/55);

	out.notAtBottom_prevPos = ScrollTextBoxAutoScroll(
		/*prevPos=*/0, /*prevLines=*/10, /*newLines=*/11,
		/*lineHeight=*/11, /*height=*/55);

	out.atBottomOverflow_prevPos = ScrollTextBoxAutoScroll(
		/*prevPos=*/5, /*prevLines=*/10, /*newLines=*/11,
		/*lineHeight=*/11, /*height=*/55);

	return true;
}

}  // namespace silencer::clay_bridge
