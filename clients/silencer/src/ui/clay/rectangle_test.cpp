// C0 unit test scene for the Rectangle primitive.
//
// `RunRectangleTest(outPath)` renders four variants in a 2x2 grid (each
// cell ~240x150 px) into a 640x480 Surface and writes a PNG. Variants:
//
//   • Top-left  — stroke-only (no fill).
//   • Top-right — fill-only (no stroke).
//   • Bot-left  — fill + stroke.
//   • Bot-right — fill-with-opacity over an existing-color background.
//     The underlying parent CLAY block has an opaque .backgroundColor;
//     the inner Rectangle fills the padded area with fillOpacity < 255.
//     Until C1 lands, the bridge treats any non-zero opacity as opaque,
//     so this currently appears as solid fillPaletteColor on top of the
//     underlying — the reference reflects that. C1 will refresh the
//     reference to show the alpha-blended midtone.

#include "clay_bridge.h"
#include "clay/clay.h"
#include "primitives/rectangle.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

namespace silencer::clay_bridge {

namespace {

// Test scene palette indices — arbitrary, picked for visibility against
// black. The primitive carries no opinion on visual identity; this scene
// spells every value explicitly so the test is self-describing.
constexpr Uint8 kStrokeColor       = 17;   // bright-green-ish family
constexpr Uint8 kFillColorA        = 220;  // form-row default elsewhere
constexpr Uint8 kFillColorB        = 152;  // title tint
constexpr Uint8 kUnderlyingFill    = 96;   // dark mid-tone for variant 4

}  // namespace

bool RunRectangleTest(::Game & game, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);

	::Clay_BeginLayout();

	using silencer::ui::primitives::Rectangle;

	CLAY({ .id = CLAY_ID("RectTestRoot"),
	       .layout = {
	           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { /*left=*/60, /*right=*/60,
	                        /*top=*/60,  /*bottom=*/60 },
	           .childGap = 30,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("RectTestRow1"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
		           .childGap = 30,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			Rectangle(CLAY_STRING("VStrokeOnly"),
			          /*width=*/240, /*height=*/150,
			          /*fillPaletteColor=*/0, /*fillOpacity=*/0,
			          /*strokePaletteColor=*/kStrokeColor,
			          /*strokeWidth=*/1);
			Rectangle(CLAY_STRING("VFillOnly"),
			          /*width=*/240, /*height=*/150,
			          /*fillPaletteColor=*/kFillColorA, /*fillOpacity=*/255,
			          /*strokePaletteColor=*/0, /*strokeWidth=*/0);
		}
		CLAY({ .id = CLAY_ID("RectTestRow2"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
		           .childGap = 30,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			Rectangle(CLAY_STRING("VFillStroke"),
			          /*width=*/240, /*height=*/150,
			          /*fillPaletteColor=*/kFillColorB, /*fillOpacity=*/255,
			          /*strokePaletteColor=*/kStrokeColor,
			          /*strokeWidth=*/1);
			CLAY({ .id = CLAY_ID("V4Underlying"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(240), CLAY_SIZING_FIXED(150) },
			           .padding = CLAY_PADDING_ALL(30),
			       },
			       .backgroundColor = {
			           /*r=*/static_cast<float>(kUnderlyingFill),
			           0.0f, 0.0f, 255.0f } }) {
				Rectangle(CLAY_STRING("V4Overlay"),
				          /*width=*/0, /*height=*/0,
				          /*fillPaletteColor=*/kFillColorA,
				          /*fillOpacity=*/128,
				          /*strokePaletteColor=*/0,
				          /*strokeWidth=*/0);
			}
		}
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

}  // namespace silencer::clay_bridge
