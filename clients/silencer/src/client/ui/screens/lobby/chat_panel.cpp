#include "chat_panel.h"

#include "primitives/scroll_text_box.h"

#include "lobby.h"
#include "lobbygame.h"
#include "text_wrap.h"
#include "world.h"

#include <algorithm>
#include <cstring>

using silencer::ui::primitives::ScrollTextBoxAutoScroll;

namespace silencer::client_ui::lobby {

namespace chat_panel_detail {

constexpr Uint8  kLineHeight = 11;
constexpr Uint16 kInputH = 14;
constexpr Uint16 kScrollbarReserve = 8;
constexpr Uint16 kTextAdvance = 6;
constexpr Uint16 kMinChatWidth = 60;
constexpr Uint16 kMinPresenceWidth = 72;
constexpr Uint16 kMaxPresenceWidth = 220;
constexpr int    kPresenceRatioNum = 110;
constexpr int    kPresenceRatioDen = 358;
constexpr size_t kMaxStoredEntries = 256;
constexpr size_t kMaxStoredLines = 256;

constexpr const char * kActionInput = "lobby.chat.input";

int ClampInt(int value, int lo, int hi) {
	if(value < lo) return lo;
	if(value > hi) return hi;
	return value;
}

int ScaleWidthPx(int base,
                 Uint16 panelWidth,
                 int legacyWidth,
                 int minValue,
                 int maxValue) {
	const int scaled = static_cast<int>(
		(static_cast<long long>(base) * std::max<int>(panelWidth, 1) + legacyWidth / 2)
		/ legacyWidth);
	return ClampInt(scaled, minValue, maxValue);
}

int ScaleHeightPx(int base,
                  Uint16 panelHeight,
                  int legacyHeight,
                  int minValue,
                  int maxValue) {
	const int scaled = static_cast<int>(
		(static_cast<long long>(base) * std::max<int>(panelHeight, 1) + legacyHeight / 2)
		/ legacyHeight);
	return ClampInt(scaled, minValue, maxValue);
}

int MaxScrollForLineCount(int lineCount,
                          Uint8 lineHeight,
                          Uint16 height) {
	if(lineHeight == 0) return 0;
	const int visibleLines = height / lineHeight;
	const int maxScroll = lineCount - visibleLines;
	return maxScroll > 0 ? maxScroll : 0;
}

bool IsPinnedToBottom(Uint16 scrollPosition,
                      int lineCount,
                      Uint16 height) {
	return static_cast<int>(scrollPosition) >=
	       MaxScrollForLineCount(lineCount, kLineHeight, height);
}

void PushEntry(std::vector<ChatEntry> & entries,
               std::string text,
               Uint8 color,
               Uint8 brightness,
               Uint8 continuationIndentSpaces) {
	if(entries.size() >= kMaxStoredEntries){
		entries.erase(entries.begin());
	}
	ChatEntry entry;
	entry.text = std::move(text);
	entry.color = color;
	entry.brightness = brightness;
	entry.continuationIndentSpaces = continuationIndentSpaces;
	entries.push_back(std::move(entry));
}

void AppendWrappedEntry(std::vector<ChatLine> & lines,
                        const ChatEntry & entry,
                        int maxChars) {
	if(entry.text.empty()) return;
	char breakchar[10];
	std::strcpy(breakchar, "\n");
	for(int i = 0; i < entry.continuationIndentSpaces && i < 8; i++){
		std::strcat(breakchar, " ");
	}
	char * wrapped = silencer::ui::WordWrapText(
		entry.text.c_str(),
		maxChars > 0 ? maxChars : 1,
		breakchar);
	if(!wrapped) return;
	char * line = std::strtok(wrapped, "\n");
	while(line){
		ChatLine wrappedLine;
		wrappedLine.text = line;
		wrappedLine.color = entry.color;
		wrappedLine.brightness = entry.brightness;
		wrappedLine.indent = 0;
		lines.push_back(std::move(wrappedLine));
		line = std::strtok(nullptr, "\n");
	}
	delete[] wrapped;
}

void TrimWrappedLines(std::vector<ChatLine> & lines) {
	if(lines.size() > kMaxStoredLines){
		lines.erase(lines.begin(), lines.begin() + (lines.size() - kMaxStoredLines));
	}
}

void RebuildWrappedLines(std::vector<ChatLine> & lines,
                         const std::vector<ChatEntry> & entries,
                         int maxChars) {
	lines.clear();
	lines.reserve(entries.size());
	for(const ChatEntry & entry : entries){
		AppendWrappedEntry(lines, entry, maxChars);
	}
	TrimWrappedLines(lines);
}

void RefreshChatLines(ChatPanelState & state) {
	const bool pinnedToBottom =
		IsPinnedToBottom(state.chatScrollPos,
		                 static_cast<int>(state.chatLines.size()),
		                 state.chatViewportHeight);
	RebuildWrappedLines(state.chatLines, state.chatEntries, state.chatWrapChars);
	const int newMax = MaxScrollForLineCount(
		static_cast<int>(state.chatLines.size()),
		kLineHeight,
		state.chatViewportHeight);
	if(pinnedToBottom){
		state.chatScrollPos = static_cast<Uint16>(newMax);
	}else if(static_cast<int>(state.chatScrollPos) > newMax){
		state.chatScrollPos = static_cast<Uint16>(newMax);
	}
	state.chatWrapDirty = false;
}

void RefreshPresenceLines(ChatPanelState & state) {
	RebuildWrappedLines(state.presenceLines,
	                    state.presenceEntries,
	                    state.presenceWrapChars);
	const int newMax = MaxScrollForLineCount(
		static_cast<int>(state.presenceLines.size()),
		kLineHeight,
		state.chatViewportHeight);
	if(static_cast<int>(state.presenceScrollPos) > newMax){
		state.presenceScrollPos = static_cast<Uint16>(newMax);
	}
	state.presenceWrapDirty = false;
}

void RebuildPresenceEntries(ChatPanelState & state, World & world) {
	state.presenceEntries.clear();
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
			PushEntry(state.presenceEntries, header, 0, 128 + 32, 0);
			lastgroup = r.group;
		}
		PushEntry(state.presenceEntries, r.label, 0, 128, 2);
	}
	state.presenceWrapDirty = true;
}

