#include "client/ui/hud/InGameOverlays.h"

#include "objecttypes.h"
#include "player.h"
#include "primitives/bank_text.h"
#include "render/renderer.h"
#include "surface.h"
#include "team.h"
#include "clay_ui_compositor.h"
#include "user.h"
#include "world.h"

#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace silencer {
namespace client_ui {

class InGameOverlayRenderer {
public:
	static void DrawAll(Renderer& renderer, World& world, Surface* surface);

private:
	static void DrawMessage(Renderer& renderer, World& world, Surface* surface);
	static void DrawStatus(Renderer& renderer, World& world, Surface* surface);
	static void DrawTopMessage(Renderer& renderer, World& world, Surface* surface);
	static void DrawQuitPrompt(Renderer& renderer, World& world, Surface* surface);
	static void DrawPlayerList(Renderer& renderer, World& world, Surface* surface);
};

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

constexpr int kTeamEmblemPayloadCapacity = 16;
silencer::clay_bridge::TeamEmblemPayload g_teamEmblemPayloads[kTeamEmblemPayloadCapacity];
silencer::clay_bridge::ClayCustomData g_teamEmblemCustomData[kTeamEmblemPayloadCapacity];
int g_teamEmblemPayloadCount = 0;

void OverlaysBeginFrame() {
	g_teamEmblemPayloadCount = 0;
}

silencer::clay_bridge::ClayCustomData* AllocTeamEmblemCustomData(silencer::clay_bridge::TeamEmblemPayload payload) {
	if(g_teamEmblemPayloadCount >= kTeamEmblemPayloadCapacity) return nullptr;
	g_teamEmblemPayloads[g_teamEmblemPayloadCount] = payload;
	g_teamEmblemCustomData[g_teamEmblemPayloadCount] = {
		silencer::clay_bridge::CustomKind::TeamEmblem,
		&g_teamEmblemPayloads[g_teamEmblemPayloadCount],
	};
	return &g_teamEmblemCustomData[g_teamEmblemPayloadCount++];
}

}  // namespace

void InGameOverlayRenderer::DrawMessage(Renderer& renderer, World& world, Surface* surface) {
	if(!world.message_i) return;
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
	glyphs.reserve(std::strlen(world.message) * 2);
	int linelength = std::strlen(world.message);
	char* newline = std::strchr(world.message, '\n');
	if(newline) linelength = newline - world.message;
	int liney = 60;
	Uint8 color = 208;
	int textwidth = 11;
	int textbank = 135;
	int lineheight = 20;
	switch(world.messagetype) {
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
	for(int i = 0; i < (int)std::strlen(world.message); i++) {
		if(i >= world.message_i) break;
		Uint8 brightness = 128;
		if(world.messagetype < 10) {
			if(world.message_i - world.messagetime + 8 >= 0) brightness -= (world.message_i - world.messagetime + 8) * 8;
			if(world.message_i % 32 >= 16) {
				brightness += ((16 - (world.message_i % 16)) * 2);
			}else{
				brightness += ((world.message_i % 16) * 2);
			}
		}
		if(world.message_i - i <= 5) brightness += 40 - ((world.message_i - i) * 8);
		char temp[2] = { world.message[i], 0 };
		if(world.messagetype >= 10) {
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
			linelength = std::strlen(&world.message[i + 1]);
			char* nextNewline = std::strchr(&world.message[i + 1], '\n');
			if(nextNewline) linelength = nextNewline - &world.message[i + 1];
			nextline = linelength;
			liney += (line == 0 && world.messagetype >= 10) ? 40 : lineheight;
			line++;
		}
	}
	if(glyphs.empty()) return;
	silencer::clay_bridge::EnsureInitialized(surface->w, surface->h);
	Clay_SetPointerState({-1.0f, -1.0f}, false);
	Clay_BeginLayout();
	CLAY({ .id = CLAY_ID("InGameMessageRoot"), .layout = { .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) } } }) {
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
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	silencer::clay_bridge::Render(world.resources, renderer, surface, cmds);
}

