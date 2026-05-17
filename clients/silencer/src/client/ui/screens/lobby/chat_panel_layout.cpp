#include "chat_panel.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/text.h"
#include "primitives/scroll_text_box.h"
#include "primitives/text_input.h"

#include <SDL3/SDL.h>

#include <cstdint>

using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::ScrollTextBox;
using silencer::ui::primitives::ScrollTextBoxLine;
using silencer::ui::primitives::ScrollTextBoxOpts;
using silencer::ui::primitives::ScrollTextBoxOrigin;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputOpts;
using silencer::ui::primitives::TextInputHandle;
using silencer::ui::primitives::Text;
using silencer::clay_bridge::PackImage;

namespace silencer::client_ui::lobby {

namespace chat_panel_layout_detail {

// Legacy ChatInterface geometry inside the chat-box outer frame. The
// surrounding LobbyChatBox already matches the outer 378x260 chrome;
// this component recreates the inner 368x234 chat interface within it.
constexpr Uint16 kInterfaceW    = 368;
constexpr Uint16 kInterfaceH    = 234;
constexpr Uint16 kChatW        = 242;
constexpr Uint16 kChatH        = 207;
constexpr Uint16 kPresW        = 110;
constexpr Uint16 kPresH        = 207;
constexpr Uint8  kLineHeight   = 11;
constexpr Uint16 kInputW       = 360;
constexpr Uint16 kInputH       = 14;
constexpr Uint16 kRootPadLeft   = 5;
constexpr Uint16 kRootPadTop    = 5;
constexpr Uint16 kChannelWrapH  = 16;
constexpr Uint16 kBodyOffsetX   = 4;
constexpr Uint16 kBodyOffsetY   = 4;
constexpr Uint16 kBodyGap       = 6;
constexpr Uint16 kInputOffsetX  = 3;
constexpr Uint16 kInputOffsetY  = 221;
constexpr Uint16 kBodyW         = kChatW + kBodyGap + kPresW;
constexpr Uint8  kChromeBank    = 7;
constexpr Uint16 kChatBorderIdx = 11;
constexpr Uint16 kInputBorderIdx = 14;
constexpr int    kChatInputUid  = 1;
// Sprite bank that holds the lobby's scrollbar track/thumb cells.
constexpr Uint8  kScrollbarBank = 7;
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
		slab[i].effect      = TextEffect::LegacyPalette(ln.color, ln.brightness);
		slab[i].indent      = ln.indent;
	}
	return count;
}

}  // namespace chat_panel_layout_detail

