#include "client/ui/hud/InGameOverlays.h"

#include "client/ui/hud/HudPayloadArena.h"
#include "client/ui/views/HudView.h"
#include "clay_ui_compositor.h"
#include "primitives/bank_text.h"
#include "render/clay_ui_payloads.h"
#include "render/renderer.h"
#include "resources.h"
#include "surface.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace silencer {
namespace client_ui {

namespace {

Clay_String StringFromStd(const std::string& text) {
	return Clay_String{
		.isStaticallyAllocated = false,
		.length = static_cast<int32_t>(text.size()),
		.chars = text.c_str(),
	};
}

Clay_ElementDeclaration FloatingTextElement(const char* id, int x, int y, int w, int h) {
	Clay_String idString{
		.isStaticallyAllocated = false,
		.length = static_cast<int32_t>(std::strlen(id)),
		.chars = id,
	};
	return {
		.id = Clay_GetElementId(idString),
		.layout = {
			.sizing = { CLAY_SIZING_FIXED((float)w), CLAY_SIZING_FIXED((float)h) },
			.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
		},
		.floating = {
			.offset = { (float)x, (float)y },
			.attachTo = CLAY_ATTACH_TO_ROOT,
		},
	};
}

Clay_ElementDeclaration FloatingTextElementI(const char* id, uint32_t index, int x, int y, int w, int h) {
	Clay_ElementDeclaration decl = FloatingTextElement(id, x, y, w, h);
	Clay_String idString{
		.isStaticallyAllocated = false,
		.length = static_cast<int32_t>(std::strlen(id)),
		.chars = id,
	};
	decl.id = Clay_GetElementIdWithIndex(idString, index);
	return decl;
}

void DrawMessage(const HudView& view, Surface* surface) {
	const InGameMessageView& msg = view.message;
	if(!msg.message_i) return;
	if(msg.message.empty()) return;

	struct MessageGlyph {
		std::string text;
		int x;
		int y;
		Uint8 bank;
		Uint8 width;
		Uint8 color;
		silencer::clay_bridge::BankTextDrawData draw;
	};
	std::vector<MessageGlyph> glyphs;
	const char* text = msg.message.c_str();
	int totalLen = (int)msg.message.size();
	glyphs.reserve(totalLen * 2);
	int linelength = totalLen;
	const char* newline = std::strchr(text, '\n');
	if(newline) linelength = (int)(newline - text);
	int liney = 60;
	Uint8 color = 208;
	int textwidth = 11;
	int textbank = 135;
	int lineheight = 20;
	switch(msg.messagetype) {
		case 1: color = 128; liney = 190; textbank = 134; textwidth = 10; break;
		case 2: color = 128; break;
		case 3: color = 192; break;
		case 4: color = 153; break;
		case 10: color = 224; break;
		case 11: color = 153; break;
		case 20: color = 153; liney = 200; break;
	}
	int nextline = linelength;
	int line = 0;
	for(int i = 0; i < totalLen; i++) {
		if(i >= msg.message_i) break;
		Uint8 brightness = 128;
		if(msg.messagetype < 10) {
			if(msg.message_i - msg.messagetime + 8 >= 0) brightness -= (msg.message_i - msg.messagetime + 8) * 8;
			if(msg.message_i % 32 >= 16) {
				brightness += ((16 - (msg.message_i % 16)) * 2);
			}else{
				brightness += ((msg.message_i % 16) * 2);
			}
		}
		if(msg.message_i - i <= 5) brightness += 40 - ((msg.message_i - i) * 8);
		char temp[2] = { text[i], 0 };
		if(msg.messagetype >= 10) {
			if(line == 0) {
				textbank = 136;
				textwidth = 25;
			}else{
				textwidth = 13;
				textbank = 135;
			}
		}
		Uint8 brightness2 = (int(brightness) - 64) < 8 ? 8 : brightness - 64;
		const int x = ((surface->w - (linelength * textwidth)) / 2) + (textwidth * (linelength - nextline));
		glyphs.push_back({temp, x + 1, liney + 1, (Uint8)textbank, (Uint8)textwidth, color, {brightness2, false, false}});
		glyphs.push_back({temp, x, liney, (Uint8)textbank, (Uint8)textwidth, color, {brightness, false, false}});
		nextline--;
		if(nextline < 0) {
			linelength = (int)std::strlen(&text[i + 1]);
			const char* nextNewline = std::strchr(&text[i + 1], '\n');
			if(nextNewline) linelength = (int)(nextNewline - &text[i + 1]);
			nextline = linelength;
			liney += (line == 0 && msg.messagetype >= 10) ? 40 : lineheight;
			line++;
		}
	}
	if(glyphs.empty()) return;
	CLAY({ .id = CLAY_ID("InGameMessageRoot"),
	       .layout = { .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) } } }) {
		for(size_t i = 0; i < glyphs.size(); ++i) {
			MessageGlyph& glyph = glyphs[i];
			CLAY(FloatingTextElementI("InGameMessageGlyph", (uint32_t)i, glyph.x, glyph.y, glyph.width, 20)) {
				CLAY_TEXT(StringFromStd(glyph.text), CLAY_TEXT_CONFIG({
					.userData = &glyph.draw,
					.textColor = { (float)glyph.color, 0, 0, 255 },
					.fontId = glyph.bank,
					.fontSize = glyph.width,
				}));
			}
		}
	}
}

