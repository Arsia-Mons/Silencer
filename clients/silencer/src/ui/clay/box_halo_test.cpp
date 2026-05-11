// C0c unit test scene for the Box primitive's multi-tone stroke halo path.
//
// `RunBoxHaloTest(outPath)` renders a single Box with the canonical lobby
// chrome halo params (primary stroke + 1-px outer + 1-px inner halos)
// against a flat black background into a 640x480 Surface. This is the
// visual proof-of-concept for replacing the legacy BG sprite's
// bright-stroke-bracketed-by-dark-green look with a procedural BoxStroke.
//
// The Box is centered, sized 400x300, no fill — so only the 3-px-thick
// halo stroke chrome shows. Pixdiff bar against the committed reference
// is < 1.0% (per the C0c pass_check; the visual is exactly the chrome
// signature, not pixel-locked to a baked sprite).

#include "clay_bridge.h"
#include "clay/clay.h"
#include "primitives/box.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

namespace silencer::clay_bridge {

namespace {

// Canonical lobby chrome halo palette indices, sampled from
// /tmp/lobby_bg.png at multiple clean rectangle edges:
//   • primary stroke (idx 216, rgb 8,84,0)  — the bright green
//   • outer halo    (idx 75,  rgb 8,40,8)   — dark green one ring out
//   • inner halo    (idx 77,  similar dark) — dark green one ring in
constexpr Uint8 kHaloPrimary = 216;
constexpr Uint8 kHaloOuter   = 75;
constexpr Uint8 kHaloInner   = 77;

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
	           .padding = { /*left=*/120, /*right=*/120,
	                        /*top=*/90,   /*bottom=*/90 },
	       } }) {
		Box(CLAY_STRING("HaloBox"),
		    /*width=*/400, /*height=*/300,
		    /*fillPaletteColor=*/0, /*fillOpacity=*/0,
		    /*strokePaletteColor=*/kHaloPrimary,
		    /*strokeWidth=*/1,
		    /*strokeOuterHaloColor=*/kHaloOuter, /*strokeOuterHaloWidth=*/1,
		    /*strokeInnerHaloColor=*/kHaloInner, /*strokeInnerHaloWidth=*/1);
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	::Renderer & r = game.GetRenderer();
	return r.CapturePNG(dst, r.palette.GetColors(), outPath);
}

}  // namespace silencer::clay_bridge
