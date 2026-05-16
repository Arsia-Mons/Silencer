#include "chat_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/bank_text.h"
#include "primitives/scroll_text_box.h"
#include "primitives/text_input.h"

#include <cstdint>

using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::ScrollTextBox;
using silencer::ui::primitives::ScrollTextBoxLine;
using silencer::ui::primitives::ScrollTextBoxOpts;
using silencer::ui::primitives::ScrollTextBoxOrigin;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputOpts;
using silencer::ui::primitives::TextInputHandle;
using silencer::ui::primitives::BankText;

namespace silencer::client_ui::lobby {

namespace chat_panel_layout_detail {

// Chat panel widget dimensions — preserved from the legacy chat_panel.cpp
// Build. The X/Y origins are now DERIVED from the LobbyChatBox flex layout;
// widget extents are still authoritative inputs to the layout math.
constexpr Uint16 kChatW        = 242;
constexpr Uint16 kChatH        = 207;
constexpr Uint16 kPresW        = 110;
constexpr Uint16 kPresH        = 207;
constexpr Uint8  kLineHeight   = 11;
constexpr Uint8  kFontWidth    = 6;
constexpr Uint16 kInputW       = 360;
constexpr Uint16 kInputH       = 14;
// Sprite bank that holds the lobby's scrollbar track/thumb cells.
constexpr Uint8  kScrollbarBank = 7;

constexpr uint16_t kPanelPad       = 6;
constexpr uint16_t kChannelWrapH   = 20;
constexpr uint16_t kBodyChildGap   = 6;
constexpr uint16_t kInputPadTop    = 10;
constexpr const char * kActionInput = "lobby.chat.input";

// Per-frame Clay_String / ScrollTextBoxLine slabs. The std::strings owned
// by ChatPanelState are pointer-stable across this Build call, so we hand
// out raw c_str()s into them; the slabs themselves only need to outlive
// the central ClientUi frame end.
constexpr int kMaxLines = 256;
ScrollTextBoxLine g_chatSlab[kMaxLines];
ScrollTextBoxLine g_presSlab[kMaxLines];

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars  = s.c_str();
	return cs;
}

int FillSlab(ScrollTextBoxLine * slab, const std::vector<ChatLine> & lines) {
	int count = static_cast<int>(lines.size());
	if(count > kMaxLines) count = kMaxLines;
	for(int i = 0; i < count; i++){
		const ChatLine & ln = lines[i];
		slab[i].text        = FromStd(ln.text);
		slab[i].effectColor = ln.color;
		slab[i].brightness  = ln.brightness;
		slab[i].indent      = ln.indent;
	}
	return count;
}

}  // namespace chat_panel_layout_detail

void BuildChatPanelTree(ChatPanelState & state,
                        World & world,
                        Resources & resources,
                        silencer::ui::UiInteractionRegistry& interactions) {
	// The lobby shell supplies LobbyChatBox chrome. This component is only
	// the chat content tree: channel header, scrollback/presence row, input.
	(void)resources;
	(void)world;

	// Prepare slabs + opts BEFORE the CLAY block so the inside of the
	// CLAY block stays focused on the layout structure.
	ScrollTextBoxOpts chatOpts;
	chatOpts.width               = chat_panel_layout_detail::kChatW;
	chatOpts.height              = chat_panel_layout_detail::kChatH;
	chatOpts.lineHeight          = chat_panel_layout_detail::kLineHeight;
	chatOpts.textVariant         = BankTextVariant::Body;  // bank 133 w6
	chatOpts.origin              = ScrollTextBoxOrigin::BottomUp;
	chatOpts.showScrollbar       = false;
	chatOpts.scrollbarBank       = chat_panel_layout_detail::kScrollbarBank;
	chatOpts.scrollbarTrackIndex = 12;
	chatOpts.scrollbarThumbIndex = 13;
	chatOpts.scrollbarWidth      = 0;
	chatOpts.scrollbarGap        = 0;

	const int chatN = chat_panel_layout_detail::FillSlab(chat_panel_layout_detail::g_chatSlab, state.chatLines);

	ScrollTextBoxOpts presOpts;
	presOpts.width        = chat_panel_layout_detail::kPresW;
	presOpts.height       = chat_panel_layout_detail::kPresH;
	presOpts.lineHeight   = chat_panel_layout_detail::kLineHeight;
	presOpts.textVariant  = BankTextVariant::Body;
	presOpts.origin       = ScrollTextBoxOrigin::TopDown;
	presOpts.showScrollbar = false;

	const int presN = chat_panel_layout_detail::FillSlab(chat_panel_layout_detail::g_presSlab, state.presenceLines);

	// `showCaret=false` mirrors the legacy steady state where the chat
	// interface is not the active interface (gameSelect/etc. are), so
	// `interface.cpp`'s "showcaret = (activeobject == textinput->id)"
	// resolves to false.
	TextInputOpts inOpts;
	inOpts.widthPx     = chat_panel_layout_detail::kInputW;
	inOpts.heightPx    = chat_panel_layout_detail::kInputH;
	inOpts.fontBank    = 133;
	inOpts.fontWidth   = chat_panel_layout_detail::kFontWidth;
	inOpts.brightness  = 128;
	inOpts.showCaret   = false;
	inOpts.inactive    = false;

	CLAY({ .id = CLAY_ID("ChatPanelContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { chat_panel_layout_detail::kPanelPad, chat_panel_layout_detail::kPanelPad, chat_panel_layout_detail::kPanelPad, chat_panel_layout_detail::kPanelPad },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {

		// Channel-header band — fixed height so the body row lands at
		// a stable offset whether or not the channel name has arrived.
		CLAY({ .id = CLAY_ID("ChatChannelWrap"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED((float)chat_panel_layout_detail::kChannelWrapH) },
		       } }) {
			if(!state.channel.empty()){
				BankText(chat_panel_layout_detail::FromStd(state.channel),
				         BankTextVariant::Heading,
				         {});
			}
		}

		// Body row — chat scrollback + presence list side-by-side.
		CLAY({ .id = CLAY_ID("ChatBodyRow"),
		       .layout = {
		           .childGap = chat_panel_layout_detail::kBodyChildGap,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			CLAY({ .id = CLAY_ID("ChatBoxWrap") }) {
				ScrollTextBox(CLAY_STRING("ChatBox"),
				              chat_panel_layout_detail::g_chatSlab,
				              chatN,
				              state.chatScrollPos,
				              chatOpts);
			}
			CLAY({ .id = CLAY_ID("ChatPresWrap") }) {
				ScrollTextBox(CLAY_STRING("ChatPresence"),
				              chat_panel_layout_detail::g_presSlab,
				              presN,
				              state.presenceScrollPos,
				              presOpts);
			}
		}

		// Input row.
		CLAY({ .id = CLAY_ID("ChatInputWrap"),
		       .layout = {
		           .padding = { 0, 0, chat_panel_layout_detail::kInputPadTop, 0 },
		       } }) {
			TextInput(CLAY_STRING("ChatInput"),
			          state.inputBuffer,
			          inOpts,
			          TextInputHandle{ nullptr, chat_panel_layout_detail::kActionInput,
			                           "Chat", &interactions, -1,
			                           static_cast<int>(sizeof(state.inputBuffer)) - 1 });
		}
	}
}

}  // namespace silencer::client_ui::lobby