void DrawStatus(const HudView& view, Surface* surface) {
	struct StatusLine {
		std::string text;
		int x;
		int y;
		Uint8 color;
		silencer::clay_bridge::BankTextDrawData draw;
	};
	std::vector<StatusLine> lines;
	lines.reserve(view.statusMessages.size() * 2);
	int liney = 0;
	for(const InGameStatusLineView& src : view.statusMessages) {
		Uint8 brightness = 128;
		if(src.time <= 16) brightness -= (16 - src.time) * 8;
		Uint8 brightness2 = (int(brightness) - 64) < 8 ? 8 : brightness - 64;
		const int x = (surface->w - ((int)src.text.size() * 7)) / 2;
		lines.push_back({src.text, x + 1, 370 + liney + 1, src.color, {brightness2, false, false}});
		lines.push_back({src.text, x, 370 + liney, src.color, {brightness, false, false}});
		liney -= 10;
	}
	if(lines.empty()) return;
	CLAY({ .id = CLAY_ID("InGameStatusRoot"),
	       .layout = { .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) } } }) {
		for(size_t i = 0; i < lines.size(); ++i) {
			StatusLine& line = lines[i];
			CLAY(FloatingTextElementI("InGameStatusLine", (uint32_t)i, line.x, line.y, (int)line.text.size() * 7, 12)) {
				CLAY_TEXT(StringFromStd(line.text), CLAY_TEXT_CONFIG({
					.userData = &line.draw,
					.textColor = { (float)line.color, 0, 0, 255 },
					.fontId = 133,
					.fontSize = 7,
				}));
			}
		}
	}
}

void DrawTopMessage(const HudView& view, Surface* surface) {
	const InGameTopMessageView& msg = view.topMessage;
	if(!msg.topmessage_i) return;
	if(msg.text.empty()) return;

	const char* text = msg.text.c_str();
	int progress = msg.topmessage_i;
	int slen = (int)msg.text.size();
	int start = 0;
	if(progress / 2 > 24){
		start = (progress / 2) - 24;
		if(start > slen) start = slen;
	}
	const int maxlength = 35;
	char textmax[maxlength + 1];
	std::memset(textmax, 0, sizeof(textmax));
	std::strncpy(textmax, text + start, maxlength);
	std::string topText(textmax);
	CLAY({ .id = CLAY_ID("InGameTopMessageRoot"),
	       .layout = { .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) } } }) {
		CLAY(FloatingTextElement("InGameTopMessage", 200, 10, 245, 12)) {
			CLAY_TEXT(StringFromStd(topText), CLAY_TEXT_CONFIG({
				.textColor = { 0, 0, 0, 255 },
				.fontId = 133,
				.fontSize = 7,
			}));
		}
	}
}

void DrawQuitPrompt(const HudView& /*view*/, Surface* surface) {
#ifdef OUYA
	std::string text = "Hit O To QUIT";
#else
	std::string text = "Hit Enter To Quit";
#endif
	const int width = (int)text.size() * 16;
	CLAY({ .id = CLAY_ID("QuitPromptRoot"),
	       .layout = { .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) } } }) {
		CLAY(FloatingTextElement("QuitPromptText", (surface->w - width) / 2, 200, width, 24)) {
			CLAY_TEXT(StringFromStd(text), CLAY_TEXT_CONFIG({
				.textColor = { 202, 0, 0, 255 },
				.fontId = 136,
				.fontSize = 16,
			}));
		}
	}
}

