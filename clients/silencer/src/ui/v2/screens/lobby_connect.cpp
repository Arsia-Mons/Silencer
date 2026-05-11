#include "lobby_connect.h"

#include "context.h"
#include "node.h"

#include <cstring>
#include <string>

namespace ui {
namespace v2 {

Node BuildLobbyConnect(const Context & ctx, const LobbyConnectHandlers & handlers, const LobbyConnectState * state)
{
	(void)ctx;
	// Mirrors LobbyConnectScreen::Build (clients/silencer/src/ui/screens/
	// lobby_connect/lobby_connect_screen.cpp). At preview gate state
	// (post-Build, after iface->ActiveChanged(mouse=false), pre-Tick) the
	// rendered pixels are:
	//   - background sprite (bank=7, idx=2)
	//   - "Username" / "Password" Overlay text (bank=134, width=9)
	//   - Login / Cancel button text (B52x21: no chrome, text only)
	//   - The active username TextInput's caret (1×11 rect, color 140) —
	//     iface->ActiveChanged sets showcaret=true on the active input;
	//     renderer.cpp:1797 draws it because state_i % 32 < 16 holds with
	//     state_i==0 (preview gate skips renderer.Tick).
	// TextBox/TextInput sprites + Interface itself contribute no pixels
	// (empty text, res_bank=0xFF/135 with res_index=0 = empty glyph).
	std::vector<Node> children;
	children.push_back(Label("Username", /*font_bank=*/134, /*font_width=*/9).at(190, 291));
	children.push_back(Label("Password", /*font_bank=*/134, /*font_width=*/9).at(190, 318));
	children.push_back(Button("Login",  ButtonType::B52x21).at(264, 339).onClick(handlers.on_login));
	children.push_back(Button("Cancel", ButtonType::B52x21).at(321, 339).onClick(handlers.on_cancel));

	if(state == nullptr){
		// Preview / PPM dump path — replicate legacy pre-Tick output exactly:
		// the username field is the activeobject and its caret is drawn at
		// offset 0 (empty buffer).
		children.push_back(FilledRect(1, 11, 140).at(275, 292));
	}else{
		// Live engine path. Inputs render their own text + caret; status
		// lines accumulated by Game::TickLobbyConnectV2 render as Labels at
		// the legacy textbox (x=185, y=101, lineheight=11, fontwidth=6,
		// bank=133).
		const char * u = state->username ? state->username : "";
		const char * p = state->password ? state->password : "";
		size_t ulen = std::strlen(u);
		size_t plen = std::strlen(p);
		// Effect-brightness 64 mirrors the legacy `if(inactive) brightness = 64`
		// path in Renderer::DrawTextInput.
		Uint8 b = state->inactive ? 64u : 128u;
		if(ulen > 0){
			children.push_back(
				Label(std::string(u), /*font_bank=*/133, /*font_width=*/6)
				.at(275, 293).withBrightness(b));
		}
		if(plen > 0){
			std::string masked(plen, '*');
			children.push_back(
				Label(masked, /*font_bank=*/133, /*font_width=*/6)
				.at(275, 320).withBrightness(b));
		}
		if(!state->inactive && state->caret_visible){
			if(state->active_field == 1){
				int cx = 275 + (int)ulen * 6;
				children.push_back(FilledRect(1, 11, 140).at((Sint16)cx, 292));
			}else if(state->active_field == 2){
				int cx = 275 + (int)plen * 6;
				children.push_back(FilledRect(1, 11, 140).at((Sint16)cx, 319));
			}
		}
		// Textbox lines (no scrolling — line count is small in practice).
		int line = 0;
		int max_lines = 170 / 11;  // legacy textbox height/lineheight
		for(const auto & s : state->textbox_lines){
			if(line >= max_lines) break;
			if(!s.empty()){
				children.push_back(Label(s, /*font_bank=*/133, /*font_width=*/6)
					.at(185, (Sint16)(101 + line * 11)));
			}
			line++;
		}
	}
	return Background(/*bank=*/7, /*index=*/2, std::move(children));
}

}  // namespace v2
}  // namespace ui
