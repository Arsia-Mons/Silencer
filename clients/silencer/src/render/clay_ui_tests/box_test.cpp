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

#include "clay_ui_compositor.h"
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
// Halo test palette indices — sampled from the legacy lobby BG sprite.
constexpr Uint8 kHaloPrimary       = 216;  // bright lobby-frame green
constexpr Uint8 kHaloOuter         = 75;   // dark green (rgb ~8, 40, 8)
constexpr Uint8 kHaloInner         = 77;   // close-dark green

}  // namespace

bool RunBoxTest(::Game & game, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);

	using silencer::ui::primitives::Box;
	using silencer::ui::primitives::BoxBeginFrame;
	using silencer::ui::primitives::BoxStrokeStyle;

	// Per-variant stroke styles. Test scenes spell these out (no
	// `BoxVariants::*` lookup) because each row deliberately exercises
	// a non-canonical configuration (outer-only halo, inner-only halo,
	// etc.) that no design-system variant should encode. Halo bands
	// share the primary color; only their width and the shared opacity
	// vary across variants.
	const BoxStrokeStyle plain{
		/*strokeColor=*/kStrokeColor, /*strokeWidth=*/1,
	};
	const BoxStrokeStyle noStroke{};
	const BoxStrokeStyle haloOuterOnly{
		/*strokeColor=*/kHaloPrimary, /*strokeWidth=*/1,
		/*outerHaloColor=*/kHaloOuter, /*outerHaloWidth=*/1,
		/*innerHaloColor=*/0, /*innerHaloWidth=*/0,
	};
	const BoxStrokeStyle haloInnerOnly{
		/*strokeColor=*/kHaloPrimary, /*strokeWidth=*/1,
		/*outerHaloColor=*/0, /*outerHaloWidth=*/0,
		/*innerHaloColor=*/kHaloInner, /*innerHaloWidth=*/1,
	};
	const BoxStrokeStyle haloBoth{
		/*strokeColor=*/kHaloPrimary, /*strokeWidth=*/1,
		/*outerHaloColor=*/kHaloOuter, /*outerHaloWidth=*/1,
		/*innerHaloColor=*/kHaloInner, /*innerHaloWidth=*/1,
	};

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
			CLAY(Box(plain, {
			         .id = CLAY_ID("VStrokeOnly"),
			         .layout = { .sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) } },
			     })) {}
			CLAY(Box(noStroke, {
			         .id = CLAY_ID("VFillOnly"),
			         .layout = { .sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) } },
			         .backgroundColor = { (float)kFillColorA, 0.0f, 0.0f, 255.0f },
			     })) {}
		}
		CLAY({ .id = CLAY_ID("BoxTestRow2"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(120) },
		           .childGap = 20,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			CLAY(Box(plain, {
			         .id = CLAY_ID("VFillStroke"),
			         .layout = { .sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) } },
			         .backgroundColor = { (float)kFillColorB, 0.0f, 0.0f, 255.0f },
			     })) {}
			CLAY({ .id = CLAY_ID("V4Underlying"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) },
			           .padding = CLAY_PADDING_ALL(24),
			       },
			       .backgroundColor = {
			           /*r=*/static_cast<float>(kUnderlyingFill),
			           0.0f, 0.0f, 255.0f } }) {
				CLAY(Box(noStroke, {
				         .id = CLAY_ID("V4Overlay"),
				         .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } },
				         .backgroundColor = { (float)kFillColorA, 0.0f, 0.0f, 128.0f },
				     })) {}
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
			CLAY(Box(haloOuterOnly, {
			         .id = CLAY_ID("VHaloOuter"),
			         .layout = { .sizing = { CLAY_SIZING_FIXED(170), CLAY_SIZING_FIXED(140) } },
			     })) {}
			CLAY(Box(haloInnerOnly, {
			         .id = CLAY_ID("VHaloInner"),
			         .layout = { .sizing = { CLAY_SIZING_FIXED(170), CLAY_SIZING_FIXED(140) } },
			     })) {}
			CLAY(Box(haloBoth, {
			         .id = CLAY_ID("VHaloBoth"),
			         .layout = { .sizing = { CLAY_SIZING_FIXED(170), CLAY_SIZING_FIXED(140) } },
			     })) {}
		}
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

}  // namespace silencer::clay_bridge