void CopyUiText(char * dst, int dstLen, const std::string & value)
{
	if(!dst || dstLen <= 0) return;
	int n = static_cast<int>(value.size());
	if(n > dstLen - 1) n = dstLen - 1;
	std::memcpy(dst, value.data(), n);
	dst[n] = '\0';
}

}  // namespace chat_panel_detail

ChatPanelLayoutMetrics ResolveChatPanelLayout(Uint16 panelWidth,
                                              Uint16 panelHeight) {
	ChatPanelLayoutMetrics metrics;
	metrics.rootPadX = static_cast<Uint16>(
		chat_panel_detail::ScaleWidthPx(5, panelWidth, 378, 5, 12));
	metrics.rootPadTop = static_cast<Uint16>(
		chat_panel_detail::ScaleHeightPx(5, panelHeight, 260, 4, 8));
	metrics.rootPadBottom = metrics.rootPadTop;
	metrics.channelHeight = static_cast<Uint16>(
		chat_panel_detail::ScaleHeightPx(16, panelHeight, 260, 16, 22));
	metrics.mainGap = static_cast<Uint16>(
		chat_panel_detail::ScaleHeightPx(4, panelHeight, 260, 3, 7));
	metrics.bodyGap = static_cast<Uint16>(
		chat_panel_detail::ScaleWidthPx(6, panelWidth, 378, 6, 14));
	metrics.bodyPadLeft = static_cast<Uint16>(
		chat_panel_detail::ScaleWidthPx(4, panelWidth, 378, 4, 10));
	metrics.bodyPadRight = metrics.bodyPadLeft;
	metrics.bodyPadTop = static_cast<Uint16>(
		chat_panel_detail::ScaleHeightPx(4, panelHeight, 260, 3, 8));
	metrics.bodyPadBottom = static_cast<Uint16>(
		chat_panel_detail::ScaleHeightPx(2, panelHeight, 260, 2, 5));
	metrics.inputBorderHeight = static_cast<Uint16>(
		chat_panel_detail::ScaleHeightPx(17, panelHeight, 260, 17, 24));
	metrics.inputPadX = static_cast<Uint16>(
		chat_panel_detail::ScaleWidthPx(3, panelWidth, 378, 3, 8));
	metrics.inputPadTop = static_cast<Uint16>(
		chat_panel_detail::ScaleHeightPx(1, panelHeight, 260, 1, 3));
	metrics.inputPadBottom = static_cast<Uint16>(
		chat_panel_detail::ScaleHeightPx(2, panelHeight, 260, 2, 4));
	const int contentW = std::max(
		0,
		static_cast<int>(panelWidth) - static_cast<int>(metrics.rootPadX) * 2);
	const int contentH = std::max(
		0,
		static_cast<int>(panelHeight)
			- static_cast<int>(metrics.rootPadTop)
			- static_cast<int>(metrics.rootPadBottom));
	const int mainBorderH = std::max(
		0,
		contentH
			- static_cast<int>(metrics.channelHeight)
			- static_cast<int>(metrics.mainGap)
			- static_cast<int>(metrics.inputBorderHeight));
	const int bodyW = std::max(
		0,
		contentW
			- static_cast<int>(metrics.bodyPadLeft)
			- static_cast<int>(metrics.bodyPadRight));
	const int bodyH = std::max(
		0,
		mainBorderH
			- static_cast<int>(metrics.bodyPadTop)
			- static_cast<int>(metrics.bodyPadBottom));

	int presenceW = 0;
	if(bodyW > static_cast<int>(metrics.bodyGap)
	            + static_cast<int>(chat_panel_detail::kMinChatWidth)){
		const int scaledPresence =
			(bodyW * chat_panel_detail::kPresenceRatioNum
			 + chat_panel_detail::kPresenceRatioDen / 2)
			/ chat_panel_detail::kPresenceRatioDen;
		const int maxPresence = std::max(
			0,
			bodyW - static_cast<int>(metrics.bodyGap)
			      - static_cast<int>(chat_panel_detail::kMinChatWidth));
		presenceW = std::max<int>(chat_panel_detail::kMinPresenceWidth, scaledPresence);
		if(presenceW > static_cast<int>(chat_panel_detail::kMaxPresenceWidth)){
			presenceW = chat_panel_detail::kMaxPresenceWidth;
		}
		if(presenceW > maxPresence){
			presenceW = maxPresence;
		}
	}
	const int bodyGap = presenceW > 0 ? static_cast<int>(metrics.bodyGap) : 0;
	const int chatW = std::max(0, bodyW - bodyGap - presenceW);

	metrics.mainBorderWidth = static_cast<Uint16>(contentW);
	metrics.mainBorderHeight = static_cast<Uint16>(mainBorderH);
	metrics.chatWidth = static_cast<Uint16>(chatW);
	metrics.chatHeight = static_cast<Uint16>(bodyH);
	metrics.presenceWidth = static_cast<Uint16>(presenceW);
	metrics.presenceHeight = static_cast<Uint16>(bodyH);
	metrics.inputWidth = static_cast<Uint16>(std::max(
		0,
		contentW - static_cast<int>(metrics.inputPadX) * 2));
	metrics.inputHeight = static_cast<Uint16>(std::max(
		static_cast<int>(chat_panel_detail::kInputH),
		static_cast<int>(metrics.inputBorderHeight)
			- static_cast<int>(metrics.inputPadTop)
			- static_cast<int>(metrics.inputPadBottom)));
	metrics.chatWrapChars = static_cast<Uint16>(std::max(
		1,
		(std::max(1, chatW - static_cast<int>(chat_panel_detail::kScrollbarReserve)))
			/ static_cast<int>(chat_panel_detail::kTextAdvance)));
	metrics.presenceWrapChars = static_cast<Uint16>(std::max(
		1,
		std::max(1, presenceW) / static_cast<int>(chat_panel_detail::kTextAdvance)));
	return metrics;
}

