#include "password.h"

#include "context.h"
#include "node.h"

#include "resources.h"

#include <cstring>
#include <string>

namespace ui {
namespace v2 {

Node BuildPassword(const Context & ctx,
                   const std::string & password_text,
                   const PasswordHandlers & handlers)
{
	// Background sprite (bank=40, idx=2) — the password-dialog backdrop.
	// Legacy positions it centered around (320, 240) based on the runtime
	// sprite dimensions; mirror that here so the v2 build matches pixel-for-pixel.
	const int bg_w = (int)ctx.resources.spritewidth[40][2];
	const int bg_h = (int)ctx.resources.spriteheight[40][2];
	const Sint16 bg_x = (Sint16)(320 - bg_w / 2);
	const Sint16 bg_y = (Sint16)(240 - bg_h / 2);

	// Prompt label — centered horizontally on the dialog using the same
	// `320 - (len * fontwidth) / 2` formula the legacy modal uses.
	static const char prompt[] = "This game requires a password";
	const int prompt_len = (int)(sizeof(prompt) - 1);  // 29
	const Sint16 prompt_x = (Sint16)(320 - (prompt_len * 8) / 2);
	const Sint16 prompt_y = 196;

	// Password input glyphs (masked as '*'). PasswordModal::Build does NOT
	// call iface->ActiveChanged, so the legacy TextInput keeps its default
	// showcaret=false — no caret renders at preview-gate time. Live-game
	// wiring (P16) will add the caret when activeobject focus is plumbed
	// through this builder.
	const Sint16 input_x         = 210;
	const Sint16 input_y         = 243;
	const Uint8  input_fontwidth = 11;
	std::string masked(password_text.size(), '*');

	std::vector<Node> children;
	children.push_back(
		Label(prompt, /*font_bank=*/134, /*font_width=*/8).at(prompt_x, prompt_y));
	if(!masked.empty()){
		children.push_back(
			Label(masked, /*font_bank=*/135, /*font_width=*/input_fontwidth)
				.at(input_x, input_y));
	}
	children.push_back(Button("OK", ButtonType::B156x21)
	                       .at(242, 267)
	                       .onClick([handlers, password_text](){
		                       if(handlers.on_submit) handlers.on_submit(password_text);
	                       }));

	return Background(/*bank=*/40, /*index=*/2, std::move(children)).at(bg_x, bg_y);
}

}  // namespace v2
}  // namespace ui
