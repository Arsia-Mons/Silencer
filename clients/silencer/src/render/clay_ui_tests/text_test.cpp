// P4 unit test scene for the Text primitive.
//
// Builds a minimal Clay tree consisting of a 640x480 root with a single
// Text(Title, "Silencer", LegacyPalette(152)) positioned at (15, 32) via
// padding. Runs it through silencer::clay_bridge::Render into a fresh
// Surface and writes the result as PNG. The committed reference at
// tests/lobby-ui/text_test/reference.png is the pinned baseline; the
// run.sh harness pixdiffs against it.
//
// Invoked via the JSON-lines control op `clay_text_test` (immediate
// phase) — see HandleImmediate in net/controldispatch.cpp.

#include "clay_ui_compositor.h"
#include "clay/clay.h"
#include "primitives/text.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

namespace silencer::clay_bridge {

bool RunTextTest(::Game & game, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);
	silencer::ui::primitives::TextBeginFrame();

	::Clay_BeginLayout();

	// Root: full-screen container that positions its sole child at (15, 32)
	// via padding. No background fill — the bridge's bbox-clipped DrawText
	// path is the only thing painting into the Surface.
	CLAY({ .id = CLAY_ID("TextTestRoot"),
	       .layout = {
	           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { 15, 0, 32, 0 },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		silencer::ui::primitives::Text(
			CLAY_STRING("Silencer"),
			{ .size = silencer::ui::primitives::TextSize::Title,
			  .effect = silencer::ui::primitives::TextEffect::LegacyPalette(152) });
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

}  // namespace silencer::clay_bridge