void InGameOverlayRenderer::DrawStatus(Renderer& renderer, World& world, Surface* surface) {
	struct StatusLine {
		std::string text;
		int x;
		int y;
		Uint8 color;
		silencer::clay_bridge::BankTextDrawData draw;
	};
	std::vector<StatusLine> lines;
	lines.reserve(world.statusmessages.size() * 2);
	int liney = 0;
	for(std::deque<char*>::iterator it = world.statusmessages.begin(); it != world.statusmessages.end(); it++) {
		char* text = *it;
		char* time = &text[std::strlen(text) + 1];
		char* color = &text[std::strlen(text) + 2];
		Uint8 brightness = 128;
		if(*time <= 16) brightness -= (16 - *time) * 8;
		Uint8 brightness2 = (int(brightness) - 64) < 8 ? 8 : brightness - 64;
		const int x = (surface->w - (std::strlen(text) * 7)) / 2;
		lines.push_back({text, x + 1, 370 + liney + 1, (Uint8)*color, {brightness2, false, false}});
		lines.push_back({text, x, 370 + liney, (Uint8)*color, {brightness, false, false}});
		liney -= 10;
	}
	if(lines.empty()) return;
	silencer::clay_bridge::EnsureInitialized(surface->w, surface->h);
	Clay_SetPointerState({-1.0f, -1.0f}, false);
	Clay_BeginLayout();
	CLAY({ .id = CLAY_ID("InGameStatusRoot"), .layout = { .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) } } }) {
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
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	silencer::clay_bridge::Render(world.resources, renderer, surface, cmds);
}

void InGameOverlayRenderer::DrawTopMessage(Renderer& renderer, World& world, Surface* surface) {
	if(!world.topmessage_i) return;
	char* text = world.topmessage;
	if(world.topmessage_i / 2 > 24) text = &world.topmessage[(world.topmessage_i / 2) - 24];
	const int maxlength = 35;
	char textmax[maxlength + 1];
	std::memset(textmax, 0, sizeof(textmax));
	std::strncpy(textmax, text, maxlength);
	std::string topText(textmax);
	silencer::clay_bridge::EnsureInitialized(surface->w, surface->h);
	Clay_SetPointerState({-1.0f, -1.0f}, false);
	Clay_BeginLayout();
	CLAY({ .id = CLAY_ID("InGameTopMessageRoot"), .layout = { .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) } } }) {
		CLAY(FloatingTextElement("InGameTopMessage", 200, 10, 245, 12)) {
			CLAY_TEXT(StringFromStd(topText), CLAY_TEXT_CONFIG({
				.textColor = { 0, 0, 0, 255 },
				.fontId = 133,
				.fontSize = 7,
			}));
		}
	}
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	silencer::clay_bridge::Render(world.resources, renderer, surface, cmds);
}

void InGameOverlayRenderer::DrawQuitPrompt(Renderer& renderer, World& world, Surface* surface) {
#ifdef OUYA
	std::string text = "Hit O To QUIT";
#else
	std::string text = "Hit Enter To Quit";
#endif
	const int width = (int)text.size() * 16;
	silencer::clay_bridge::EnsureInitialized(surface->w, surface->h);
	Clay_SetPointerState({-1.0f, -1.0f}, false);
	Clay_BeginLayout();
	CLAY({ .id = CLAY_ID("QuitPromptRoot"), .layout = { .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) } } }) {
		CLAY(FloatingTextElement("QuitPromptText", (surface->w - width) / 2, 200, width, 24)) {
			CLAY_TEXT(StringFromStd(text), CLAY_TEXT_CONFIG({
				.textColor = { 202, 0, 0, 255 },
				.fontId = 136,
				.fontSize = 16,
			}));
		}
	}
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	silencer::clay_bridge::Render(world.resources, renderer, surface, cmds);
}

