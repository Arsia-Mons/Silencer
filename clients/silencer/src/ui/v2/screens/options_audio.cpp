#include "options_audio.h"

#include "context.h"
#include "node.h"

#include <string>

namespace ui {
namespace v2 {

Node BuildOptionsAudio(const Context & ctx, const OptionsAudioHandlers & handlers, const OptionsAudioState * state)
{
	(void)ctx;
	const std::string title = "Audio Options";
	const int title_x = 320 - ((int)title.size() * 12) / 2;

	auto row = [](int i, const char * label, std::function<void()> handler, int off_idx, int on_idx) {
		const int by = 50 + i * 53;
		const int oy = 137 + i * 53;
		return Group({
			Button(label, ButtonType::B220x33).at(100, (Sint16)by).onClick(std::move(handler)),
			Sprite(6, off_idx).at(420, (Sint16)oy),
			Sprite(6, on_idx).at(450, (Sint16)oy),
		});
	};

	const int mu_off = state ? (state->music ? 12 : 13) : 12;
	const int mu_on  = state ? (state->music ? 15 : 14) : 14;

	return Background(/*bank=*/6, /*index=*/0, {
		Label(title, /*font_bank=*/135, /*font_width=*/12).at((Sint16)title_x, 14),
		row(0, "Music", handlers.on_toggle_music, mu_off, mu_on),
		Button("Save")  .at(-200, 117).onClick(handlers.on_save),
		Button("Cancel").at(  20, 117).onClick(handlers.on_cancel),
	});
}

}  // namespace v2
}  // namespace ui
