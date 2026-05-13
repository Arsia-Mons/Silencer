// P10 unit test scene for the LabelValueRow primitive.
//
// `RunLabelValueRowTest(outPath)` renders three LabelValueRow primitives
// stacked vertically inside a TOP_TO_BOTTOM column at root padding
// (247, 90). Mirrors the legacy gamecreate form's 14-px row rhythm and
// label/value column widths (label 76 px, value 80 px, total row 156 px).
// Writes the captured 640x480 PNG to `outPath`.

#include "clay_ui_compositor.h"
#include "clay/clay.h"
#include "primitives/bank_text.h"
#include "primitives/label_value_row.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

namespace silencer::clay_bridge {

bool RunLabelValueRowTest(::Game & game, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);
	silencer::ui::primitives::BankTextBeginFrame();

	::Clay_BeginLayout();

	CLAY({ .id = CLAY_ID("LabelValueRowTestRoot"),
	       .layout = {
	           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { /*left=*/247, /*right=*/0,
	                        /*top=*/90, /*bottom=*/0 },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		silencer::ui::primitives::LabelValueRow(
			CLAY_STRING("row_security"),
			CLAY_STRING("Security:"),
			CLAY_STRING("Public"),
			{ .width = 156, .height = 14, .labelWidth = 76,
			  .variant = silencer::ui::primitives::BankTextVariant::Body });
		silencer::ui::primitives::LabelValueRow(
			CLAY_STRING("row_spectatable"),
			CLAY_STRING("Spectatable:"),
			CLAY_STRING("Yes"),
			{ .width = 156, .height = 14, .labelWidth = 76,
			  .variant = silencer::ui::primitives::BankTextVariant::Body });
		silencer::ui::primitives::LabelValueRow(
			CLAY_STRING("row_password"),
			CLAY_STRING("Password:"),
			CLAY_STRING(""),
			{ .width = 156, .height = 14, .labelWidth = 76,
			  .variant = silencer::ui::primitives::BankTextVariant::Body });
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

}  // namespace silencer::clay_bridge