void ChatPanelSyncLayout(ChatPanelState & state,
                         Uint16 panelWidth,
                         Uint16 panelHeight) {
	const ChatPanelLayoutMetrics metrics =
		ResolveChatPanelLayout(panelWidth, panelHeight);
	const bool chatLayoutChanged =
		state.chatWrapChars != metrics.chatWrapChars
		|| state.chatViewportHeight != metrics.chatHeight;
	const bool presenceLayoutChanged =
		state.presenceWrapChars != metrics.presenceWrapChars;

	state.chatWrapChars = metrics.chatWrapChars;
	state.presenceWrapChars = metrics.presenceWrapChars;
	state.chatViewportHeight = metrics.chatHeight;

	if(chatLayoutChanged || state.chatWrapDirty){
		chat_panel_detail::RefreshChatLines(state);
	}
	if(presenceLayoutChanged || state.presenceWrapDirty){
		chat_panel_detail::RefreshPresenceLines(state);
	}
}

void ChatPanelInit(ChatPanelState & state) {
	state.chatEntries.clear();
	state.presenceEntries.clear();
	state.chatLines.clear();
	state.presenceLines.clear();
	state.chatScrollPos = 0;
	state.presenceScrollPos = 0;
	state.chatWrapChars = 0;
	state.presenceWrapChars = 0;
	state.chatViewportHeight = 0;
	state.chatWrapDirty = true;
	state.presenceWrapDirty = true;
	state.channel.clear();
	state.inputBuffer[0] = '\0';
}

