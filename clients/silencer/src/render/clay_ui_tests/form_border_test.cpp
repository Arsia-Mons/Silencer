// Unit test scene for Box's plain (no-halo) stroke path.
//
// `RunFormBorderTest(outPath)` renders a single Box sized 156x93
// (matching the legacy gamecreate form's outer chrome) at root padding
// (243, 87) with a 1-px palette idx 220 stroke into a 640x480 Surface
// and writes a PNG. The committed reference at
// tests/lobby-clay/form_border_test/reference.png is the pinned baseline;
// the rendered output is byte-identical to the pre-Box `FormBorder`
// helper this test used to exercise (same `.border` render commands).

#include "clay_ui_compositor.h"
#include "clay/clay.h"
#include "primitives/box.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

namespace silencer::clay_bridge {

bool RunFormBorderTest(::Game & game, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);

	using silencer::ui::primitives::Box;
	using silencer::ui::primitives::BoxStrokeStyle;

	::Clay_BeginLayout();

	CLAY({ .id = CLAY_ID("FormBorderTestRoot"),
	       .layout = {
	           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { /*left=*/243, /*right=*/0,
	                        /*top=*/87, /*bottom=*/0 },
	       } }) {
		CLAY(Box(BoxStrokeStyle{ /*strokeColor=*/220, /*strokeWidth=*/1 }, {
		         .id = CLAY_ID("FormBorderBox"),
		         .layout = {
		             .sizing = { CLAY_SIZING_FIXED(156),
		                         CLAY_SIZING_FIXED(93) },
		         },
		     })) {}
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

}  // namespace silencer::clay_bridge
