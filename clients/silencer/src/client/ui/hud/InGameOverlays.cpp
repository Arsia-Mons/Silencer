#include "client/ui/hud/InGameOverlays.h"

#include "client/ui/hud/HudClayHelpers.h"
#include "client/ui/hud/hud_player_list_overlay.h"
#include "client/ui/views/HudView.h"
#include "clay_ui_compositor.h"
#include "render/renderer.h"
#include "resources.h"
#include "surface.h"
#include "ui/primitives/text.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace silencer {
namespace client_ui {

namespace ingameoverlays_detail {

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextAdvance;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextLineHeight;
using silencer::ui::primitives::TextSize;

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
		TextSize size;
		Uint8 color;
		TextEffect effect;
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
	TextSize textSize = TextSize::Title;
	int lineheight = 20;
	switch(msg.messagetype) {
		case 1: color = 128; liney = 190; textSize = TextSize::MessageHeading; break;
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
				textSize = TextSize::MessageTitle;
			}else{
				textSize = TextSize::MessageSubtitle;
			}
		}
		Uint8 brightness2 = (int(brightness) - 64) < 8 ? 8 : brightness - 64;
		const int advance = TextAdvance(textSize);
		const int x = ((surface->w - (linelength * advance)) / 2) + (advance * (linelength - nextline));
		glyphs.push_back({temp, x + 1, liney + 1, textSize, color,
		                 TextEffect::LegacyPalette(color, brightness2)});
		glyphs.push_back({temp, x, liney, textSize, color,
		                 TextEffect::LegacyPalette(color, brightness)});
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
			CLAY(FloatingTextElementI("InGameMessageGlyph", (uint32_t)i,
			                          glyph.x, glyph.y,
			                          TextAdvance(glyph.size),
			                          TextLineHeight(glyph.size))) {
				Text(ClayStringFromStd(glyph.text),
				     { .size = glyph.size,
				       .effect = glyph.effect });
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
		TextEffect effect;
	};
	std::vector<StatusLine> lines;
	lines.reserve(view.statusMessages.size() * 2);
	int liney = 0;
	for(const InGameStatusLineView& src : view.statusMessages) {
		Uint8 brightness = 128;
		if(src.time <= 16) brightness -= (16 - src.time) * 8;
		Uint8 brightness2 = (int(brightness) - 64) < 8 ? 8 : brightness - 64;
		const int x = (surface->w - ((int)src.text.size() * TextAdvance(TextSize::BodySm))) / 2;
		lines.push_back({src.text, x + 1, 370 + liney + 1, src.color,
		                TextEffect::LegacyPalette(src.color, brightness2)});
		lines.push_back({src.text, x, 370 + liney, src.color,
		                TextEffect::LegacyPalette(src.color, brightness)});
		liney -= 10;
	}
	if(lines.empty()) return;
	CLAY({ .id = CLAY_ID("InGameStatusRoot"),
	       .layout = { .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) } } }) {
		for(size_t i = 0; i < lines.size(); ++i) {
			StatusLine& line = lines[i];
			CLAY(FloatingTextElementI("InGameStatusLine", (uint32_t)i, line.x, line.y,
			                          (int)line.text.size() * TextAdvance(TextSize::BodySm),
			                          TextLineHeight(TextSize::BodySm))) {
				Text(ClayStringFromStd(line.text),
				     { .size = TextSize::BodySm,
				       .effect = line.effect });
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
			Text(ClayStringFromStd(topText), { .size = TextSize::BodySm });
		}
	}
}

void DrawQuitPrompt(const HudView& /*view*/, Surface* surface) {
#ifdef OUYA
	std::string text = "Hit O To QUIT";
#else
	std::string text = "Hit Enter To Quit";
#endif
	const int width = (int)text.size() * TextAdvance(TextSize::Prompt);
	CLAY({ .id = CLAY_ID("QuitPromptRoot"),
	       .layout = { .sizing = { CLAY_SIZING_FIXED((float)surface->w), CLAY_SIZING_FIXED((float)surface->h) } } }) {
		CLAY(FloatingTextElement("QuitPromptText", (surface->w - width) / 2, 200,
		                         width, TextLineHeight(TextSize::Prompt))) {
			Text(ClayStringFromStd(text),
			     { .size = TextSize::Prompt,
			       .effect = TextEffect::LegacyPalette(202) });
		}
	}
}

}  // namespace ingameoverlays_detail

void BuildInGameOverlaysUi(Renderer& /*renderer*/, const Resources& /*resources*/,
                           const HudView& view, Surface* surface, bool drawLegacyTextOverlays) {
	if(!view.mapLoaded) return;
	if(drawLegacyTextOverlays){
		ingameoverlays_detail::DrawStatus(view, surface);
		ingameoverlays_detail::DrawTopMessage(view, surface);
	}
	if(drawLegacyTextOverlays){
		ingameoverlays_detail::DrawMessage(view, surface);
	}
	if(drawLegacyTextOverlays && view.showPlayerList){
		BuildPlayerListOverlay(view, surface);
	}
	if(drawLegacyTextOverlays && (view.quitState == 1 || view.quitState == 2)){
		ingameoverlays_detail::DrawQuitPrompt(view, surface);
	}
}

}  // namespace client_ui
}  // namespace silencer
