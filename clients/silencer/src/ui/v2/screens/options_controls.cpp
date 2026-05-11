#include "options_controls.h"

#include "context.h"
#include "node.h"

#include "resources.h"

#include <string>

namespace ui {
namespace v2 {

Node BuildOptionsControls(const Context & ctx, const OptionsControlsHandlers & handlers)
{
	// Title centered at y=14, textbank=135, textwidth=12. Legacy formula:
	//   x = 320 - (len * textwidth) / 2
	// "Configure Controls" = 18 chars → x = 320 - 108 = 212.
	const std::string title = "Configure Controls";
	const int title_x = 320 - ((int)title.size() * 12) / 2;

	// Preset button's anchor bakes the chrome-offset delta between
	// B220x33 (sprite index 23) and B112x33 (sprite index 28). Legacy
	// math:  x = -30 + spriteoffsetx[6][23] - spriteoffsetx[6][28]
	// — keeps the visible chrome left edge aligned with the row-0 c1
	// button (which is B112x33 at x=-30). The v2 absolute `.at()` path
	// applies the same B220x33 offset at render time, so passing the
	// same anchor reproduces the legacy chrome top-left pixel-exactly.
	const int preset_x = -30 + ctx.resources.spriteoffsetx[6][23]
	                         - ctx.resources.spriteoffsetx[6][28];

	// Per-row factory. slot = i+1 because the preset row occupies slot 0.
	// Legacy: c1 y = slot*53,        x = -30
	//         c2 y = slot*53,        x = 120
	//         keyname/op overlays render no pixels in the static (pre-Tick)
	//         build state — text is empty for keyname; the OR/AND toggle
	//         button is BNONE (no chrome) with empty text. Omit both from
	//         the tree.
	auto row = [](int i) {
		const int slot = i + 1;
		const int by = slot * 53;
		return Group({
			Button("", ButtonType::B112x33).at(-30, (Sint16)by).withKey("ctrl_c1_" + std::to_string(i)),
			Button("", ButtonType::B112x33).at( 120, (Sint16)by).withKey("ctrl_c2_" + std::to_string(i)),
		});
	};

	return Background(/*bank=*/6, /*index=*/0, {
		// Secondary panel sprite (bank 7 frame 7) layered on top of the
		// base backdrop. Anchor (0,0) matches the legacy Overlay default.
		Sprite(7, 7),

		Label(title, /*font_bank=*/135, /*font_width=*/12).at((Sint16)title_x, 14),

		// Preset row: static "Preset:" label at the same x/y as binding row
		// labels, cycle button on the right.
		Label("Preset:", /*font_bank=*/134, /*font_width=*/10).at(80, 95),
		Button("", ButtonType::B220x33).at((Sint16)preset_x, 0)
			.withKey("ctrl_preset")
			.onClick(handlers.on_preset),

		row(0),
		row(1),
		row(2),
		row(3),
		row(4),

		Button("Save")  .at(-200, 117).onClick(handlers.on_save),
		Button("Cancel").at(  20, 117).onClick(handlers.on_cancel),
	});
}

}  // namespace v2
}  // namespace ui
