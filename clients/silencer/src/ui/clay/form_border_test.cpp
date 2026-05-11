// P10 unit test scene for the FormBorder primitive.
//
// `RunFormBorderTest(outPath)` renders a single CLAY block sized 156x93
// (matching the legacy gamecreate form's outer chrome) at root padding
// (243, 87) with `.border = FormBorder()` (1-px palette idx 220 stroke)
// into a 640x480 Surface and writes a PNG. The committed reference at
// tests/lobby-clay/form_border_test/reference.png is the pinned baseline.

#include "clay_bridge.h"
#include "clay/clay.h"
#include "primitives/form_border.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

namespace silencer::clay_bridge {

bool RunFormBorderTest(::Game & game, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);

	::Clay_BeginLayout();

	CLAY({ .id = CLAY_ID("FormBorderTestRoot"),
	       .layout = {
	           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { /*left=*/243, /*right=*/0,
	                        /*top=*/87, /*bottom=*/0 },
	       } }) {
		CLAY({ .id = CLAY_ID("FormBorderBox"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(156),
		                       CLAY_SIZING_FIXED(93) },
		       },
		       .border = silencer::ui::primitives::FormBorder(/*paletteColor=*/220) }) {}
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

}  // namespace silencer::clay_bridge
