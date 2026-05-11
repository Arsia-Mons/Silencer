// C0 unit test scene for the Box primitive.
//
// `RunBoxTest(outPath)` renders five variants in a flex layout into a
// 640x480 Surface and writes a PNG. Variants:
//
//   • Top row, left   — stroke-only (no fill, no halos).
//   • Top row, right  — fill-only (no stroke).
//   • Mid row, left   — fill + stroke (no halos).
//   • Mid row, right  — fill-with-opacity over an existing-color background
//     (C1 bridge alpha-blend path).
//   • Bottom strip    — primary stroke + outer/inner halos (C0c BoxStroke
//     custom-render path). Three side-by-side boxes exercising:
//     halos-only-outer, halos-only-inner, halos-on-both-sides (canonical).

#include "clay_bridge.h"
#include "clay/clay.h"
#include "primitives/box.h"

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
// C0c halo test colors — the canonical lobby chrome palette sampled from
// /tmp/lobby_bg.png. The lobby-canonical values get finalized in C2's
// `lobby-chrome-rectangles.md`; these are the same family, used here to
// exercise the BoxStroke render path with visually-distinct halos against
// the test scene's dark backdrop.
constexpr Uint8 kHaloPrimary       = 216;  // bright lobby-frame green
constexpr Uint8 kHaloOuter         = 75;   // dark green (rgb 8,40,8)
constexpr Uint8 kHaloInner         = 77;   // close-dark green (sampled most often inside)

}  // namespace

bool RunBoxTest(::Game & game, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);

	using silencer::ui::primitives::Box;
	using silencer::ui::primitives::BoxBeginFrame;

	BoxBeginFrame();
	::Clay_BeginLayout();

	CLAY({ .id = CLAY_ID("BoxTestRoot"),
	       .layout = {
	           .sizing  = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { /*left=*/40, /*right=*/40,
	                        /*top=*/40,  /*bottom=*/40 },
	           .childGap = 20,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("BoxTestRow1"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(120) },
		           .childGap = 20,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			Box(CLAY_STRING("VStrokeOnly"),
			    /*width=*/220, /*height=*/120,
			    /*fillPaletteColor=*/0, /*fillOpacity=*/0,
			    /*strokePaletteColor=*/kStrokeColor,
			    /*strokeWidth=*/1,
			    /*strokeOuterHaloColor=*/0, /*strokeOuterHaloWidth=*/0,
			    /*strokeInnerHaloColor=*/0, /*strokeInnerHaloWidth=*/0);
			Box(CLAY_STRING("VFillOnly"),
			    /*width=*/220, /*height=*/120,
			    /*fillPaletteColor=*/kFillColorA, /*fillOpacity=*/255,
			    /*strokePaletteColor=*/0, /*strokeWidth=*/0,
			    /*strokeOuterHaloColor=*/0, /*strokeOuterHaloWidth=*/0,
			    /*strokeInnerHaloColor=*/0, /*strokeInnerHaloWidth=*/0);
		}
		CLAY({ .id = CLAY_ID("BoxTestRow2"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(120) },
		           .childGap = 20,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			Box(CLAY_STRING("VFillStroke"),
			    /*width=*/220, /*height=*/120,
			    /*fillPaletteColor=*/kFillColorB, /*fillOpacity=*/255,
			    /*strokePaletteColor=*/kStrokeColor,
			    /*strokeWidth=*/1,
			    /*strokeOuterHaloColor=*/0, /*strokeOuterHaloWidth=*/0,
			    /*strokeInnerHaloColor=*/0, /*strokeInnerHaloWidth=*/0);
			CLAY({ .id = CLAY_ID("V4Underlying"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) },
			           .padding = CLAY_PADDING_ALL(24),
			       },
			       .backgroundColor = {
			           /*r=*/static_cast<float>(kUnderlyingFill),
			           0.0f, 0.0f, 255.0f } }) {
				Box(CLAY_STRING("V4Overlay"),
				    /*width=*/0, /*height=*/0,
				    /*fillPaletteColor=*/kFillColorA,
				    /*fillOpacity=*/128,
				    /*strokePaletteColor=*/0,
				    /*strokeWidth=*/0,
				    /*strokeOuterHaloColor=*/0, /*strokeOuterHaloWidth=*/0,
				    /*strokeInnerHaloColor=*/0, /*strokeInnerHaloWidth=*/0);
			}
		}
		// Variant 5 — halos exercise the BoxStroke custom-render path.
		// Three boxes: outer-halo-only, inner-halo-only, both-halos.
		CLAY({ .id = CLAY_ID("BoxTestRow3"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(140) },
		           .childGap = 20,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			Box(CLAY_STRING("VHaloOuter"),
			    /*width=*/170, /*height=*/140,
			    /*fillPaletteColor=*/0, /*fillOpacity=*/0,
			    /*strokePaletteColor=*/kHaloPrimary,
			    /*strokeWidth=*/1,
			    /*strokeOuterHaloColor=*/kHaloOuter, /*strokeOuterHaloWidth=*/1,
			    /*strokeInnerHaloColor=*/0,          /*strokeInnerHaloWidth=*/0);
			Box(CLAY_STRING("VHaloInner"),
			    /*width=*/170, /*height=*/140,
			    /*fillPaletteColor=*/0, /*fillOpacity=*/0,
			    /*strokePaletteColor=*/kHaloPrimary,
			    /*strokeWidth=*/1,
			    /*strokeOuterHaloColor=*/0,          /*strokeOuterHaloWidth=*/0,
			    /*strokeInnerHaloColor=*/kHaloInner, /*strokeInnerHaloWidth=*/1);
			Box(CLAY_STRING("VHaloBoth"),
			    /*width=*/170, /*height=*/140,
			    /*fillPaletteColor=*/0, /*fillOpacity=*/0,
			    /*strokePaletteColor=*/kHaloPrimary,
			    /*strokeWidth=*/1,
			    /*strokeOuterHaloColor=*/kHaloOuter, /*strokeOuterHaloWidth=*/1,
			    /*strokeInnerHaloColor=*/kHaloInner, /*strokeInnerHaloWidth=*/1);
		}
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

}  // namespace silencer::clay_bridge
