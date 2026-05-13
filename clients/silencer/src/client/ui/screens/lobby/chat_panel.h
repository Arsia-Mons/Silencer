#ifndef SILENCER_CLIENT_UI_LOBBY_CHAT_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_CHAT_PANEL_H

// Screen-side lobby ChatPanel. Composes the ScrollTextBox (chat scrollback +
// presence list) and TextInput primitives plus a small set of background
// sprites + the channel-name header.
//
// Domain glue lives HERE: draining `world.lobby.chatmessages`, watching
// `presencechanged` / `channelchanged`, and routing the input field's Enter
// to `world.lobby.SendChat`. The primitives stay screen-agnostic.

#include "shared.h"

#include <string>
#include <vector>

class World;
class Resources;

namespace silencer::client_ui::lobby {

struct ChatLine {
	std::string text;
	Uint8       color;       // palette idx; 0 = neutral.
	Uint8       brightness;  // 128 = neutral.
	Uint16      indent;      // px.
};

struct ChatPanelState {
	std::vector<ChatLine> chatLines;
	std::vector<ChatLine> presenceLines;
	Uint16 chatScrollPos     = 0;
	Uint16 presenceScrollPos = 0;
	std::string channel;          // cached channel name, updated on channelchanged.
	char inputBuffer[201]         = {0};  // legacy maxchars = 200 + NUL.
};

// One-time init. Clears state.
void ChatPanelInit(ChatPanelState & state);

// Per-frame pump: drains chatmessages, rebuilds presenceLines on
// presencechanged, caches channel name on channelchanged. Mirrors legacy
// ChatPanel::Tick.
void ChatPanelTick(ChatPanelState & state, World & world);

// Emits the panel subtree. Must be called inside an open Clay layout pass
// AFTER BankTextBeginFrame() + ScrollTextBoxBeginFrame() + TextInputBeginFrame().
void BuildChatPanelTree(ChatPanelState & state,
                        World & world,
                        Resources & resources);

}  // namespace silencer::client_ui::lobby

#endif
