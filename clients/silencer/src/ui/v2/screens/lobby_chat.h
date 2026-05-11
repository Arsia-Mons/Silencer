#ifndef SILENCER_UI_V2_SCREENS_LOBBY_CHAT_H
#define SILENCER_UI_V2_SCREENS_LOBBY_CHAT_H

#include "shared.h"

#include <deque>
#include <string>
#include <vector>

namespace ui {
namespace v2 {

struct Node;
struct Context;

// One coloured line in the chat scrollback or presence textbox. Mirrors
// the trailing color/brightness bytes the legacy TextBox::AddLine packs
// into each std::vector<char>.
struct ChatLine {
	std::string text;
	Uint8 color = 0;
	Uint8 brightness = 128;
};

// Engine-supplied dynamic state for the chat panel (channel name overlay,
// scrollback, presence list, chat input). All fields default to empty so
// the preview-gate render stays byte-identical with the legacy preview.
struct ChatPanelState {
	// Channel-name overlay (legacy uid 1, bank 134, fontwidth 8).
	std::string channel_name;
	// Scrollback. Mirrors the legacy TextBox::text deque + scrolled
	// offset semantics: render only entries with index >= scrolled,
	// stop after height/lineheight rows. bottomtotop math uses the
	// FULL deque size — so we pass the full snapshot here and let the
	// builder apply both scrolled and the bottomtotop offset.
	std::deque<ChatLine> chat_lines;
	Uint16 chat_scrolled = 0;
	// Presence list (legacy uid 9 textbox, bottomtotop=false). Re-built
	// in TickLobbyV2 whenever world.lobby.presencechanged is set.
	std::vector<ChatLine> presence_lines;
	// Chat input — full typed string + visible-window offset that
	// mirrors TextInput::scrolled (advances when length >= maxwidth+scrolled).
	std::string chat_input;
	Uint16 chat_input_scrolled = 0;
	// Caret blink phase. True at preview gate (legacy state_i=0 → 0%32<16
	// is true) so the default keeps preview byte-identical.
	bool caret_visible = true;
};

// Chat panel: chrome (bank 7 idx 11 + 14) + channel-name overlay +
// scrollback + presence list + chat input + caret.
//
// `chat_active` controls whether the chat input is the active object of
// the lobby's sub-interface. When false (right-side panel like
// game_create owns focus), the input text dims and the caret rect is
// suppressed. Mirrors the legacy ShowGameCreate/ShowGameTech ->
// chatiface->activeobject = 0 -> ActiveChanged -> chatinput.showcaret = false
// chain. Default true for the standalone preview.
Node BuildChatPanel(const Context & ctx, const ChatPanelState & state = {},
                    bool chat_active = true);

}  // namespace v2
}  // namespace ui

#endif
