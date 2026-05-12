#include "clay_chat_panel.h"

#include "clay/clay.h"
#include "clay_bridge.h"
#include "clay_inspector.h"
#include "primitives/bank_text.h"
#include "primitives/form_border.h"
#include "primitives/scroll_text_box.h"
#include "primitives/text_input.h"

#include "lobby.h"
#include "lobbygame.h"
#include "resources.h"
#include "world.h"

#include <algorithm>
#include <cstring>

using silencer::ui::primitives::BankText;
using silencer::ui::primitives::BankTextVariant;
using silencer::ui::primitives::FormBorder;
using silencer::ui::primitives::ScrollTextBox;
using silencer::ui::primitives::ScrollTextBoxLine;
using silencer::ui::primitives::ScrollTextBoxOpts;
using silencer::ui::primitives::ScrollTextBoxOrigin;
using silencer::ui::primitives::ScrollTextBoxAutoScroll;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputOpts;
using silencer::ui::primitives::TextInputHandle;

namespace silencer::ui::lobby_clay {

namespace {

// Chat panel widget dimensions — preserved from the legacy chat_panel.cpp
// Build. The X/Y origins are now DERIVED from the LobbyChatBox flex layout
// (see kBox*/kChannelWrap*/kBodyPad*/kInput* below); widget extents are
// still authoritative inputs to the layout math.
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

// LobbyChatBox geometry — matches the C2-documented "Chat-box outer frame"
// rectangle in the baked BG (docs/plans/lobby-chrome-rectangles.md).
// The Box floats @ ROOT at the legacy stroke origin; its children lay out
// via flex (TOP_TO_BOTTOM: channel header → body row → input) so the
// channel-text / chat-scrollback / presence-list / input positions
// emerge from layout rather than from absolute @ROOT offsets.
constexpr int   kBoxX             = 10;
constexpr int   kBoxY             = 195;
constexpr int   kBoxW             = 378;
constexpr int   kBoxH             = 260;
constexpr Uint8 kBoxStrokeColor   = 216;
// Inside-box layout knobs — derived from the legacy on-screen widget coords:
//   channel @ (15, 200), chat @ (19, 220), presence @ (267, 220), input @ (18, 437).
// Box has border=1 + pad{left=4,top=4} → content top-left lands at (15, 200).
// Channel header wrapper is FIXED height 20 so the body row starts at y=220
// regardless of whether the channel name has arrived yet (lobby may not
// have sent MSG_CHANNEL before the first Build). Body row has padLeft=4 so
// chat lands at x=19; childGap=6 places presence at x=19+242+6=267. Input
// wrapper has padTop=10 so it lands at y=220+207+10=437 and padLeft=3 so
// it lands at x=18.
constexpr int kBoxPadLeft        = 4;
constexpr int kBoxPadTop         = 4;
constexpr int kChannelWrapH      = 20;
constexpr int kBodyPadLeft       = 4;
constexpr int kBodyChildGap      = 6;
constexpr int kInputPadLeft      = 3;
constexpr int kInputPadTop       = 10;
// Legacy on-screen coords kept ONLY for inspector hit-rect registration —
// the CLAY inspector dispatch is label-based; the rect is used as a
// fallback for geometric hit-testing and for human-readable layout dumps.
constexpr int kInspectorInputX   = 18;
constexpr int kInspectorInputY   = 437;

// Per-frame Clay_String / ScrollTextBoxLine slabs. The std::strings owned
// by ChatPanelState are pointer-stable across this Build call, so we hand
// out raw c_str()s into them; the slabs themselves only need to outlive
// Clay_EndLayout.
constexpr int kMaxLines = 256;
ScrollTextBoxLine g_chatSlab[kMaxLines];
ScrollTextBoxLine g_presSlab[kMaxLines];

Clay_String FromCStr(const char * s, size_t len) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(len);
	cs.chars  = s;
	return cs;
}

Clay_String FromStd(const std::string & s) {
	return FromCStr(s.c_str(), s.size());
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

// Rebuild presenceLines from world.lobby.presence — mirrors the legacy
// rebuild block in chat_panel.cpp.
void RebuildPresence(ChatPanelState & state, World & world) {
	state.presenceLines.clear();
	state.presenceScrollPos = 0;

	struct Row { Uint8 group; std::string label; };
	std::vector<Row> rows;
	rows.reserve(world.lobby.presence.size());
	for(auto & kv : world.lobby.presence){
		Lobby::PresenceEntry & e = kv.second;
		Row r;
		r.label = e.name;
		r.group = (e.status <= 2) ? e.status : 0;
		if(e.gameid != 0){
			LobbyGame * g = world.lobby.GetGameById(e.gameid);
			if(g){
				r.label += " [";
				r.label += g->name;
				r.label += "]";
			}
		}
		rows.push_back(std::move(r));
	}
	std::sort(rows.begin(), rows.end(), [](const Row & a, const Row & b){
		if(a.group != b.group) return a.group < b.group;
		return a.label < b.label;
	});

	Uint8 lastgroup = 255;
	for(auto & r : rows){
		if(r.group != lastgroup){
			const char * header = (r.group == 0) ? "In Lobby"
			                    : (r.group == 1) ? "Pregame" : "Playing";
			ChatLine h;
			h.text       = header;
			h.color      = 0;
			h.brightness = 128 + 32;
			h.indent     = 0;
			state.presenceLines.push_back(std::move(h));
			lastgroup = r.group;
		}
		ChatLine nm;
		nm.text       = r.label;
		nm.color      = 0;
		nm.brightness = 128;
		nm.indent     = 2;
		state.presenceLines.push_back(std::move(nm));
	}
}

}  // namespace

void ChatPanelInit(ChatPanelState & state) {
	state.chatLines.clear();
	state.presenceLines.clear();
	state.chatScrollPos = 0;
	state.presenceScrollPos = 0;
	state.channel.clear();
	state.inputBuffer[0] = '\0';
}

void ChatPanelTick(ChatPanelState & state, World & world) {
	// Channel name — legacy ChatPanel::Tick captures the lobby's first
	// channel name and snapshots it into `lastchannel` so GoBack can
	// rejoin. Mirror that here so we don't break the GoBack-rejoin path
	// when the legacy chat panel isn't built.
	if(world.lobby.channelchanged){
		if(world.lobby.lastchannel[0] == '\0'){
			strcpy(world.lobby.lastchannel, world.lobby.channel);
		}
		state.channel = world.lobby.channel;
		world.lobby.channelchanged = false;
	}

	// Presence — rebuild on change or first pass before gamesprocessed.
	if(world.lobby.presencechanged || !world.lobby.gamesprocessed){
		RebuildPresence(state, world);
		world.lobby.presencechanged = false;
	}

	// Chat messages — drain into chatLines. Auto-scroll if we were
	// already pinned to the bottom. Use the ScrollTextBoxAutoScroll
	// helper for the canonical "stay pinned to bottom" computation.
	while(!world.lobby.chatmessages.empty()){
		auto message = world.lobby.chatmessages.front();
		const char * msgtext = message.data();
		size_t msglen        = strlen(msgtext);
		Uint8 color          = static_cast<Uint8>(message[msglen + 1]);
		Uint8 brightness     = static_cast<Uint8>(message[msglen + 2]);

		const int prevCount = static_cast<int>(state.chatLines.size());
		ChatLine cl;
		cl.text       = msgtext;
		cl.color      = color;
		cl.brightness = brightness;
		cl.indent     = 2;
		state.chatLines.push_back(std::move(cl));
		const int newCount = static_cast<int>(state.chatLines.size());

		state.chatScrollPos = ScrollTextBoxAutoScroll(
			state.chatScrollPos, prevCount, newCount,
			kLineHeight, kChatH);

		world.lobby.chatmessages.pop_front();
	}
}

void BuildChatPanelTree(ChatPanelState & state,
                        World & world,
                        Resources & resources) {
	// LobbyChatBox — Clay-drawn chrome around the chat region. Replaces
	// the legacy `ChatBorder` (bank 7 idx 11) and `ChatInputBorder` (bank
	// 7 idx 14) IMAGE sprites. The Box's flex layout positions the
	// channel header, the chat-scrollback / presence-list body row, and
	// the chat input via padding + childGap; no inner element positions
	// itself absolutely.
	(void)resources;

	// Prepare slabs + opts BEFORE the CLAY block so the inside of the
	// CLAY block stays focused on the layout structure.
	ScrollTextBoxOpts chatOpts;
	chatOpts.width               = kChatW;
	chatOpts.height              = kChatH;
	chatOpts.lineHeight          = kLineHeight;
	chatOpts.textVariant         = BankTextVariant::Body;  // bank 133 w6
	chatOpts.origin              = ScrollTextBoxOrigin::BottomUp;
	chatOpts.showScrollbar       = false;
	chatOpts.scrollbarBank       = kScrollbarBank;
	chatOpts.scrollbarTrackIndex = 12;
	chatOpts.scrollbarThumbIndex = 13;
	chatOpts.scrollbarWidth      = 0;
	chatOpts.scrollbarGap        = 0;

	const int chatN = FillSlab(g_chatSlab, state.chatLines);

	ScrollTextBoxOpts presOpts;
	presOpts.width        = kPresW;
	presOpts.height       = kPresH;
	presOpts.lineHeight   = kLineHeight;
	presOpts.textVariant  = BankTextVariant::Body;
	presOpts.origin       = ScrollTextBoxOrigin::TopDown;
	presOpts.showScrollbar = false;

	const int presN = FillSlab(g_presSlab, state.presenceLines);

	// `showCaret=false` mirrors the legacy steady state where the chat
	// interface is not the active interface (gameSelect/etc. are), so
	// `interface.cpp`'s "showcaret = (activeobject == textinput->id)"
	// resolves to false.
	TextInputOpts inOpts;
	inOpts.widthPx     = kInputW;
	inOpts.heightPx    = kInputH;
	inOpts.fontBank    = 133;
	inOpts.fontWidth   = kFontWidth;
	inOpts.brightness  = 128;
	inOpts.showCaret   = false;
	inOpts.inactive    = false;

	// Right/bottom padding stay 0: the presence list bleeds within 1 px of
	// the right stroke to match the baked BG (legacy presence ends at x=377,
	// box right stroke at x=378); the input row bleeds within ~4 px of the
	// bottom stroke (handled by kInputPadTop accounting).
	CLAY({ .id = CLAY_ID("LobbyChatBox"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED((float)kBoxW),
	                       CLAY_SIZING_FIXED((float)kBoxH) },
	           .padding = { /*left=*/(uint16_t)kBoxPadLeft, /*right=*/0,
	                        /*top=*/(uint16_t)kBoxPadTop,  /*bottom=*/0 },
	           .childGap = 0,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       },
	       .floating = { .offset   = { (float)kBoxX, (float)kBoxY },
	                     .attachTo = CLAY_ATTACH_TO_ROOT },
	       .border = FormBorder(kBoxStrokeColor) }) {

		// Channel-header band — fixed height so the body row lands at
		// y=220 whether or not the channel name has arrived from the
		// lobby. When empty the wrap is just blank space.
		CLAY({ .id = CLAY_ID("ChatChannelWrap"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED((float)kChannelWrapH) },
		       } }) {
			if(!state.channel.empty()){
				BankText(FromStd(state.channel),
				         BankTextVariant::Heading,
				         {});
			}
		}

