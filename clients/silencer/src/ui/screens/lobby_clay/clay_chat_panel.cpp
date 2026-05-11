#include "clay_chat_panel.h"

#include "clay/clay.h"
#include "clay_bridge.h"
#include "clay_inspector.h"
#include "primitives/bank_text.h"
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

// Legacy chat panel constants — copied verbatim from chat_panel.cpp's
// Build so the on-screen geometry matches one-for-one.
constexpr Uint16 kChatX        = 19;
constexpr Uint16 kChatY        = 220;
constexpr Uint16 kChatW        = 242;
constexpr Uint16 kChatH        = 207;
constexpr Uint16 kPresX        = 267;
constexpr Uint16 kPresY        = 220;
constexpr Uint16 kPresW        = 110;
constexpr Uint16 kPresH        = 207;
constexpr Uint8  kLineHeight   = 11;
constexpr Uint8  kFontWidth    = 6;
constexpr Uint16 kInputX       = 18;
constexpr Uint16 kInputY       = 437;
constexpr Uint16 kInputW       = 360;
constexpr Uint16 kInputH       = 14;
constexpr Uint16 kChannelX     = 15;
constexpr Uint16 kChannelY     = 200;
constexpr Uint8  kChannelBank  = 134;

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
	// Background sprites — chatborder (bank 7 idx 11) and chatinputborder
	// (bank 7 idx 14). Legacy creates them as Overlay objects with default
	// x=y=0; the renderer subtracts spriteoffsetx/y before BlitSurface.
	// Mirror by floating at (-spriteoffsetx, -spriteoffsety) so the Clay
	// bridge's natural top-left blit lands at the same pixels.
	const Uint16 borderW = resources.spritewidth[7][11];
	const Uint16 borderH = resources.spriteheight[7][11];
	const int borderX = 0 - resources.spriteoffsetx[7][11];
	const int borderY = 0 - resources.spriteoffsety[7][11];
	CLAY({ .id = CLAY_ID("ChatBorder"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(borderW)),
	                       CLAY_SIZING_FIXED(static_cast<float>(borderH)) },
	       },
	       .image    = { .imageData = silencer::clay_bridge::PackImage(7, 11) },
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { static_cast<float>(borderX),
	                                   static_cast<float>(borderY) } } }) {}

	const Uint16 inBorderW = resources.spritewidth[7][14];
	const Uint16 inBorderH = resources.spriteheight[7][14];
	const int inBorderX = 0 - resources.spriteoffsetx[7][14];
	const int inBorderY = 0 - resources.spriteoffsety[7][14];
	CLAY({ .id = CLAY_ID("ChatInputBorder"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(inBorderW)),
	                       CLAY_SIZING_FIXED(static_cast<float>(inBorderH)) },
	       },
	       .image    = { .imageData = silencer::clay_bridge::PackImage(7, 14) },
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { static_cast<float>(inBorderX),
	                                   static_cast<float>(inBorderY) } } }) {}

	// Channel name header at (15, 200), bank 134 / w8 — matches the legacy
	// channeltext overlay. Skip emit when empty (lobby hasn't sent
	// MSG_CHANNEL yet).
	if(!state.channel.empty()){
		CLAY({ .id = CLAY_ID("ChatChannelWrap"),
		       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
		                     .offset   = { kChannelX, kChannelY } } }) {
			BankText(FromStd(state.channel),
			         BankTextVariant::Heading,
			         {});
		}
	}
	(void)kChannelBank;

	// Chat scrollback ScrollTextBox at (19, 220) size 242x207. BottomUp
	// origin matches legacy bottomtotop=true. Scrollbar uses bank 7
	// idx 12/13. We let ScrollTextBox emit its own embedded scrollbar
	// column when there are enough lines to overflow.
	ScrollTextBoxOpts chatOpts;
	chatOpts.width               = kChatW;
	chatOpts.height              = kChatH;
	chatOpts.lineHeight          = kLineHeight;
	chatOpts.textVariant         = BankTextVariant::Body;  // bank 133 w6
	chatOpts.origin              = ScrollTextBoxOrigin::BottomUp;
	chatOpts.showScrollbar       = false;
	chatOpts.scrollbarBank       = 7;
	chatOpts.scrollbarTrackIndex = 12;
	chatOpts.scrollbarThumbIndex = 13;
	chatOpts.scrollbarWidth      = 0;
	chatOpts.scrollbarGap        = 0;

	const int chatN = FillSlab(g_chatSlab, state.chatLines);

	CLAY({ .id = CLAY_ID("ChatBoxWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kChatX, kChatY } } }) {
		ScrollTextBox(CLAY_STRING("ChatBox"),
		              g_chatSlab,
		              chatN,
		              state.chatScrollPos,
		              chatOpts);
	}

	// Presence list at (267, 220) size 110x207. TopDown, no scrollbar
	// (legacy presencebox has no scrollbar attached).
	ScrollTextBoxOpts presOpts;
	presOpts.width        = kPresW;
	presOpts.height       = kPresH;
	presOpts.lineHeight   = kLineHeight;
	presOpts.textVariant  = BankTextVariant::Body;
	presOpts.origin       = ScrollTextBoxOrigin::TopDown;
	presOpts.showScrollbar = false;

	const int presN = FillSlab(g_presSlab, state.presenceLines);

	CLAY({ .id = CLAY_ID("ChatPresWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kPresX, kPresY } } }) {
		ScrollTextBox(CLAY_STRING("ChatPresence"),
		              g_presSlab,
		              presN,
		              state.presenceScrollPos,
		              presOpts);
	}

	// Chat input at (18, 437) size 360x14, bank 133 / fontWidth 6.
	// `showCaret=false` mirrors the legacy steady state where the chat
	// interface is not the active interface (gameSelect/etc. are), so
	// `interface.cpp`'s "showcaret = (activeobject == textinput->id)"
	// resolves to false. SDL_TEXTINPUT wiring + onEnter routing land
	// with P18 (CLI inspect compat).
	TextInputOpts inOpts;
	inOpts.widthPx     = kInputW;
	inOpts.heightPx    = kInputH;
	inOpts.fontBank    = 133;
	inOpts.fontWidth   = kFontWidth;
	inOpts.brightness  = 128;
	inOpts.showCaret   = false;
	inOpts.inactive    = false;

	CLAY({ .id = CLAY_ID("ChatInputWrap"),
	       .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
	                     .offset   = { kInputX, kInputY } } }) {
		TextInput(CLAY_STRING("ChatInput"),
		          state.inputBuffer,
		          inOpts,
		          TextInputHandle{ /*hoveredOut*/ nullptr,
		                           /*onEnter*/    nullptr,
		                           /*user*/       nullptr });
	}
	{
		silencer::ui::clay_inspector::Widget w;
		w.label = "Chat";
		w.kind  = silencer::ui::clay_inspector::WidgetKind::TextInput;
		w.x = kInputX; w.y = kInputY; w.w = kInputW; w.h = kInputH;
		w.textBuffer    = state.inputBuffer;
		w.textBufferLen = (int)sizeof(state.inputBuffer);
		silencer::ui::clay_inspector::Register(w);
	}

	(void)world;
}

}  // namespace silencer::ui::lobby_clay
