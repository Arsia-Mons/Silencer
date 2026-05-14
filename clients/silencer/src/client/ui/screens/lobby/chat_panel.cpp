#include "chat_panel.h"

#include "primitives/scroll_text_box.h"

#include "lobby.h"
#include "lobbygame.h"
#include "world.h"

#include <algorithm>
#include <cstring>

using silencer::ui::primitives::ScrollTextBoxAutoScroll;

namespace silencer::client_ui::lobby {

namespace {

// Mirror of the Build-side chat scrollback height + line height — needed
// only by the auto-scroll computation here, NOT for any layout. The
// authoritative copies live in chat_panel_layout.cpp.
constexpr Uint16 kChatH      = 207;
constexpr Uint8  kLineHeight = 11;

constexpr const char * kActionInput = "lobby.chat.input";

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

void CopyUiText(char * dst, int dstLen, const std::string & value)
{
	if(!dst || dstLen <= 0) return;
	int n = static_cast<int>(value.size());
	if(n > dstLen - 1) n = dstLen - 1;
	std::memcpy(dst, value.data(), n);
	dst[n] = '\0';
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

bool ChatPanelHandleUiIntent(ChatPanelState & state,
                             World & world,
                             const silencer::ui::UiAction & action) {
	if(action.id != kActionInput) return false;
	if(action.kind == silencer::ui::UiActionKind::SetText){
		CopyUiText(state.inputBuffer, static_cast<int>(sizeof(state.inputBuffer)), action.value);
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::SubmitText){
		CopyUiText(state.inputBuffer, static_cast<int>(sizeof(state.inputBuffer)), action.value);
		if(std::strlen(state.inputBuffer) > 0){
			world.lobby.SendChat(world.lobby.channel, state.inputBuffer);
			state.inputBuffer[0] = '\0';
		}
		return true;
	}
	return action.kind == silencer::ui::UiActionKind::Select;
}

}  // namespace silencer::client_ui::lobby
