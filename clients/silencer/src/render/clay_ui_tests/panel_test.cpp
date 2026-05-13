// P10 unit test scenes for the Panel primitive.
//
// `RunPanelTest("right", outPath)` renders a RightChrome Panel
// (bank 7 idx 8, sized 222x390 to match the legacy lobby right pane)
// at root padding (403, 87) with title "Game Options" into a 640x480
// Surface and writes a PNG.
//
// `RunPanelTest("bare", outPath)` renders a LeftBare Panel (sized
// 240x390 to match the legacy left column) at root padding (15, 87)
// with title "Character" into the same surface. No chrome sprite is
// emitted — only the title overlays the cleared background.

#include "clay_ui_compositor.h"
#include "clay/clay.h"
#include "primitives/bank_text.h"
#include "primitives/panel.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

#include <cstring>

namespace silencer::clay_bridge {

namespace {

bool VariantIsBare(const char * variant) {
	return variant && std::strcmp(variant, "bare") == 0;
}

}  // namespace

bool RunPanelTest(::Game & game, const char * variant, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);
	silencer::ui::primitives::BankTextBeginFrame();

	const bool bare = VariantIsBare(variant);
	const int padLeft = bare ? 15  : 403;
	const int padTop  = bare ? 87  : 87;
	const Uint16 panelW = bare ? 240 : 222;
	const Uint16 panelH = bare ? 390 : 390;
	const auto panelVariant = bare
		? silencer::ui::primitives::PanelVariant::LeftBare
		: silencer::ui::primitives::PanelVariant::RightChrome;
	Clay_String title = bare
		? CLAY_STRING("Character")
		: CLAY_STRING("Game Options");

	::Clay_BeginLayout();

	CLAY({ .id = CLAY_ID("PanelTestRoot"),
	       .layout = {
	           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { /*left=*/static_cast<Uint16>(padLeft),
	                        /*right=*/0,
	                        /*top=*/static_cast<Uint16>(padTop),
	                        /*bottom=*/0 },
	       } }) {
		silencer::ui::primitives::Panel(
			CLAY_STRING("PanelInstance"),
			panelVariant,
			{ .width        = panelW,
			  .height       = panelH,
			  .chromeBank   = 7,  // Legacy lobby right-pane chrome.
			  .chromeIndex  = 8,
			  .title        = title,
			  .titleVariant = silencer::ui::primitives::BankTextVariant::Heading,
			  .titlePadLeft = 2,
			  .titlePadTop  = 0 });
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

}  // namespace silencer::clay_bridge
