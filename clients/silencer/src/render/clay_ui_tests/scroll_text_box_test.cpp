// P8 unit test scenes for the ScrollTextBox primitive.
//
// `RunScrollTextBoxTest(outPath)` renders one ScrollTextBox (6 lines, no
// scroll, top-down origin) into a 640x480 Surface and writes a PNG. The
// committed reference at tests/lobby-clay/scroll_text_box_test/
// reference.png is the pinned baseline.
//
// `RunScrollTextBoxCheck(out)` verifies the screen-side auto-scroll helper:
// a list at the bottom that grows by one line bumps the scroll position by
// one; a list that wasn't at the bottom keeps its scroll position.
//
// Invoked via the JSON-lines control ops `clay_scroll_text_box_test` and
// `clay_scroll_text_box_check`.

#include "clay_ui_compositor.h"
#include "clay/clay.h"
#include "primitives/bank_text.h"
#include "primitives/scroll_text_box.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

#include <cstdio>
#include <cstring>

namespace silencer::clay_bridge {

namespace {

using silencer::ui::primitives::ScrollTextBoxLine;
using silencer::ui::primitives::ScrollTextBoxOrigin;

// Static storage so Clay_String pointers stay valid for the layout pass.
constexpr int kTestLineCapacity = 16;
char g_lineText[kTestLineCapacity][32];
ScrollTextBoxLine g_lines[kTestLineCapacity];

void BuildLines(int count) {
	for(int i = 0; i < count && i < kTestLineCapacity; i++){
		std::snprintf(g_lineText[i], sizeof(g_lineText[i]),
		              "Chat line %02d", i + 1);
		g_lines[i].text.chars  = g_lineText[i];
		g_lines[i].text.length = static_cast<int32_t>(std::strlen(g_lineText[i]));
		g_lines[i].text.isStaticallyAllocated = false;
		// Two alternating colors so per-line color routing is exercised.
		g_lines[i].effectColor = (i & 1) ? 0 : 112;
		g_lines[i].brightness  = (i & 1) ? 128 : 160;
		g_lines[i].indent      = 0;
	}
}

}  // namespace

bool RunScrollTextBoxTest(::Game & game, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);
	silencer::ui::primitives::ScrollTextBoxBeginFrame();
	silencer::ui::primitives::BankTextBeginFrame();
	BuildLines(6);

	::Clay_BeginLayout();

	CLAY({ .id = CLAY_ID("ScrollTextBoxTestRoot"),
	       .layout = {
	           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { 50, 0, 50, 0 },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		silencer::ui::primitives::ScrollTextBox(
			CLAY_STRING("textbox"),
			g_lines,
			/*lineCount=*/6,
			/*scrollPosition=*/0,
			{ .width = 200,
			  .height = 66,            // 6 rows * 11 px = exactly fits.
			  .lineHeight = 11,
			  .textVariant = silencer::ui::primitives::BankTextVariant::Body,
			  .origin = ScrollTextBoxOrigin::TopDown });
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

bool RunScrollTextBoxCheck(::Game & game, ScrollTextBoxCheckResult & out) {
	(void)game;

	using silencer::ui::primitives::ScrollTextBoxAutoScroll;

	// Case 1: scrollbar at the bottom of a 5-line list (visibleLines=5),
	// append one line → auto-scroll bumps scrollPosition from 0 → 1.
	out.atBottom_prevPos = ScrollTextBoxAutoScroll(
		/*prevPos=*/0, /*prevLines=*/5, /*newLines=*/6,
		/*lineHeight=*/11, /*height=*/55);

	// Case 2: scrolled away from bottom (5-line list, pos=0 of max=5),
	// append one line → no auto-scroll, position stays at 0.
	out.notAtBottom_prevPos = ScrollTextBoxAutoScroll(
		/*prevPos=*/0, /*prevLines=*/10, /*newLines=*/11,
		/*lineHeight=*/11, /*height=*/55);

	// Case 3: at the bottom of a 10-line overflowing list (max=5, pos=5),
	// append one line (now max=6) → position bumps to 6.
	out.atBottomOverflow_prevPos = ScrollTextBoxAutoScroll(
		/*prevPos=*/5, /*prevLines=*/10, /*newLines=*/11,
		/*lineHeight=*/11, /*height=*/55);

	return true;
}

}  // namespace silencer::clay_bridge
