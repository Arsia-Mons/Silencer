// Smoke test for the Clay → Surface bridge (P3).
//
// Builds a tiny deterministic Clay tree exercising RECTANGLE, BORDER, TEXT,
// and IMAGE commands, runs it through silencer::clay_bridge::Render, and
// dumps the result to PNG. The committed reference at
// tests/lobby-ui/bridge_smoke/reference.png is the pinned baseline; the
// run.sh harness pixdiffs against it.
//
// Invoked via the JSON-lines control op `clay_bridge_smoke` (immediate
// phase) — see HandleImmediate in net/controldispatch.cpp.

#include "clay_ui_compositor.h"
#include "clay/clay.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

namespace silencer::clay_bridge {

bool RunSmoke(::Game & game, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);

	::Clay_BeginLayout();

	CLAY({ .id = CLAY_ID("SmokeRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = CLAY_PADDING_ALL(32),
	           .childGap = 16,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		// (1) RECTANGLE — solid fill, palette idx 200 in the .r channel.
		CLAY({ .id = CLAY_ID("SmokeRect"),
		       .layout = { .sizing = { CLAY_SIZING_FIXED(120),
		                               CLAY_SIZING_FIXED(24) } },
		       .backgroundColor = { 200, 0, 0, 255 } }) {}

		// (2) BORDER — 4-edge stroke, palette idx 220, no background fill so
		//     Clay emits BORDER only (no RECTANGLE).
		CLAY({ .id = CLAY_ID("SmokeBorder"),
		       .layout = { .sizing = { CLAY_SIZING_FIXED(120),
		                               CLAY_SIZING_FIXED(24) } },
		       .border = { .color = { 220, 0, 0, 255 },
		                   .width = { 2, 2, 2, 2, 0 } } }) {}

		// (3) TEXT — bank 135 (Title), cell width 11, palette idx 152
		//     (the lobby-title effect color).
		CLAY({ .id = CLAY_ID("SmokeText") }) {
			CLAY_TEXT(CLAY_STRING("SILENCER"),
			          CLAY_TEXT_CONFIG({ .textColor = { 152, 0, 0, 255 },
			                             .fontId = 135,
			                             .fontSize = 11 }));
		}

		// (4) IMAGE — bank 7 idx 9 (scrollbar track, ~8x48). Layout sized
		//     to match the sprite's natural extents so the bridge's blit
		//     lands on the same pixels Clay reserved.
		CLAY({ .id = CLAY_ID("SmokeImage"),
		       .layout = { .sizing = { CLAY_SIZING_FIXED(8),
		                               CLAY_SIZING_FIXED(48) } },
		       .image = { .imageData = PackImage(7, 9) } }) {}
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	// Use the static base palette (renderer.palette.GetColors()) instead of
	// game.GetPaletteColors() — the latter is the live dynamic palette which
	// the main menu fades in over ~15 frames, so its RGB values depend on
	// when the smoke op fires after boot. The base palette is constant.
	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

}  // namespace silencer::clay_bridge
