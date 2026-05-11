// P7 unit test scenes for the ScrollList primitive.
//
// `RunScrollListTest(outPath)` renders one ScrollList (30 items,
// selectedIndex=8, scrollPosition=3) into a 640x480 Surface and writes a
// PNG. The committed reference at tests/lobby-clay/scroll_list_test/
// reference.png is the pinned baseline.
//
// `RunScrollListCheck(out)` lays out a 10-item list and drives a press
// timeline over the bbox of a known visible row, asserting that exactly
// one onSelect call fires with the correct index.
//
// Invoked via the JSON-lines control ops `clay_scroll_list_test` and
// `clay_scroll_list_check`.

#include "clay_bridge.h"
#include "clay/clay.h"
#include "primitives/scroll_list.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

#include <cstdio>

namespace silencer::clay_bridge {

namespace {

constexpr int kTestItemCount = 30;

// Static storage so the Clay_String pointers stay valid for the duration
// of the layout pass. Each test scene re-uses these slots.
char g_itemText[kTestItemCount][16];
Clay_String g_items[kTestItemCount];

void BuildItems() {
	for(int i = 0; i < kTestItemCount; i++){
		std::snprintf(g_itemText[i], sizeof(g_itemText[i]), "Item %02d", i + 1);
		g_items[i].chars  = g_itemText[i];
		g_items[i].length = 7;
		g_items[i].isStaticallyAllocated = false;
	}
}

}  // namespace

bool RunScrollListTest(::Game & game, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);
	silencer::ui::primitives::ScrollListBeginFrame();
	silencer::ui::primitives::BankTextBeginFrame();
	BuildItems();

	::Clay_BeginLayout();

	// Root: position the list at (50, 50) via padding.
	CLAY({ .id = CLAY_ID("ScrollListTestRoot"),
	       .layout = {
	           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { 50, 0, 50, 0 },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		silencer::ui::primitives::ScrollList(
			CLAY_STRING("list"),
			g_items,
			kTestItemCount,
			/*selectedIndex=*/8,
			/*scrollPosition=*/3,
			{ .width = 200,
			  .height = 130,
			  .lineHeight = 13 });
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

bool RunScrollListCheck(::Game & game, ScrollListCheckResult & out) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);
	BuildItems();

	int fired = 0;
	int lastIndex = -1;
	struct Sink { int * fired; int * lastIndex; };
	Sink sink{&fired, &lastIndex};
	auto onSelect = +[](void * user, int index) {
		auto * s = static_cast<Sink *>(user);
		(*s->fired)++;
		*s->lastIndex = index;
	};

	auto runFrame = [&](float px, float py, bool down) {
		::Clay_SetPointerState(::Clay_Vector2{px, py}, down);
		::Clay_UpdateScrollContainers(false, ::Clay_Vector2{0, 0}, 0.0f);
		::Clay_ResetMeasureTextCache();
		silencer::ui::primitives::ScrollListBeginFrame();
		silencer::ui::primitives::BankTextBeginFrame();

		::Clay_BeginLayout();
		CLAY({ .id = CLAY_ID("ScrollListCheckRoot"),
		       .layout = {
		           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
		           .padding = { 50, 0, 50, 0 },
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			silencer::ui::primitives::ScrollList(
				CLAY_STRING("list"),
				g_items,
				kTestItemCount,
				/*selectedIndex=*/-1,
				/*scrollPosition=*/3,
				{ .width = 200, .height = 130, .lineHeight = 13 },
				{ /*hoveredOut=*/nullptr, onSelect, &sink });
		}
		::Clay_EndLayout();
	};

	// Press over row index 5: at scrollPosition=3, row 5 is the 3rd visible
	// row → top y = 50 + (5-3)*13 = 50 + 26 = 76. Bbox spans y=[76, 89].
	// Click at (60, 82) — well inside the row column (x>=50, x<50+200-8).
	const float px = 60.0f;
	const float py = 82.0f;

	// Frame 1: warm hit-test cache, no pointer.
	runFrame(-1.0f, -1.0f, false);
	// Frame 2: hover only.
	runFrame(px, py, false);
	// Frames 3-4: press transition + dispatch.
	runFrame(px, py, true);
	runFrame(px, py, true);
	// Frame 5: still held — no additional fire.
	runFrame(px, py, true);

	out.onSelectFired = fired;
	out.lastSelectedIndex = lastIndex;
	return true;
}

}  // namespace silencer::clay_bridge
