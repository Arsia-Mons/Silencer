#include "lobby_connect.h"

#include "context.h"
#include "node.h"

namespace ui {
namespace v2 {

Node BuildLobbyConnect(const Context & ctx, const LobbyConnectHandlers & handlers)
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
	return Background(/*bank=*/7, /*index=*/2, {
		Label("Username", /*font_bank=*/134, /*font_width=*/9).at(190, 291),
		Label("Password", /*font_bank=*/134, /*font_width=*/9).at(190, 318),
		Button("Login",  ButtonType::B52x21).at(264, 339).onClick(handlers.on_login),
		Button("Cancel", ButtonType::B52x21).at(321, 339).onClick(handlers.on_cancel),
		// Caret for active username input. Legacy:
		//   dstrect = (x=275+0, y=293-1, w=1, h=14*0.8f=11), color=140.
		FilledRect(1, 11, 140).at(275, 292),
	});
}

}  // namespace v2
}  // namespace ui
