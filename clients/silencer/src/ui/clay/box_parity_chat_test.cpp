// C0d Box vs legacy chat-box parity scene. Renders a single Box matching
// the legacy lobby chat-box outer stroke rectangle: visual rect at
// (10, 195, 378, 260) in the 640x480 lobby BG, surrounded by a 6 px black
// margin. The output (390x272) lines up byte-for-byte with the legacy
// crop produced by `/tmp/crop_lobby_chat_box.py` so the two can be DM'd
// side-by-side. Halo params come from sampling the legacy stroke at all
// four edges of this exact rectangle — see
// docs/plans/lobby-chrome-rectangles.md.
//
// This scene does not assert a pixdiff against a reference: it IS the
// visual diff being shipped to the user for review.

#include "clay_bridge.h"
#include "clay/clay.h"
#include "primitives/box.h"

#include "game.h"
#include "palette.h"
#include "renderer.h"
#include "surface.h"

namespace silencer::clay_bridge {

namespace {

// Sampled from /tmp/lobby_bg.png at all four edges of the chat-box outer
// stroke (visual rect at (10, 195, 378, 260) — i.e. NOT the panel's
// internal interface rect, which is (15, 216, 368, 234)). Primary idx
// 216 was unanimous (23/23 sample points). Outer/inner halo are the
// dominant non-stroke neighbors at offset ±1 on the LEFT and BOTTOM
// edges (74/77). The RIGHT edge halo is anomalous (66 inner, 90 outer)
// — likely a sprite-adjacency artifact where the character panel
// touches the chat-box. The canonical pick is left/bottom — those are
// the chat-box's "clean" edges.
constexpr Uint8 kHaloPrimary = 216;
constexpr Uint8 kHaloOuter   = 75;
constexpr Uint8 kHaloInner   = 77;

// Visual rect dimensions in the legacy BG (see comment above).
constexpr int kChatBoxW = 378;
constexpr int kChatBoxH = 260;

// 6 px margin around the rect so halos are clearly framed in the crop.
constexpr int kMargin = 6;

}  // namespace

bool RunBoxParityChatTest(::Game & game, const char * outPath) {
	const int W = kChatBoxW + 2 * kMargin;  // 390
	const int H = kChatBoxH + 2 * kMargin;  // 272
	EnsureInitialized(W, H);

	// The legacy BG sprite is decoded with palette block 2; idx 216/75/77
	// only resolve to the lobby's signature greens when palette 2 is
	// active. Other test scenes leave the startup palette (block 0) where
	// 216 reads as red — fine for those, but parity here requires the
	// lobby palette.
	::Renderer & r = game.GetRenderer();
	r.palette.SetPalette(2);

	using silencer::ui::primitives::Box;
	using silencer::ui::primitives::BoxBeginFrame;

	BoxBeginFrame();
	::Clay_BeginLayout();

	CLAY({ .id = CLAY_ID("BoxParityChatRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(W), CLAY_SIZING_FIXED(H) },
	           .padding = { /*left=*/kMargin, /*right=*/kMargin,
	                        /*top=*/kMargin, /*bottom=*/kMargin },
	       } }) {
		Box(CLAY_STRING("ParityChatBox"),
		    /*width=*/kChatBoxW, /*height=*/kChatBoxH,
		    /*fillPaletteColor=*/0, /*fillOpacity=*/0,
		    /*strokePaletteColor=*/kHaloPrimary,
		    /*strokeWidth=*/1,
		    /*strokeOuterHaloColor=*/kHaloOuter, /*strokeOuterHaloWidth=*/1,
		    /*strokeInnerHaloColor=*/kHaloInner, /*strokeInnerHaloWidth=*/1);
	}

	::Clay_RenderCommandArray cmds = ::Clay_EndLayout();

	Surface dst(W, H, /*clearcolor=*/0);
	Render(game, &dst, cmds);

	bool ok = r.CapturePNG(dst, r.palette.GetColors(), outPath);
	// Restore the startup palette so subsequent control-op tests in the
	// same daemon session see their expected colors.
	r.palette.SetPalette(0);
	return ok;
}

}  // namespace silencer::clay_bridge
