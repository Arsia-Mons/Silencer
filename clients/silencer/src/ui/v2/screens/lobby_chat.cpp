#include "lobby_chat.h"

#include "context.h"
#include "node.h"

#include <cmath>

namespace ui {
namespace v2 {

namespace {

// Legacy ChatPanel::Build values — kept here as named constants so the
// rendering math reads as a 1:1 mirror.
constexpr Uint16 CHAT_TEXTBOX_X      = 19;
constexpr Uint16 CHAT_TEXTBOX_Y      = 220;
constexpr Uint16 CHAT_TEXTBOX_W      = 242;
constexpr Uint16 CHAT_TEXTBOX_H      = 207;
constexpr Uint8  CHAT_LINEHEIGHT     = 11;
constexpr Uint8  CHAT_FONT_BANK      = 133;
constexpr Uint8  CHAT_FONT_WIDTH     = 6;
constexpr Uint16 PRESENCE_X          = 267;
constexpr Uint16 PRESENCE_Y          = 220;
constexpr Uint16 PRESENCE_H          = 207;
constexpr Uint16 CHAT_INPUT_X        = 18;
constexpr Uint16 CHAT_INPUT_Y        = 437;
constexpr Uint16 CHAT_INPUT_HEIGHT   = 14;
constexpr Uint8  CHAT_CARET_COLOR    = 140;

}  // namespace

Node BuildChatPanel(const Context & ctx, const ChatPanelState & state, bool chat_active)
{
	(void)ctx;
	std::vector<Node> children;
	children.reserve(8);
	children.push_back(Sprite(/*bank=*/7, /*index=*/11).at(0, 0));
	children.push_back(Sprite(/*bank=*/7, /*index=*/14).at(0, 0));

	// Channel name overlay (legacy uid 1, bank 134, fontwidth 8). Empty
	// at preview gate -> no Label emitted.
	if(!state.channel_name.empty()){
		children.push_back(
			Label(state.channel_name, /*font_bank=*/134, /*font_width=*/8)
				.at(15, 200));
	}

	// Chat scrollback (bottomtotop=true). Mirror Renderer::Draw's
	// TEXTBOX branch (renderer.cpp:919): iterate from `chat_scrolled`,
	// stop after height/lineheight rows, apply (height - capped_size)
	// y offset where capped_size uses the FULL deque size.
	if(!state.chat_lines.empty()){
		const int max_visible_rows = CHAT_TEXTBOX_H / CHAT_LINEHEIGHT;
		int size = (int)state.chat_lines.size() * CHAT_LINEHEIGHT;
		if(size > CHAT_TEXTBOX_H){
			size = (int)std::ceil(float(CHAT_TEXTBOX_H) / CHAT_LINEHEIGHT) * CHAT_LINEHEIGHT;
		}
		const int bottomtotop_offset = CHAT_TEXTBOX_H - size;
		int line = 0;
		for(size_t i = 0; i < state.chat_lines.size(); i++){
			if((Uint16)i < state.chat_scrolled) continue;
			if(line > max_visible_rows) break;
			const ChatLine & cl = state.chat_lines[i];
			if(!cl.text.empty()){
				int y = CHAT_TEXTBOX_Y + line * CHAT_LINEHEIGHT + bottomtotop_offset;
				children.push_back(
					Label(cl.text, CHAT_FONT_BANK, CHAT_FONT_WIDTH)
						.at(CHAT_TEXTBOX_X, y)
						.withColor(cl.color)
						.withBrightness(cl.brightness));
			}
			line++;
		}
	}

	// Presence list (bottomtotop=false). Each line at y = presencebox.y + row*lineheight.
	if(!state.presence_lines.empty()){
		const int max_visible_rows = PRESENCE_H / CHAT_LINEHEIGHT;
		int line = 0;
		for(size_t i = 0; i < state.presence_lines.size(); i++){
			if(line > max_visible_rows) break;
			const ChatLine & cl = state.presence_lines[i];
			if(!cl.text.empty()){
				int y = PRESENCE_Y + line * CHAT_LINEHEIGHT;
				children.push_back(
					Label(cl.text, CHAT_FONT_BANK, CHAT_FONT_WIDTH)
						.at(PRESENCE_X, y)
						.withColor(cl.color)
						.withBrightness(cl.brightness));
			}
			line++;
		}
	}

	// Chat input text. Render the visible window (text + scrolled). When
	// chat is not the active object the legacy DrawTextInput dims with
	// effectbrightness=64.
	if(state.chat_input.size() > state.chat_input_scrolled){
		std::string visible = state.chat_input.substr(state.chat_input_scrolled);
		children.push_back(
			Label(visible, CHAT_FONT_BANK, CHAT_FONT_WIDTH)
				.at(CHAT_INPUT_X, CHAT_INPUT_Y)
				.withBrightness(chat_active ? 128 : 64));
	}

	// Caret. Only when chat is the active sub-interface AND the blink
	// phase is on. Position tracks the visible-window length so the
	// caret stays at the input's right edge once scrolling kicks in.
	if(chat_active && state.caret_visible){
		int visible_len = (int)state.chat_input.size() - (int)state.chat_input_scrolled;
		if(visible_len < 0) visible_len = 0;
		int caret_x = CHAT_INPUT_X + visible_len * CHAT_FONT_WIDTH;
		int caret_y = CHAT_INPUT_Y - 1;
		int caret_h = (int)(CHAT_INPUT_HEIGHT * 0.8f);  // matches DrawTextInput
		children.push_back(FilledRect(/*w=*/1, /*h=*/(Uint16)caret_h, /*color=*/CHAT_CARET_COLOR)
			.at((Sint16)caret_x, (Sint16)caret_y));
	}

	return Group(std::move(children));
}

}  // namespace v2
}  // namespace ui