void InGameOverlayRenderer::DrawPlayerList(Renderer& renderer, World& world, Surface* surface) {
	struct PeerRow {
		std::string displayName;
		std::string stats;
	};
	struct TeamRow {
		Uint8 agency;
		Uint8 color;
		int emblemW;
		int emblemH;
		std::vector<PeerRow> peers;
	};
	std::vector<TeamRow> rows;
	for(std::vector<Uint16>::iterator it = world.objectsbytype[ObjectTypes::TEAM].begin(); it != world.objectsbytype[ObjectTypes::TEAM].end(); it++) {
		Team* team = static_cast<Team*>(world.GetObjectFromId(*it));
		if(!team) continue;
		Surface* emblem = nullptr;
		if(181 < world.resources.spritebank.size() && team->agency < world.resources.spritebank[181].size()) {
			emblem = world.resources.spritebank[181][team->agency].get();
		}
		TeamRow row;
		row.agency = team->agency;
		row.color = team->GetColor();
		row.emblemW = emblem ? emblem->w * 2 : 32;
		row.emblemH = emblem ? emblem->h * 2 : 32;
		for(int i = 0; i < team->numpeers; i++) {
			Peer* peer = world.peerlist[team->peers[i]];
			if(!peer) continue;
			Player* player = world.GetPeerPlayer(peer->id);
			User* user = world.lobby.GetUserInfo(peer->accountid);
			if(player && user) {
				char displayname[120];
				if(peer->isbot) {
					std::snprintf(displayname, sizeof(displayname), "%s [BOT]", user->name);
				}else{
					std::snprintf(displayname, sizeof(displayname), "%s", user->name);
				}
				char stats[100];
				std::snprintf(stats, sizeof(stats), "L:%d    E:%d  S:%d  J:%d  H:%d  C:%d",
				              user->agency[team->agency].level,
				              user->agency[team->agency].endurance,
				              user->agency[team->agency].shield,
				              user->agency[team->agency].jetpack,
				              user->agency[team->agency].hacking,
				              user->agency[team->agency].contacts);
				row.peers.push_back(PeerRow{displayname, stats});
			}
		}
		rows.push_back(row);
	}
	if(rows.empty()) return;

	using namespace silencer::ui::primitives;
	silencer::clay_bridge::EnsureInitialized(surface->w, surface->h);
	Clay_SetPointerState({-1.0f, -1.0f}, false);
	BankTextBeginFrame();
	OverlaysBeginFrame();
	Clay_BeginLayout();

	CLAY({ .id = CLAY_ID("PlayerListRoot"),
	       .layout = {
		       .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) },
		       .padding = { 50, 50, 50, 0 },
		       .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP },
	       } }) {
		CLAY({ .id = CLAY_ID("PlayerListPanel"),
		       .layout = {
			       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED((float)(10 + (rows.size() * 58))) },
			       .padding = { 10, 10, 10, 0 },
			       .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       },
		       .backgroundColor = { 0, 0, 0, 128 },
		}) {
			for(unsigned int teamIndex = 0; teamIndex < rows.size(); ++teamIndex) {
				const TeamRow& row = rows[teamIndex];
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
						       .layout = { .sizing = { CLAY_SIZING_FIXED((float)row.emblemW), CLAY_SIZING_FIXED((float)row.emblemH) } },
						       .custom = { .customData = AllocTeamEmblemCustomData({181, row.agency, row.color, 17, true}) },
						}) {}
					}
					int yoffset = ((4 - (int)row.peers.size()) * 12) / 2;
					if(yoffset < 0) yoffset = 0;
					CLAY({ .id = CLAY_IDI("PlayerListPeerColumn", teamIndex),
					       .layout = {
						       .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
						       .padding = { 0, 0, (uint16_t)yoffset, 0 },
						       .layoutDirection = CLAY_TOP_TO_BOTTOM,
					       } }) {
						for(unsigned int peerIndex = 0; peerIndex < row.peers.size(); ++peerIndex) {
							const PeerRow& peer = row.peers[peerIndex];
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
								       .layout = { .sizing = { CLAY_SIZING_FIXED((float)((peer.stats.size() + 1) * 6)), CLAY_SIZING_FIT(0) } } }) {
									BankText(StringFromStd(peer.stats), BankTextVariant::Body);
								}
							}
						}
					}
				}
			}
		}
	}
	Clay_RenderCommandArray cmds = Clay_EndLayout();
	silencer::clay_bridge::Render(world.resources, renderer, surface, cmds);
}

void InGameOverlayRenderer::DrawAll(Renderer& renderer, World& world, Surface* surface) {
	DrawStatus(renderer, world, surface);
	DrawTopMessage(renderer, world, surface);
	DrawMessage(renderer, world, surface);
	if(world.showplayerlist) {
		DrawPlayerList(renderer, world, surface);
	}
	if(world.quitstate == 1 || world.quitstate == 2) {
		DrawQuitPrompt(renderer, world, surface);
	}
}

void DrawInGameOverlays(Renderer& renderer, World& world, Surface* surface) {
	InGameOverlayRenderer::DrawAll(renderer, world, surface);
}

}  // namespace client_ui
}  // namespace silencer
