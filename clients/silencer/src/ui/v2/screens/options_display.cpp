#include "options_display.h"

#include "context.h"
#include "node.h"

#include <string>

namespace ui {
namespace v2 {

Node BuildOptionsDisplay(const Context & ctx, const OptionsDisplayHandlers & handlers, const OptionsDisplayState * state)
{
	(void)ctx;
	// Title text: "Display Options" centered at y=14. Legacy formula:
	//   x = 320 - (len * textwidth) / 2
	// with textwidth=12, len=15 → x = 320 - 90 = 230.
	const std::string title = "Display Options";
	const int title_x = 320 - ((int)title.size() * 12) / 2;

	// Two toggle rows. Legacy spacing: row y = 50 + i*53 (button anchor),
	// indicator y = 137 + i*53 (Overlay top-left, no chrome offset since
	// off/on sprites are bare Overlays in the legacy path). When `state`
	// is null, indices match the build-time defaults that the preview
	// PPM diff is locked to; when provided, they follow the legacy Tick
	// derivation.
	auto row = [](int i, const char * label, std::function<void()> handler, int off_idx, int on_idx) {
		const int by = 50 + i * 53;
		const int oy = 137 + i * 53;
		return Group({
			Button(label, ButtonType::B220x33).at(100, (Sint16)by).onClick(std::move(handler)),
			Sprite(6, off_idx).at(420, (Sint16)oy),
			Sprite(6, on_idx).at(450, (Sint16)oy),
		});
	};

	const int fs_off  = state ? (state->fullscreen ? 12 : 13)   : 12;
	const int fs_on   = state ? (state->fullscreen ? 15 : 14)   : 14;
	const int ss_off  = state ? (state->scalefilter ? 12 : 13)  : 12;
	const int ss_on   = state ? (state->scalefilter ? 15 : 14)  : 14;

	return Background(/*bank=*/6, /*index=*/0, {
		Label(title, /*font_bank=*/135, /*font_width=*/12).at((Sint16)title_x, 14),
		row(0, "Fullscreen",     handlers.on_toggle_fullscreen,     fs_off, fs_on),
		row(1, "Smooth Scaling", handlers.on_toggle_smooth_scaling, ss_off, ss_on),
		Button("Save")  .at(-200, 117).onClick(handlers.on_save),
		Button("Cancel").at(  20, 117).onClick(handlers.on_cancel),
	});
}

}  // namespace v2
}  // namespace ui
