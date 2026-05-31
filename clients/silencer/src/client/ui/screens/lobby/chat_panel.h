#ifndef SILENCER_CLIENT_UI_LOBBY_CHAT_PANEL_H
#define SILENCER_CLIENT_UI_LOBBY_CHAT_PANEL_H

// Screen-side lobby ChatPanel state. The cppx lobby view reads these cached
// lines and the screen lifecycle owns the lobby chat mutations.
//
// Domain glue lives HERE: draining `world.lobby.chatmessages`, watching
// `presencechanged` / `channelchanged`, and routing the input field's Enter
// to `world.lobby.SendChat`. The primitives stay screen-agnostic.

#include "shared.h"
#include "runtime/UiActionQueue.h"

#include <string>
#include <vector>

class World;

namespace silencer::client_ui::lobby {

struct ChatEntry {
	std::string text;
	Uint8       color;                     // palette idx; 0 = neutral.
	Uint8       brightness;                // 128 = neutral.
	Uint8       continuationIndentSpaces;  // spaces inserted after soft wraps.
};

struct ChatLine {
	std::string text;
	Uint8       color;       // palette idx; 0 = neutral.
	Uint8       brightness;  // 128 = neutral.
	Uint16      indent;      // px.
};

struct ChatPanelLayoutMetrics {
	Uint16 rootPadX = 5;
	Uint16 rootPadTop = 5;
	Uint16 rootPadBottom = 5;
	Uint16 channelHeight = 16;
	Uint16 mainGap = 4;
	Uint16 bodyGap = 6;
	Uint16 bodyPadLeft = 4;
	Uint16 bodyPadRight = 4;
	Uint16 bodyPadTop = 4;
	Uint16 bodyPadBottom = 2;
	Uint16 inputBorderHeight = 17;
	Uint16 inputPadX = 3;
	Uint16 inputPadTop = 1;
	Uint16 inputPadBottom = 2;
	Uint16 mainBorderWidth = 0;
	Uint16 mainBorderHeight = 0;
	Uint16 chatWidth = 0;
	Uint16 chatHeight = 0;
	Uint16 presenceWidth = 0;
	Uint16 presenceHeight = 0;
	Uint16 inputWidth = 0;
	Uint16 inputHeight = 14;
	Uint16 chatWrapChars = 1;
	Uint16 presenceWrapChars = 1;
};

struct ChatPanelState {
	std::vector<ChatEntry> chatEntries;
	std::vector<ChatEntry> presenceEntries;
	std::vector<ChatLine> chatLines;
	std::vector<ChatLine> presenceLines;
	Uint16 chatScrollPos     = 0;
	Uint16 presenceScrollPos = 0;
	Uint16 chatWrapChars = 0;
	Uint16 presenceWrapChars = 0;
	Uint16 chatViewportHeight = 0;
	bool chatWrapDirty = true;
	bool presenceWrapDirty = true;
	std::string channel;          // cached channel name, updated on channelchanged.
	char inputBuffer[201]         = {0};  // legacy maxchars = 200 + NUL.
};

// One-time init. Clears state.
void ChatPanelInit(ChatPanelState & state);

// Per-frame pump: drains chatmessages, rebuilds presenceLines on
// presencechanged, caches channel name on channelchanged. Mirrors legacy
// ChatPanel::Tick.
void ChatPanelTick(ChatPanelState & state, World & world);
bool ChatPanelHandleUiIntent(ChatPanelState & state,
                             World & world,
                             const silencer::ui::UiAction & action);
void ChatPanelSetInput(ChatPanelState & state, const std::string & value);
ChatPanelLayoutMetrics ResolveChatPanelLayout(Uint16 panelWidth,
                                              Uint16 panelHeight);
void ChatPanelSyncLayout(ChatPanelState & state,
                         Uint16 panelWidth,
                         Uint16 panelHeight);

}  // namespace silencer::client_ui::lobby

#endif
