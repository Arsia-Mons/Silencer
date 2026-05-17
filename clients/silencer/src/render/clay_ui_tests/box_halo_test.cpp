// C0c unit test scene for the Box primitive's multi-tone stroke halo path.
//
// `RunBoxHaloTest(outPath)` renders a small gallery of halo-bearing Box
// variants against a flat black background into a 640x480 Surface:
// a closed rectangle, the lobby's open-right and open-left shelf shapes,
// and a stitched stepped composition that mirrors the lobby's inner elbow.
//
// This is the visual regression guard for shared halo raster behavior:
// open-sided shelves must keep their lower corners visually contiguous,
// while the canonical closed-rectangle chrome remains unchanged.

#include "clay_ui_compositor.h"
#include "clay/clay.h"
#include "primitives/box.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

namespace silencer::clay_bridge {

namespace {

using silencer::ui::primitives::BoxStrokeStyle;
namespace BoxSides = silencer::ui::primitives::BoxSides;
namespace BoxVariants = silencer::ui::primitives::BoxVariants;

BoxStrokeStyle OpenRightChrome() {
	BoxStrokeStyle style = BoxVariants::Chrome;
	style.sides = static_cast<Uint8>(BoxSides::Top | BoxSides::Bottom | BoxSides::Left);
	return style;
}

BoxStrokeStyle OpenLeftChrome() {
	BoxStrokeStyle style = BoxVariants::Chrome;
	style.sides = static_cast<Uint8>(BoxSides::Top | BoxSides::Bottom | BoxSides::Right);
	return style;
}

BoxStrokeStyle RightEdgeChrome() {
	BoxStrokeStyle style = BoxVariants::Chrome;
	style.sides = BoxSides::Right;
	return style;
}

}  // namespace

bool RunBoxHaloTest(::Game & game, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);

	using silencer::ui::primitives::Box;
	using silencer::ui::primitives::BoxBeginFrame;

	BoxBeginFrame();
	::Clay_BeginLayout();

	CLAY({ .id = CLAY_ID("BoxHaloRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { /*left=*/30, /*right=*/30,
	                        /*top=*/30,  /*bottom=*/30 },
	           .childGap = 30,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("BoxHaloRow"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(130) },
		           .childGap = 20,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			CLAY(Box(BoxVariants::Chrome, {
			         .id = CLAY_ID("HaloBoxClosed"),
			         .layout = {
			             .sizing = { CLAY_SIZING_FIXED(180), CLAY_SIZING_FIXED(130) },
			         },
			     })) {}
			CLAY(Box(OpenRightChrome(), {
			         .id = CLAY_ID("HaloBoxOpenRight"),
			         .layout = {
			             .sizing = { CLAY_SIZING_FIXED(180), CLAY_SIZING_FIXED(130) },
			         },
			     })) {}
			CLAY(Box(OpenLeftChrome(), {
			         .id = CLAY_ID("HaloBoxOpenLeft"),
			         .layout = {
			             .sizing = { CLAY_SIZING_FIXED(180), CLAY_SIZING_FIXED(130) },
			         },
			     })) {}
		}

		CLAY({ .id = CLAY_ID("BoxHaloSteppedWrap"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(400), CLAY_SIZING_FIXED(230) },
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			CLAY({ .id = CLAY_ID("BoxHaloLeftStack"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(250), CLAY_SIZING_GROW(0) },
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       } }) {
				CLAY({ .id = CLAY_ID("BoxHaloUpperRow"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(85) },
				           .childGap = 10,
				           .layoutDirection = CLAY_LEFT_TO_RIGHT,
				       } }) {
					CLAY(Box(BoxVariants::Chrome, {
					         .id = CLAY_ID("HaloCharacterBox"),
					         .layout = {
					             .sizing = { CLAY_SIZING_FIXED(105), CLAY_SIZING_GROW(0) },
					         },
					     })) {}
					CLAY(Box(OpenRightChrome(), {
					         .id = CLAY_ID("HaloUpperShelf"),
					         .layout = {
					             .sizing = { CLAY_SIZING_FIXED(135), CLAY_SIZING_GROW(0) },
					         },
					     })) {}
				}
				CLAY({ .id = CLAY_ID("BoxHaloElbowGapRow"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(10) },
				           .layoutDirection = CLAY_LEFT_TO_RIGHT,
				       } }) {
					CLAY({ .id = CLAY_ID("BoxHaloElbowGapFill"),
					       .layout = {
					           .sizing = { CLAY_SIZING_FIXED(240), CLAY_SIZING_GROW(0) },
					       } }) {}
					CLAY(Box(RightEdgeChrome(), {
					         .id = CLAY_ID("BoxHaloElbowGapSeam"),
					         .layout = {
					             .sizing = { CLAY_SIZING_FIXED(10), CLAY_SIZING_GROW(0) },
					         },
					     })) {}
				}
				CLAY({ .id = CLAY_ID("BoxHaloLowerRow"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
				           .layoutDirection = CLAY_LEFT_TO_RIGHT,
				       } }) {
					CLAY(Box(BoxVariants::Chrome, {
					         .id = CLAY_ID("HaloChatBox"),
					         .layout = {
					             .sizing = { CLAY_SIZING_FIXED(240), CLAY_SIZING_GROW(0) },
					         },
					     })) {}
					CLAY(Box(RightEdgeChrome(), {
					         .id = CLAY_ID("BoxHaloLowerGapSeam"),
					         .layout = {
					             .sizing = { CLAY_SIZING_FIXED(10), CLAY_SIZING_GROW(0) },
					         },
					     })) {}
				}
			}
			CLAY(Box(OpenLeftChrome(), {
			         .id = CLAY_ID("HaloTallBox"),
			         .layout = {
			             .sizing = { CLAY_SIZING_FIXED(150), CLAY_SIZING_GROW(0) },
			         },
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