void ChatPanelTick(ChatPanelState & state, World & world) {
	if(world.lobby.channelchanged){
		if(world.lobby.lastchannel[0] == '\0'){
			strcpy(world.lobby.lastchannel, world.lobby.channel);
		}
		state.channel = world.lobby.channel;
		world.lobby.channelchanged = false;
	}

	if(world.lobby.presencechanged || !world.lobby.gamesprocessed){
		chat_panel_detail::RebuildPresenceEntries(state, world);
		if(state.presenceWrapChars > 0){
			chat_panel_detail::RefreshPresenceLines(state);
		}
		world.lobby.presencechanged = false;
	}

	bool chatChanged = false;
	while(!world.lobby.chatmessages.empty()){
		auto message = world.lobby.chatmessages.front();
		const char * msgtext = message.data();
		size_t msglen = std::strlen(msgtext);
		Uint8 color = static_cast<Uint8>(message[msglen + 1]);
		Uint8 brightness = static_cast<Uint8>(message[msglen + 2]);

		chat_panel_detail::PushEntry(
			state.chatEntries,
			msgtext ? std::string(msgtext) : std::string(),
			color,
			brightness,
			2);
		state.chatWrapDirty = true;
		chatChanged = true;
		world.lobby.chatmessages.pop_front();
	}
	if(chatChanged && state.chatWrapChars > 0){
		const int prevCount = static_cast<int>(state.chatLines.size());
		chat_panel_detail::RefreshChatLines(state);
		const int newCount = static_cast<int>(state.chatLines.size());
		state.chatScrollPos = ScrollTextBoxAutoScroll(
			state.chatScrollPos,
			prevCount,
			newCount,
			chat_panel_detail::kLineHeight,
			state.chatViewportHeight);
	}
}

bool ChatPanelHandleUiIntent(ChatPanelState & state,
                             World & world,
                             const silencer::ui::UiAction & action) {
	if(action.id != chat_panel_detail::kActionInput) return false;
	if(action.kind == silencer::ui::UiActionKind::SetText){
		chat_panel_detail::CopyUiText(state.inputBuffer, static_cast<int>(sizeof(state.inputBuffer)), action.value);
		return true;
	}
	if(action.kind == silencer::ui::UiActionKind::SubmitText){
		chat_panel_detail::CopyUiText(state.inputBuffer, static_cast<int>(sizeof(state.inputBuffer)), action.value);
		if(std::strlen(state.inputBuffer) > 0){
			world.lobby.SendChat(world.lobby.channel, state.inputBuffer);
			state.inputBuffer[0] = '\0';
		}
		return true;
	}
	return action.kind == silencer::ui::UiActionKind::Select;
}

}  // namespace silencer::client_ui::lobby