void BuildChatPanelTree(ChatPanelState & state,
                        World & world,
                        Resources & resources,
                        silencer::ui::UiInteractionRegistry& interactions) {
	// The lobby shell supplies the outer 378x260 frame. This component
	// recreates the legacy 368x234 ChatInterface inside that frame.
	(void)resources;
	(void)world;

	// Prepare slabs + opts BEFORE the CLAY block so the inside of the
	// CLAY block stays focused on the layout structure.
	ScrollTextBoxOpts chatOpts;
	chatOpts.width               = chat_panel_layout_detail::kChatW;
	chatOpts.height              = chat_panel_layout_detail::kChatH;
	chatOpts.lineHeight          = chat_panel_layout_detail::kLineHeight;
	chatOpts.text.size           = TextSize::Body;
	chatOpts.origin              = ScrollTextBoxOrigin::BottomUp;
	chatOpts.showScrollbar       = true;
	chatOpts.scrollbarBank       = chat_panel_layout_detail::kScrollbarBank;
	chatOpts.scrollbarTrackIndex = 12;
	chatOpts.scrollbarThumbIndex = 13;
	chatOpts.scrollbarWidth      = 8;
	chatOpts.scrollbarGap        = 0;

	const int chatN = chat_panel_layout_detail::FillSlab(chat_panel_layout_detail::g_chatSlab, state.chatLines);

	ScrollTextBoxOpts presOpts;
	presOpts.width        = chat_panel_layout_detail::kPresW;
	presOpts.height       = chat_panel_layout_detail::kPresH;
	presOpts.lineHeight   = chat_panel_layout_detail::kLineHeight;
	presOpts.text.size    = TextSize::Body;
	presOpts.origin       = ScrollTextBoxOrigin::TopDown;
	presOpts.showScrollbar = false;

	const int presN = chat_panel_layout_detail::FillSlab(chat_panel_layout_detail::g_presSlab, state.presenceLines);

	// Mirror the legacy caret rule: no focus means no caret, and a focused
	// field blinks on the old 32-tick cadence.
	TextInputOpts inOpts;
	inOpts.widthPx     = chat_panel_layout_detail::kInputW;
	inOpts.heightPx    = chat_panel_layout_detail::kInputH;
	inOpts.textSize    = TextSize::Body;
	inOpts.showCaret   = interactions.IsTextInputFocused(chat_panel_layout_detail::kChatInputUid)
	                  && ((SDL_GetTicks() / 50) % 32 < 16);
	inOpts.inactive    = false;

	CLAY({ .id = CLAY_ID("ChatPanelContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { chat_panel_layout_detail::kRootPadLeft, 0,
	                        chat_panel_layout_detail::kRootPadTop, 0 },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	           .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
	       } }) {

		CLAY({ .id = CLAY_ID("ChatChannelWrap"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIT(0),
		                       CLAY_SIZING_FIXED((float)chat_panel_layout_detail::kChannelWrapH) },
		       } }) {
			if(!state.channel.empty()){
				Text(chat_panel_layout_detail::FromStd(state.channel),
				     { .size = TextSize::Heading });
			}
		}

		CLAY({ .id = CLAY_ID("ChatInterfaceChrome"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED((float)chat_panel_layout_detail::kInterfaceW),
		                       CLAY_SIZING_FIXED((float)chat_panel_layout_detail::kInterfaceH) },
		       },
		       .image = { .imageData = PackImage(chat_panel_layout_detail::kChromeBank,
		                                        chat_panel_layout_detail::kChatBorderIdx) } }) {
			CLAY({ .id = CLAY_ID("ChatInputBorderOverlay"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED((float)chat_panel_layout_detail::kInterfaceW),
			                       CLAY_SIZING_FIXED((float)chat_panel_layout_detail::kInterfaceH) },
			       },
			       .floating = {
			           .offset = { 0, 0 },
			           .attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_TOP,
			                             .parent = CLAY_ATTACH_POINT_LEFT_TOP },
			           .attachTo = CLAY_ATTACH_TO_PARENT,
			       },
			       .image = { .imageData = PackImage(chat_panel_layout_detail::kChromeBank,
			                                        chat_panel_layout_detail::kInputBorderIdx) } }) {}

			CLAY({ .id = CLAY_ID("ChatBodyRow"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED((float)chat_panel_layout_detail::kBodyW),
			                       CLAY_SIZING_FIXED((float)chat_panel_layout_detail::kChatH) },
			           .childGap = chat_panel_layout_detail::kBodyGap,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       },
		       .floating = {
			           .offset = { (int16_t)chat_panel_layout_detail::kBodyOffsetX,
			                       (int16_t)chat_panel_layout_detail::kBodyOffsetY },
			           .attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_TOP,
			                             .parent = CLAY_ATTACH_POINT_LEFT_TOP },
			           .attachTo = CLAY_ATTACH_TO_PARENT,
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

			CLAY({ .id = CLAY_ID("ChatInputWrap"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED((float)chat_panel_layout_detail::kInputW),
			                       CLAY_SIZING_FIXED((float)chat_panel_layout_detail::kInputH) },
			       },
			       .floating = {
			           .offset = { (int16_t)chat_panel_layout_detail::kInputOffsetX,
			                       (int16_t)chat_panel_layout_detail::kInputOffsetY },
			           .attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_TOP,
			                             .parent = CLAY_ATTACH_POINT_LEFT_TOP },
			           .attachTo = CLAY_ATTACH_TO_PARENT,
			       } }) {
				TextInput(CLAY_STRING("ChatInput"),
				          state.inputBuffer,
				          inOpts,
				          TextInputHandle{ nullptr, chat_panel_layout_detail::kActionInput,
				                           "Chat", &interactions,
				                           chat_panel_layout_detail::kChatInputUid,
				                           static_cast<int>(sizeof(state.inputBuffer)) - 1 });
			}
		}
	}
}

}  // namespace silencer::client_ui::lobby
