// C1 unit test scene for the Rectangle primitive's alpha-blend bridge path.
//
// `RunRectangleAlphaTest(outPath)` renders a fully-opaque red rect with a
// 50%-opaque blue rect overlapping its right half, into a 640x480 Surface
// and writes a PNG. The left half of the red rect shows the pure red
// palette index; the overlap on the right half shows the alphaed-LUT blend
// between red and blue.
//
// First-cut: any non-255 opacity quantizes to ≈50% via ramp position 8
// (see `Palette::Calculate`). The reference.png is regenerated from the
// live binary the first time the test runs with `REGEN=1`; once committed,
// pixdiff against it < 0.5%.

#include "clay_bridge.h"
#include "clay/clay.h"
#include "primitives/rectangle.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

namespace silencer::clay_bridge {

namespace {

// Three palette indices: a neutral backdrop and two distinct-ramp colors
// for the overlapping rects. The backdrop is non-zero on purpose — the
// alphaed LUT only populates entries for src/dst in the standard range
// [2, 226), so a translucent rect over palette index 0 collapses to 0
// (intentional in the LUT; a known quirk callers must respect).
constexpr Uint8 kBackdrop  = 7;    // mid-grey backdrop
constexpr Uint8 kRedColor  = 164;  // mid-red family
constexpr Uint8 kBlueColor = 132;  // mid-blue family

}  // namespace

bool RunRectangleAlphaTest(::Game & game, const char * outPath) {
	const int W = 640;
	const int H = 480;
	EnsureInitialized(W, H);

	::Clay_BeginLayout();

	using silencer::ui::primitives::Rectangle;

	// Full-screen backdrop + two overlapping rects. The blue rect's left
	// edge lands at the red rect's horizontal midpoint so its left half
	// overlaps the red's right half. Three distinct regions to inspect:
	// (1) pure red on backdrop, (2) red+blue overlap blend, (3) blue on
	// backdrop.
	CLAY({ .id = CLAY_ID("RectAlphaRoot"),
	       .layout = { .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) } } }) {
		CLAY({ .id = CLAY_ID("Backdrop"),
		       .floating = { .offset = { 0, 0 },
		                     .attachTo = CLAY_ATTACH_TO_ROOT } }) {
			Rectangle(CLAY_STRING("VBackdrop"),
			          /*width=*/W, /*height=*/H,
			          /*fillPaletteColor=*/kBackdrop,
			          /*fillOpacity=*/255,
			          /*strokePaletteColor=*/0, /*strokeWidth=*/0);
		}
		CLAY({ .id = CLAY_ID("RedHost"),
		       .floating = { .offset = { 160, 160 },
		                     .attachTo = CLAY_ATTACH_TO_ROOT } }) {
			Rectangle(CLAY_STRING("VRedOpaque"),
			          /*width=*/320, /*height=*/160,
			          /*fillPaletteColor=*/kRedColor,
			          /*fillOpacity=*/255,
			          /*strokePaletteColor=*/0, /*strokeWidth=*/0);
		}
		CLAY({ .id = CLAY_ID("BlueHost"),
		       .floating = { .offset = { 320, 160 },
		                     .attachTo = CLAY_ATTACH_TO_ROOT } }) {
			Rectangle(CLAY_STRING("VBlueHalf"),
			          /*width=*/320, /*height=*/160,
			          /*fillPaletteColor=*/kBlueColor,
			          /*fillOpacity=*/128,
			          /*strokePaletteColor=*/0, /*strokeWidth=*/0);
		}
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

}  // namespace silencer::clay_bridge
