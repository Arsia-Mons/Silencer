#include "chat_panel.h"

#include "clay/clay.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/box.h"
#include "primitives/text.h"
#include "primitives/scroll_text_box.h"
#include "primitives/text_input.h"

#include <SDL3/SDL.h>

#include <cstdint>

using silencer::ui::primitives::Box;
using silencer::ui::primitives::ScrollTextBox;
using silencer::ui::primitives::ScrollTextBoxLine;
using silencer::ui::primitives::ScrollTextBoxOpts;
using silencer::ui::primitives::ScrollTextBoxOrigin;
using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputHandle;
using silencer::ui::primitives::TextInputOpts;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::TextWrap;
namespace BoxVariants = silencer::ui::primitives::BoxVariants;

namespace silencer::client_ui::lobby {

namespace chat_panel_layout_detail {

constexpr Uint8  kLineHeight = 11;
constexpr Uint16 kRootPadX = 5;
constexpr Uint16 kRootPadTop = 5;
constexpr Uint16 kRootPadBottom = 5;
constexpr Uint16 kChannelWrapH = 16;
constexpr Uint16 kMainGap = 4;
constexpr Uint16 kBodyGap = 6;
constexpr Uint16 kBodyPadLeft = 4;
constexpr Uint16 kBodyPadRight = 4;
constexpr Uint16 kBodyPadTop = 4;
constexpr Uint16 kBodyPadBottom = 2;
constexpr Uint16 kInputBorderH = 17;
constexpr Uint16 kInputPadX = 3;
constexpr Uint16 kInputPadTop = 1;
constexpr Uint16 kInputPadBottom = 2;
constexpr int    kChatInputUid = 1;
constexpr Uint8  kScrollbarBank = 7;
constexpr const char * kActionInput = "lobby.chat.input";

constexpr int kMaxLines = 256;
ScrollTextBoxLine g_chatSlab[kMaxLines];
ScrollTextBoxLine g_presSlab[kMaxLines];

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars = s.c_str();
	return cs;
}

int FillSlab(ScrollTextBoxLine * slab, const std::vector<ChatLine> & lines) {
	int count = static_cast<int>(lines.size());
	if(count > kMaxLines) count = kMaxLines;
	for(int i = 0; i < count; i++){
		const ChatLine & ln = lines[i];
		slab[i].text = FromStd(ln.text);
		slab[i].effect = TextEffect::LegacyPalette(ln.color, ln.brightness);
		slab[i].indent = ln.indent;
	}
	return count;
}

}  // namespace chat_panel_layout_detail

void BuildChatPanelTree(ChatPanelState & state,
                        World & world,
                        Resources & resources,
                        Uint16 panelWidth,
                        Uint16 panelHeight,
                        silencer::ui::UiInteractionRegistry& interactions) {
	(void)resources;
	(void)world;

	ChatPanelSyncLayout(state, panelWidth, panelHeight);
	const ChatPanelLayoutMetrics metrics =
		ResolveChatPanelLayout(panelWidth, panelHeight);

	ScrollTextBoxOpts chatOpts;
	chatOpts.width = metrics.chatWidth;
	chatOpts.height = metrics.chatHeight;
	chatOpts.lineHeight = chat_panel_layout_detail::kLineHeight;
	chatOpts.text.size = TextSize::Body;
	chatOpts.text.wrap = TextWrap::None;
	chatOpts.origin = ScrollTextBoxOrigin::BottomUp;
	chatOpts.showScrollbar = true;
	chatOpts.scrollbarBank = chat_panel_layout_detail::kScrollbarBank;
	chatOpts.scrollbarTrackIndex = 12;
	chatOpts.scrollbarThumbIndex = 13;
	chatOpts.scrollbarWidth = 8;
	chatOpts.scrollbarGap = 0;

	const int chatN = chat_panel_layout_detail::FillSlab(
		chat_panel_layout_detail::g_chatSlab,
		state.chatLines);

	ScrollTextBoxOpts presOpts;
	presOpts.width = metrics.presenceWidth;
	presOpts.height = metrics.presenceHeight;
	presOpts.lineHeight = chat_panel_layout_detail::kLineHeight;
	presOpts.text.size = TextSize::Body;
	presOpts.text.wrap = TextWrap::None;
	presOpts.origin = ScrollTextBoxOrigin::TopDown;
	presOpts.showScrollbar = false;

	const int presN = chat_panel_layout_detail::FillSlab(
		chat_panel_layout_detail::g_presSlab,
		state.presenceLines);

	TextInputOpts inOpts;
	inOpts.widthPx = metrics.inputWidth;
	inOpts.heightPx = metrics.inputHeight;
	inOpts.textSize = TextSize::Body;
	inOpts.showCaret = interactions.IsTextInputFocused(chat_panel_layout_detail::kChatInputUid)
		&& ((SDL_GetTicks() / 50) % 32 < 16);
	inOpts.inactive = false;

	CLAY({ .id = CLAY_ID("ChatPanelContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { chat_panel_layout_detail::kRootPadX,
	                        chat_panel_layout_detail::kRootPadX,
	                        chat_panel_layout_detail::kRootPadTop,
	                        chat_panel_layout_detail::kRootPadBottom },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("ChatChannelRow"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED((float)chat_panel_layout_detail::kChannelWrapH) },
		       } }) {
			if(!state.channel.empty()){
				Text(chat_panel_layout_detail::FromStd(state.channel),
				     { .size = TextSize::Heading });
			}
		}

		CLAY(Box(BoxVariants::Plain, {
		         .id = CLAY_ID("ChatMainBorder"),
		         .layout = {
		             .sizing = { CLAY_SIZING_GROW(0),
		                         CLAY_SIZING_GROW(0) },
		             .padding = { chat_panel_layout_detail::kBodyPadLeft,
		                          chat_panel_layout_detail::kBodyPadRight,
		                          chat_panel_layout_detail::kBodyPadTop,
		                          chat_panel_layout_detail::kBodyPadBottom },
		             .childGap = chat_panel_layout_detail::kBodyGap,
		             .layoutDirection = CLAY_LEFT_TO_RIGHT,
		         },
		     })) {
			CLAY({ .id = CLAY_ID("ChatBoxWrap") }) {
				ScrollTextBox(CLAY_STRING("ChatBox"),
				              chat_panel_layout_detail::g_chatSlab,
				              chatN,
				              state.chatScrollPos,
				              chatOpts);
			}
			if(metrics.presenceWidth > 0){
				CLAY({ .id = CLAY_ID("ChatPresWrap") }) {
					ScrollTextBox(CLAY_STRING("ChatPresence"),
					              chat_panel_layout_detail::g_presSlab,
					              presN,
					              state.presenceScrollPos,
					              presOpts);
				}
			}
		}

		CLAY({ .id = CLAY_ID("ChatInputGap"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED((float)chat_panel_layout_detail::kMainGap) },
		       } }) {}

		CLAY(Box(BoxVariants::Plain, {
		         .id = CLAY_ID("ChatInputBorder"),
		         .layout = {
		             .sizing = { CLAY_SIZING_GROW(0),
		                         CLAY_SIZING_FIXED((float)chat_panel_layout_detail::kInputBorderH) },
		             .padding = { chat_panel_layout_detail::kInputPadX,
		                          chat_panel_layout_detail::kInputPadX,
		                          chat_panel_layout_detail::kInputPadTop,
		                          chat_panel_layout_detail::kInputPadBottom },
		             .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
		         },
		     })) {
			CLAY({ .id = CLAY_ID("ChatInputWrap") }) {
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
