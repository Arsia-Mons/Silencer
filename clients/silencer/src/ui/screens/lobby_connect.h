#ifndef SILENCER_UI_V2_SCREENS_LOBBY_CONNECT_H
#define SILENCER_UI_V2_SCREENS_LOBBY_CONNECT_H

#include "runtime.h"
#include "ui_state.h"

#include <functional>
#include <string>
#include <vector>

class World;
class ScreenContext;

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

// Engine-side runtime for GameState::LOBBYCONNECT. Owns the username +
// password input buffers + textbox status lines + caret-blink phase.
// Tick() runs the legacy LobbyConnectScreen::Tick state machine
// (Connecting -> CheckingVersion -> Authenticating -> Authenticated).
// DispatchKeyDown handles BACKSPACE / RETURN / ESCAPE / TAB editing;
// DispatchTextInput appends typed chars into the active buffer.
class LobbyConnectRuntime : public Runtime
{
public:
	LobbyConnectRuntime(World & world, ScreenContext & sctx);

	void Render(Surface & target, ::Renderer & renderer,
	            int mouse_x, int mouse_y, float dt,
	            int logical_w, int logical_h, int scale) override;
	bool DispatchMouseDown(int mouse_x, int mouse_y,
	                       int logical_w, int logical_h, int scale) override;
	bool DispatchKeyDown(int sdl_scancode) override;
	bool DispatchTextInput(char ascii) override;
	void Tick() override;

private:
	void Submit();
	void Cancel();
	void CycleField();
	void AppendChar(char c);
	void Backspace();

	World &         world_;
	ScreenContext & sctx_;
	UIState         state_;

	char        username_[17] = {0};
	char        password_[29] = {0};
	int         active_field_ = 1;     // 0 = none, 1 = username, 2 = password
	std::vector<std::string> textbox_lines_;
	bool        motd_printed_ = false;
	unsigned    frames_ = 0;           // local caret-blink counter
};

}  // namespace v2
}  // namespace ui

#endif
