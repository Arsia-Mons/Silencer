#ifndef SILENCER_UI_V2_SCREENS_LOBBY_CONNECT_H
#define SILENCER_UI_V2_SCREENS_LOBBY_CONNECT_H

#include <functional>
#include <string>
#include <vector>

namespace ui {
namespace v2 {

struct Node;
struct Context;

struct LobbyConnectHandlers {
	std::function<void()> on_login;
	std::function<void()> on_cancel;
};

// Live state for the engine-wired LOBBYCONNECT render path. Carries the
// username + password buffers, the active-input field, whether the inputs
// are display-inactive (post-AUTHSENT), the caret-blink phase, and the
// textbox status lines accumulated by the lobby state machine. Preview /
// PPM dump leaves this nullptr so the build-time defaults match the legacy
// pre-Tick pixel state byte-for-byte.
struct LobbyConnectState {
	const char * username     = "";
	const char * password     = "";
	int          active_field = 1;     // 0 = none, 1 = username, 2 = password
	bool         inactive     = false; // Sent credentials; inputs disabled.
	bool         caret_visible = true; // legacy: state_i % 32 < 16
	std::vector<std::string> textbox_lines;
};

Node BuildLobbyConnect(const Context & ctx, const LobbyConnectHandlers & handlers = {}, const LobbyConnectState * state = nullptr);

}  // namespace v2
}  // namespace ui

#endif