		// Body row — chat scrollback + presence list side-by-side. The
		// row's padLeft=4 + childGap=6 reproduces the legacy x-offsets
		// (chat at 19, presence at 267).
		CLAY({ .id = CLAY_ID("ChatBodyRow"),
		       .layout = {
		           .padding = { (uint16_t)kBodyPadLeft, 0, 0, 0 },
		           .childGap = (uint16_t)kBodyChildGap,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			CLAY({ .id = CLAY_ID("ChatBoxWrap") }) {
				ScrollTextBox(CLAY_STRING("ChatBox"),
				              g_chatSlab,
				              chatN,
				              state.chatScrollPos,
				              chatOpts);
			}
			CLAY({ .id = CLAY_ID("ChatPresWrap") }) {
				ScrollTextBox(CLAY_STRING("ChatPresence"),
				              g_presSlab,
				              presN,
				              state.presenceScrollPos,
				              presOpts);
			}
		}

		// Input row — padTop=10 + padLeft=3 lands the 360x14 input at
		// the legacy (18, 437) on-screen pixel.
		CLAY({ .id = CLAY_ID("ChatInputWrap"),
		       .layout = {
		           .padding = { (uint16_t)kInputPadLeft, 0,
		                        (uint16_t)kInputPadTop,  0 },
		       } }) {
			TextInput(CLAY_STRING("ChatInput"),
			          state.inputBuffer,
			          inOpts,
			          TextInputHandle{ /*hoveredOut*/ nullptr,
			                           /*onEnter*/    nullptr,
			                           /*user*/       nullptr });
		}
	}

	{
		silencer::ui::clay_inspector::Widget w;
		w.label = "Chat";
		w.kind  = silencer::ui::clay_inspector::WidgetKind::TextInput;
		w.x = kInspectorInputX; w.y = kInspectorInputY;
		w.w = kInputW; w.h = kInputH;
		w.textBuffer    = state.inputBuffer;
		w.textBufferLen = (int)sizeof(state.inputBuffer);
		silencer::ui::clay_inspector::Register(w);
	}

	(void)world;
}

}  // namespace silencer::ui::lobby_clay
