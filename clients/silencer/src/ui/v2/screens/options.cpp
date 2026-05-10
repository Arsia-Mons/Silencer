#include "options.h"

#include "context.h"
#include "node.h"

namespace ui {
namespace v2 {

Node BuildOptions(const Context & ctx, const OptionsHandlers & handlers)
{
	(void)ctx;
	// Absolute `.at()` mirrors legacy OptionsScreen::Build exactly. The four
	// buttons stack at evenly-spaced y values (52 px between anchors, with
	// chrome height 33 → 19 px visible gap) so a VStack with .withGap(19)
	// could reproduce the *spacing*, but it can't reproduce the *anchor*
	// origin without baking in the sprite offset for bank-6 index-7 — and
	// that offset is loaded at runtime from PALETTE/sprite metadata. Per
	// the design doc's escape hatch, falling back to absolute keeps the
	// port pixel-identical with no runtime fragility.
	return Background(/*bank=*/6, /*index=*/0, {
		Button("Controls").at(-89, -142).onClick(handlers.on_controls),
		Button("Display") .at(-89,  -90).onClick(handlers.on_display),
		Button("Audio")   .at(-89,  -38).onClick(handlers.on_audio),
		Button("Go Back") .at(-89,   15).onClick(handlers.on_go_back),
	});
}

}  // namespace v2
}  // namespace ui