void DrawPlayerList(const HudView& view, Surface* surface) {
	if(view.teams.empty()) return;
	// Skip empty player-list (no peer rows means nothing to render).
	bool anyPeers = false;
	for(const TeamHudView& team : view.teams){
		if(!team.playerListPeers.empty()){ anyPeers = true; break; }
	}
	if(!anyPeers) return;

	using namespace silencer::ui::primitives;

	CLAY({ .id = CLAY_ID("PlayerListRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
		       .padding = { 50, 50, 50, 0 },
		       .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       } }) {
		CLAY({ .id = CLAY_ID("PlayerListPanel"),
		       .layout = {
			       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED((float)(10 + (view.teams.size() * 58))) },
			       .padding = { 10, 10, 10, 0 },
			       .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .backgroundColor = { 0, 0, 0, 128 },
		}) {
			for(unsigned int teamIndex = 0; teamIndex < view.teams.size(); ++teamIndex) {
				const TeamHudView& team = view.teams[teamIndex];
				CLAY({ .id = CLAY_IDI("PlayerListTeamRow", teamIndex),
				       .layout = {
					       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(58) },
					       .layoutDirection = CLAY_LEFT_TO_RIGHT,
				       } }) {
					CLAY({ .id = CLAY_IDI("PlayerListEmblemSlot", teamIndex),
					       .layout = {
						       .sizing = { CLAY_SIZING_FIXED(40), CLAY_SIZING_GROW(0) },
						       .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
					       } }) {
						CLAY({ .id = CLAY_IDI("PlayerListTeamEmblem", teamIndex),
						       .layout = { .sizing = { CLAY_SIZING_FIXED((float)team.emblemW), CLAY_SIZING_FIXED((float)team.emblemH) } },
						       .custom = { .customData = AllocTeamEmblemCustomData({181, team.agency, team.color, 17, true}) },
						}) {}
					}
					int yoffset = ((4 - (int)team.playerListPeers.size()) * 12) / 2;
					if(yoffset < 0) yoffset = 0;
					CLAY({ .id = CLAY_IDI("PlayerListPeerColumn", teamIndex),
					       .layout = {
						       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
						       .padding = { 0, 0, (uint16_t)yoffset, 0 },
						       .layoutDirection = CLAY_TOP_TO_BOTTOM,
					       } }) {
						for(unsigned int peerIndex = 0; peerIndex < team.playerListPeers.size(); ++peerIndex) {
							const TeamPeerView& peer = team.playerListPeers[peerIndex];
							char stats[100];
							std::snprintf(stats, sizeof(stats), "L:%d    E:%d  S:%d  J:%d  H:%d  C:%d",
							              peer.agencyLevel, peer.agencyEndurance, peer.agencyShield,
							              peer.agencyJetpack, peer.agencyHacking, peer.agencyContacts);
							std::string statsString = stats;
							CLAY({ .id = CLAY_IDI("PlayerListPeerRow", (teamIndex * 8) + peerIndex),
							       .layout = {
								       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(12) },
								       .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
								       .layoutDirection = CLAY_LEFT_TO_RIGHT,
							       } }) {
								CLAY({ .id = CLAY_IDI("PlayerListPeerName", (teamIndex * 8) + peerIndex),
								       .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) } } }) {
									BankText(StringFromStd(peer.displayName), BankTextVariant::Body);
								}
								CLAY({ .id = CLAY_IDI("PlayerListPeerStats", (teamIndex * 8) + peerIndex),
								       .layout = { .sizing = { CLAY_SIZING_FIXED((float)((statsString.size() + 1) * 6)), CLAY_SIZING_FIT(0) } } }) {
									BankText(StringFromStd(statsString), BankTextVariant::Body);
								}
							}
						}
					}
				}
			}
		}
	}
}

}  // namespace

void BuildInGameOverlaysUi(Renderer& /*renderer*/, const Resources& /*resources*/,
                           const HudView& view, Surface* surface) {
	if(!view.mapLoaded) return;
	DrawStatus(view, surface);
	DrawTopMessage(view, surface);
	DrawMessage(view, surface);
	if(view.showPlayerList){
		DrawPlayerList(view, surface);
	}
	if(view.quitState == 1 || view.quitState == 2){
		DrawQuitPrompt(view, surface);
	}
}

}  // namespace client_ui
}  // namespace silencer
