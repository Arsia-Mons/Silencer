#ifndef CHAT_PANEL_H
#define CHAT_PANEL_H

#include "panel.h"

// Chat box on the LobbyScreen: channel name overlay, scrollback textbox,
// presence list, chat input, and a scrollbar tying the chat textbox together.
// Drains world.lobby.chatmessages each tick and pushes typed lines to
// world.lobby.SendChat. When the window is minimized and new chat arrives,
// flashes the OS taskbar / dock to draw attention.
class ChatPanel : public Panel
{
public:
	void Build(ScreenContext & ctx, Interface * parent) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;
};

#endif
