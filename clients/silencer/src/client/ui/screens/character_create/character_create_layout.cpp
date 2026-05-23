#include "character_create_screen.h"

#include "screen_context.h"
#include "game_state.h"
#include "game.h"
#include "lobby.h"
#include "renderer.h"
#include "surface.h"
#include "team.h"

#include "clay/clay.h"
#include "clay_ui_compositor.h"
#include "clay_ui_payloads.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/text.h"
#include "primitives/text_input.h"
#include "primitives/toggle.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

namespace character_create_screen_detail {

using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;
using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextOpts;
using silencer::ui::primitives::TextLineHeight;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::TextInput;
using silencer::ui::primitives::TextInputHandle;
using silencer::ui::primitives::TextInputOpts;
using silencer::ui::primitives::Toggle;
using silencer::ui::primitives::ToggleOpts;

constexpr int kStageW = 640;
constexpr int kStageH = 480;
constexpr int kFrameMarginLeft = 5;
constexpr int kFrameMarginRight = 7;
constexpr int kFrameMarginTop = 19;
constexpr int kFrameMarginBottom = 20;
constexpr int kPanelMinW = 628;
constexpr int kPanelMinH = 441;
constexpr int kContentPadLeft = 55;
constexpr int kContentPadRight = 22;
constexpr int kContentPadTop = 18;
constexpr int kContentPadBottom = 34;
constexpr int kColumnGap = 78;
constexpr int kLeftColumnW = 236;
constexpr int kRightColumnW = 196;
constexpr int kTitleH = 33;
// The baked title pill in sprite 7/5 is centered 11px right of the row column.
constexpr int kTitlePlateCenterOffsetX = 11;
constexpr int kTitleTextPadLeft = kTitlePlateCenterOffsetX * 2;
constexpr int kTitleToRowsGap = 14;
constexpr int kRowH = 27;
constexpr int kRowGap = 5;
constexpr int kAgentRowsH = 272;
constexpr int kAgencyRowsH = 168;
constexpr int kDetailTopPad = 42;
constexpr int kAgentDetailPadLeft = 22;
constexpr int kAgentHeaderH = 46;
constexpr int kAgentNameLineTop = 16;
constexpr int kAgentNameIndent = 20;
constexpr int kAgentIconX = 116;
constexpr int kAgentIconW = 32;
constexpr int kAgentIconH = 30;
constexpr int kAgentStatsTopGap = 14;
constexpr int kAgentStatsLineGap = 1;
constexpr int kAgentAdvantagesTopGap = 17;
constexpr int kAgentAdvantagesLineGap = 1;
constexpr int kAdvantageListIndent = 20;
constexpr int kAdvantageMetadataGap = 4;
constexpr Uint8 kAdvantageBracketFontBank = 134;
constexpr Uint16 kAdvantageLeftBracketGlyph = static_cast<Uint16>('[' - 33);
constexpr Uint16 kAdvantageRightBracketGlyph = static_cast<Uint16>(']' - 33);
constexpr int kAdvantageBracketW = 4;
constexpr int kAdvantageBracketH = 11;
constexpr Uint8 kDetailTextColor = 129;
constexpr Uint8 kAdvantageMetadataColor = 224;
constexpr int kAliasModalW = 284;
constexpr int kAliasModalH = 108;
constexpr int kAliasModalTop = 161;
constexpr int kAliasModalOffsetX = 0;
constexpr int kAliasTitleH = 33;
constexpr int kAliasInputX = 24;
constexpr int kAliasInputY = 49;
constexpr int kAliasInputFrameW = 236;
constexpr int kAliasInputW = 224;
constexpr int kMaxRows = 32;

constexpr const char * kActionAgentPrefix = "character_create.agent";
constexpr const char * kActionAgencyPrefix = "character_create.agency";
constexpr const char * kActionCreate = "character_create.create";
constexpr const char * kActionAlias = "character_create.alias";

enum : int {
	kAliasInputUid = 31,
};

struct AgencyDef {
	Uint8 agency;
	const char * name;
	const char * displayName;
	const char * advantages;
	const char * ability;
	const char * description;
};

struct AdvantageLine {
	std::string label;
	std::string metadata;
};

constexpr int kAdvantageGlyphPayloadCapacity = 32;
silencer::clay_bridge::SpritePayload g_advantageGlyphPayloads[
	kAdvantageGlyphPayloadCapacity];
silencer::clay_bridge::ClayCustomData g_advantageGlyphCustomData[
	kAdvantageGlyphPayloadCapacity];
int g_advantageGlyphPayloadCount = 0;
int g_advantageGlyphCustomDataCount = 0;

const AgencyDef kAgencies[5] = {
	{ Team::NOXIS,
	  "Noxis",
	  "Noxis",
	  "Endurance +3\nJump +5",
	  "Health Packs",
	  "The Noxis corporation terraformed the majority of the initial habitable sectors of Mars, and continues to do so, as well as producing and selling 70 percent of the breathable oxygen. Since they are widely known to the populace and government, they have taken steps to bolster their agent's health so they are better able to avoid detection. Training in bio-sporria rich environments, and possessing suits with advanced oxygen processors and filters, has given these agents improved physical abilities such as higher jumps, more stamina, and enhanced durability." },
	{ Team::LAZARUS,
	  "Lazarus",
	  "Lazarus",
	  "Resurrection Ability",
	  "Resurrection",
	  "Like the mythical phoenix, the loose organization known as Lazarus has been resurrected every hundred years and causing chaos in the larger urban areas of Mars. Whether it's belief in some higher power, an understanding of mystical truths, or something else entirely, no one outside of the group is sure of this Lazarus. Numerous witnesses of Lazarus attacks claim to have seen supposedly fatally wounded agents continue themselves off and continue their efforts, virtually undamaged." },
	{ Team::CALIBER,
	  "Caliber",
	  "Caliber",
	  "Contacts +3",
	  "Security Passes",
	  "Caliber agents specialize in access, favors, and fast-moving procurement. Their contacts let them gather better mission rewards and keep security pressure away from their teams for longer than most agencies can manage." },
	{ Team::STATIC,
	  "Static",
	  "Static",
	  "Satellite Ability\nHacking +3",
	  "Virus",
	  "The Guv first realized that there was a brain-drain about four years ago. Silently, the youngest talented computer specialists and high level programmers were disappearing and leaving the Guv without the technical support it required. They ended up creating Static as a technological savvy, but anti-government agency, but many members are more interested in reinforcing their reputation as the best skilled hackers with the best skills and techniques." },
	{ Team::BLACKROSE,
	  "Black Rose",
	  "Black Rose",
	  "Stealth Ability\nShield +2",
	  "Poison Flare",
	  "More like a dark force than a loose collection of humans, little is known of Black Rose. Something vibrant was taken from these people, and left in its place was a new form of sustenance. Its members seem to be misanthropic and have a penchant for using illegal compounds such as Milloxthal injections to cause extreme pain and internal withering in their victims. Shunned by even the most avaristic mercenaries, these agents work alone and yet still prosper due to some combination of ego and superlative talent." },
};

Clay_String FromStd(const std::string & s)
{
	return Clay_String{ false, static_cast<int32_t>(s.size()), s.c_str() };
}

Clay_String FromCStr(const char * s)
{
	return Clay_String{ false, static_cast<int32_t>(std::strlen(s)), s };
}

bool StartsWith(const std::string& value, const char * prefix)
{
	const size_t n = std::strlen(prefix);
	return value.size() >= n && value.compare(0, n, prefix) == 0;
}

int SuffixInt(const std::string& value, const char * prefix)
{
	if(!StartsWith(value, prefix)) return -1;
	const char * suffix = value.c_str() + std::strlen(prefix);
	if(*suffix == '.') ++suffix;
	return std::atoi(suffix);
}

int AgencyIndex(Uint8 agency)
{
	for(int i = 0; i < 5; ++i){
		if(kAgencies[i].agency == agency) return i;
	}
	return 0;
}

const AgencyDef& SelectedAgency(Uint8 agency)
{
	return kAgencies[AgencyIndex(agency)];
}

void Title(Clay_String text)
{
	Text(text, { .size = TextSize::ScreenTitle,
	             .effect = TextEffect::LegacyPalette(0) });
}

void DetailHeading(Clay_String text)
{
	Text(text, { .size = TextSize::Heading,
	             .effect = TextEffect::LegacyPalette(kDetailTextColor, 160, true) });
}

void DetailTitleText(Clay_String text)
{
	Text(text, { .size = TextSize::Heading,
	             .effect = TextEffect::LegacyPalette(kDetailTextColor, 160, true) });
}

void DetailStatText(Clay_String text)
{
	Text(text, { .size = TextSize::BodySm,
	             .effect = TextEffect::LegacyPalette(kDetailTextColor, 160, true) });
}

void DetailText(Clay_String text)
{
	Text(text, { .size = TextSize::BodySm,
	             .wrap = silencer::ui::primitives::TextWrap::Newlines,
	             .effect = TextEffect::LegacyPalette(kDetailTextColor, 160, true) });
}

std::string TrimCopy(const std::string& value)
{
	size_t begin = 0;
	while(begin < value.size() &&
	      std::isspace(static_cast<unsigned char>(value[begin]))){
		++begin;
	}
	size_t end = value.size();
	while(end > begin &&
	      std::isspace(static_cast<unsigned char>(value[end - 1]))){
		--end;
	}
	return value.substr(begin, end - begin);
}

bool IsNumericMetadata(const std::string& value)
{
	if(value.size() < 2) return false;
	if(value[0] != '+' && value[0] != '-') return false;
	for(size_t i = 1; i < value.size(); ++i){
		if(!std::isdigit(static_cast<unsigned char>(value[i]))) return false;
	}
	return true;
}

AdvantageLine ParseAdvantageLine(const std::string& raw)
{
	AdvantageLine line;
	std::string text = TrimCopy(raw);
	const std::string abilitySuffix = " Ability";
	if(text.size() > abilitySuffix.size() &&
	   text.compare(text.size() - abilitySuffix.size(),
	                abilitySuffix.size(),
	                abilitySuffix) == 0){
		line.label = TrimCopy(text.substr(0, text.size() - abilitySuffix.size()));
		line.metadata = "Ability";
		return line;
	}
	size_t split = text.find_last_of(' ');
	if(split != std::string::npos){
		std::string suffix = TrimCopy(text.substr(split + 1));
		if(IsNumericMetadata(suffix)){
			line.label = TrimCopy(text.substr(0, split));
			line.metadata = suffix;
			return line;
		}
	}
	line.label = text;
	return line;
}

std::vector<AdvantageLine> ParseAdvantageLines(const char * text)
{
	std::vector<AdvantageLine> out;
	if(!text) return out;
	const char * start = text;
	for(const char * p = text; ; ++p){
		if(*p == '\n' || *p == '\0'){
			out.push_back(ParseAdvantageLine(std::string(start, p - start)));
			if(*p == '\0') break;
			start = p + 1;
		}
	}
	return out;
}

void ResetAdvantageGlyphPayloads()
{
	g_advantageGlyphPayloadCount = 0;
	g_advantageGlyphCustomDataCount = 0;
}

silencer::clay_bridge::SpritePayload *
AllocAdvantageGlyphPayload(Uint16 glyph, Sint16 srcX)
{
	if(g_advantageGlyphPayloadCount >= kAdvantageGlyphPayloadCapacity) return nullptr;
	auto * p = &g_advantageGlyphPayloads[g_advantageGlyphPayloadCount++];
	*p = silencer::clay_bridge::SpritePayload(
		kAdvantageBracketFontBank,
		glyph,
		srcX,
		0,
		kAdvantageBracketW,
		kAdvantageBracketH,
		0,
		128,
		kAdvantageMetadataColor,
		0,
		0,
		0);
	return p;
}

silencer::clay_bridge::ClayCustomData *
AllocAdvantageGlyphCustomData(void * payload)
{
	if(g_advantageGlyphCustomDataCount >= kAdvantageGlyphPayloadCapacity) return nullptr;
	auto * c = &g_advantageGlyphCustomData[g_advantageGlyphCustomDataCount++];
	c->kind = silencer::clay_bridge::CustomKind::Sprite;
	c->payload = payload;
	return c;
}

void AdvantageBracket(Clay_String id, int index, bool left)
{
	const Uint16 glyph = left ? kAdvantageLeftBracketGlyph
	                          : kAdvantageRightBracketGlyph;
	const Sint16 srcX = left ? 0 : 1;
	auto * payload = AllocAdvantageGlyphPayload(glyph, srcX);
	auto * ccd = AllocAdvantageGlyphCustomData(payload);
	CLAY({ .id = CLAY_SIDI(id, static_cast<uint32_t>(0x300 + index * 2 +
	                                                (left ? 0 : 1))),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(kAdvantageBracketW),
	                       CLAY_SIZING_FIXED(kAdvantageBracketH) },
	       },
	       .custom = { .customData = ccd } }) {}
}

void AdvantageLineText(Clay_String id, int index, const AdvantageLine& line)
{
	CLAY({ .id = CLAY_SIDI(id, static_cast<uint32_t>(0x100 + index)),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
	           .padding = { kAdvantageListIndent, 0, 0, 0 },
	       } }) {
		CLAY({ .id = CLAY_SIDI(id, static_cast<uint32_t>(0x200 + index)),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
		           .childGap = 0,
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       } }) {
			Text(FromStd(line.label),
			     { .size = TextSize::BodySm,
			       .effect = TextEffect::LegacyPalette(kDetailTextColor, 160, true) });
			if(!line.metadata.empty()){
				const std::string& metadata = line.metadata;
				CLAY({ .id = CLAY_SIDI(id, static_cast<uint32_t>(0x280 + index)),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIXED(kAdvantageMetadataGap),
				                       CLAY_SIZING_FIXED(1) },
				       } }) {}
				AdvantageBracket(id, index, true);
				Text(FromStd(metadata),
				     { .size = TextSize::BodySm,
				       .effect = TextEffect::LegacyPalette(kAdvantageMetadataColor,
				                                           128,
				                                           true) });
				AdvantageBracket(id, index, false);
			}
		}
	}
}

void AdvantageLines(Clay_String id, const char * text)
{
	std::vector<AdvantageLine> lines = ParseAdvantageLines(text);
	int rendered = 0;
	for(const AdvantageLine& line : lines){
		if(line.metadata == "Ability") continue;
		AdvantageLineText(id, rendered++, line);
	}
	for(const AdvantageLine& line : lines){
		if(line.metadata != "Ability") continue;
		AdvantageLineText(id, rendered++, line);
	}
}

void AgentDetailHeader(const AgencyDef& agency)
{
	CLAY({ .id = CLAY_ID("CharacterCreateAgentAgencyHeader"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kAgentHeaderH) },
	       } }) {
		DetailTitleText(CLAY_STRING("Agency"));
		CLAY({ .id = CLAY_ID("CharacterCreateAgentAgencyNameLine"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED(static_cast<float>(
		                           TextLineHeight(TextSize::Heading))) },
		           .padding = { kAgentNameIndent, 0, 0, 0 },
		       },
		       .floating = {
		           .offset = { 0.0f, static_cast<float>(kAgentNameLineTop) },
		           .zIndex = 1,
		           .attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_TOP,
		                             .parent = CLAY_ATTACH_POINT_LEFT_TOP },
		           .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
		           .attachTo = CLAY_ATTACH_TO_PARENT,
		       } }) {
			DetailTitleText(FromCStr(agency.displayName));
		}
		CLAY({ .id = CLAY_ID("CharacterCreateAgentAgencyIconFloat"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kAgentIconW),
		                       CLAY_SIZING_FIXED(kAgentIconH) },
		       },
		       .floating = {
		           .offset = { static_cast<float>(kAgentIconX), 0.0f },
		           .zIndex = 1,
		           .attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_TOP,
		                             .parent = CLAY_ATTACH_POINT_LEFT_TOP },
		           .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
		           .attachTo = CLAY_ATTACH_TO_PARENT,
		       } }) {
			Toggle(CLAY_STRING("CharacterCreateAgentAgencyIcon"),
			       181,
			       agency.agency,
			       true,
			       ToggleOpts{ .width = kAgentIconW,
			                   .height = kAgentIconH,
			                   .effectColor = 112,
			                   .selectedBrightness = 128,
			                   .unselectedBrightness = 128 });
		}
	}
}

void Spacer(Clay_String id, int height)
{
	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_FIXED(static_cast<float>(height)) },
	       } }) {}
}

void AgentStatsBlock(Clay_String id, const std::vector<std::string>& lines)
{
	CLAY({ .id = CLAY_SID(id),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
	           .childGap = kAgentStatsLineGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		int index = 0;
		for(const std::string& line : lines){
				CLAY({ .id = CLAY_SIDI(id, static_cast<uint32_t>(index)),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0),
				                       CLAY_SIZING_FIXED(static_cast<float>(
				                           TextLineHeight(TextSize::BodySm))) },
			       } }) {
				DetailStatText(FromStd(line));
			}
			++index;
		}
	}
}

void DetailParagraph(Clay_String text)
{
	Text(text, { .size = TextSize::BodySm,
	             .wrap = silencer::ui::primitives::TextWrap::Words,
	             .effect = TextEffect::LegacyPalette(kDetailTextColor, 160, true) });
}

void ListButton(Clay_String id,
                Clay_String label,
                const char * action,
                silencer::ui::UiInteractionRegistry& interactions,
                bool selected = false,
                bool selectedVisual = true,
                bool * hoveredOut = nullptr,
                bool disabled = false)
{
	Button(id,
	       label,
	       ButtonOpts{ .variant = ButtonVariant::LegacyRow,
	                   .size = ButtonSize::Auto,
	                   .disabled = disabled,
	                   .selected = selected,
	                   .selectedVisual = selectedVisual,
	                   .minWidth = kLeftColumnW },
	       ButtonHandle{ hoveredOut, action, &interactions });
}

std::string AgentActionId(int index)
{
	std::string out = kActionAgentPrefix;
	out += ".";
	out += std::to_string(index);
	return out;
}

std::string AgencyActionId(int index)
{
	std::string out = kActionAgencyPrefix;
	out += ".";
	out += std::to_string(index);
	return out;
}

const AgencyDef& AgencyByIndex(int index)
{
	if(index < 0 || index >= 5) return kAgencies[0];
	return kAgencies[index];
}

int CreateRowIndex(size_t characterCount)
{
	return std::min(static_cast<int>(characterCount), kMaxRows - 1);
}

}  // namespace character_create_screen_detail

void CharacterCreateScreen::BuildUi(ScreenContext & ctx,
                                    Surface & dst,
                                    float frametime,
                                    silencer::ui::UiInteractionRegistry& interactions)
{
	(void)dst;
	(void)frametime;
	using namespace silencer::clay_bridge;
	using namespace character_create_screen_detail;

	ResetAdvantageGlyphPayloads();
	RebuildAgentRows(ctx);
	if(agentScrollDelta != 0){
		int next = static_cast<int>(agentScroll) + agentScrollDelta;
		const int visible = kAgentRowsH / (kRowH + kRowGap);
		const int maxScroll = std::max(0, static_cast<int>(agentRows.size()) - visible);
		if(next < 0) next = 0;
		if(next > maxScroll) next = maxScroll;
		agentScroll = static_cast<Uint16>(next);
		agentScrollDelta = 0;
	}

	CLAY({ .id = CLAY_ID("CharacterCreateRoot"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0),
	                       CLAY_SIZING_GROW(0) },
	           .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
	       },
	       .image = { .imageData = PackImage(6, 0) } }) {
		CLAY({ .id = CLAY_ID("CharacterCreateStage"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kStageW),
		                       CLAY_SIZING_FIXED(kStageH) },
		           .padding = { kFrameMarginLeft,
		                        kFrameMarginRight,
		                        kFrameMarginTop,
		                        kFrameMarginBottom },
		       } }) {
			CLAY({ .id = CLAY_ID("CharacterCreatePanel"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIXED(kPanelMinW),
			                       CLAY_SIZING_FIXED(kPanelMinH) },
			           .padding = { kContentPadLeft,
			                        kContentPadRight,
			                        kContentPadTop,
			                        kContentPadBottom },
			           .childGap = kColumnGap,
			           .layoutDirection = CLAY_LEFT_TO_RIGHT,
			       },
			       .image = { .imageData = PackImageStretch(7, 5) } }) {
				if(step == Step::SelectAgent){
					BuildSelectAgent(ctx, interactions);
				}else if(step == Step::EnterAlias){
					BuildEnterAlias(ctx, interactions);
				}else{
					BuildSelectAgency(ctx, interactions);
				}
			}
		}
	}
}

void CharacterCreateScreen::BuildSelectAgent(ScreenContext & ctx,
                                             silencer::ui::UiInteractionRegistry& interactions,
                                             bool interactive)
{
	using namespace character_create_screen_detail;
	int detailAgentIndex = previewAgentIndex >= 0
		? previewAgentIndex
		: selectedAgentIndex;

	CLAY({ .id = CLAY_ID("CharacterCreateAgentColumn"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(kLeftColumnW),
	                       CLAY_SIZING_GROW(0) },
	           .childGap = kRowGap,
	           .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("CharacterCreateAgentTitle"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kTitleH) },
		           .padding = { kTitleTextPadLeft, 0, 0, 0 },
		           .childAlignment = { .x = CLAY_ALIGN_X_CENTER,
		                               .y = CLAY_ALIGN_Y_CENTER },
		} }) {
			Title(CLAY_STRING("SELECT AGENT"));
		}
		CLAY({ .id = CLAY_ID("CharacterCreateAgentTitleSpacer"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kTitleToRowsGap) },
		       } }) {}
		CLAY({ .id = CLAY_ID("CharacterCreateAgentRows"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kAgentRowsH) },
		           .childGap = kRowGap,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			const int visibleRows = kAgentRowsH / (kRowH + kRowGap);
			const int count = std::min(static_cast<int>(agentRows.size()), kMaxRows);
			for(int row = 0; row < visibleRows; ++row){
				const int index = static_cast<int>(agentScroll) + row;
				if(index < 0 || index >= count) break;
				const std::string id = std::string("CharacterCreateAgentRow") + std::to_string(index);
				const std::string action = AgentActionId(index);
				const bool existingAgent =
					index < static_cast<int>(ctx.lobby.characters.size());
				bool hovered = false;
				const bool selected = existingAgent && index == selectedAgentIndex;
				const bool selectedVisual = selected && previewAgentIndex == index;
				ListButton(FromStd(id),
				           FromStd(agentRows[static_cast<size_t>(index)]),
				           interactive ? action.c_str() : nullptr,
				           interactions,
				           selected,
				           selectedVisual,
				           interactive ? &hovered : nullptr);
				const auto * snapshot = interactive ? interactions.FindById(action) : nullptr;
				if(interactive && (hovered || (snapshot && snapshot->focused)) && existingAgent){
					detailAgentIndex = index;
				}
			}
		}
	}

		CLAY({ .id = CLAY_ID("CharacterCreateAgentDetails"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kRightColumnW),
		                       CLAY_SIZING_GROW(0) },
		           .padding = { kAgentDetailPadLeft, 0, kDetailTopPad, 0 },
		           .childGap = 0,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			const int createIndex = CreateRowIndex(ctx.lobby.characters.size());
			if(detailAgentIndex >= 0 &&
			   detailAgentIndex < static_cast<int>(ctx.lobby.characters.size()) &&
			   detailAgentIndex != createIndex){
				const Lobby::Character& ch = ctx.lobby.characters[static_cast<size_t>(detailAgentIndex)];
				const AgencyDef& agency = AgencyByIndex(static_cast<int>(ch.agencyIdx));
				AgentDetailHeader(agency);
				std::string security = std::string("Security Level ") + std::to_string(ch.stats.level);
				DetailTitleText(FromStd(security));
				Spacer(CLAY_STRING("CharacterCreateAgentStatsTopGap"),
				       kAgentStatsTopGap);
				std::vector<std::string> statLines;
				statLines.push_back(std::string("Technology Slots:") + std::to_string(ch.stats.techslots));
				statLines.push_back(std::string("Agency Points:") + std::to_string(ch.stats.xp));
				statLines.push_back(std::string("Next Level At:") + std::to_string((ch.stats.level + 1) * 100));
				statLines.push_back(std::string("Successful Missions:") + std::to_string(ch.stats.wins));
				AgentStatsBlock(CLAY_STRING("CharacterCreateAgentStats"),
				                statLines);
				Spacer(CLAY_STRING("CharacterCreateAgentAdvantagesTopGap"),
				       kAgentAdvantagesTopGap);
				CLAY({ .id = CLAY_ID("CharacterCreateAgentAdvantages"),
				       .layout = {
				           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
				           .childGap = kAgentAdvantagesLineGap,
				           .layoutDirection = CLAY_TOP_TO_BOTTOM,
				       } }) {
					DetailHeading(CLAY_STRING("Advantages"));
					AdvantageLines(CLAY_STRING("CharacterCreateAgentAdvantageList"),
					               agency.advantages);
				}
			}
		}
	}

void CharacterCreateScreen::BuildEnterAlias(ScreenContext & ctx,
                                            silencer::ui::UiInteractionRegistry& interactions)
{
	using namespace character_create_screen_detail;
	const bool focused = interactions.IsTextInputFocused(kAliasInputUid);
	const bool blink = (ctx.renderer.GetHudAnimationPhase() % 32) < 16;

	BuildSelectAgent(ctx, interactions, false);

	CLAY({ .id = CLAY_ID("CharacterAliasModalWrap"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(kAliasModalW),
	                       CLAY_SIZING_FIXED(kAliasModalH) },
	       },
	       .image = { .imageData = silencer::clay_bridge::PackImage(40, 2) },
	       .floating = {
	           .offset = { static_cast<float>(kAliasModalOffsetX),
	                       static_cast<float>(kAliasModalTop +
	                                          (kAliasModalH / 2) -
	                                          kFrameMarginTop -
	                                          (kPanelMinH / 2)) },
	           .zIndex = 2,
	           .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_CENTER,
	                             .parent = CLAY_ATTACH_POINT_CENTER_CENTER },
	           .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
	           .attachTo = CLAY_ATTACH_TO_PARENT,
	       } }) {
		CLAY({ .id = CLAY_ID("CharacterAliasTitleFloat"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kAliasModalW),
		                       CLAY_SIZING_FIXED(kAliasTitleH) },
		           .childAlignment = { .x = CLAY_ALIGN_X_CENTER,
		                               .y = CLAY_ALIGN_Y_CENTER },
		       },
		       .floating = {
		           .offset = { 0.0f, 0.0f },
		           .zIndex = 3,
		           .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_TOP,
		                             .parent = CLAY_ATTACH_POINT_CENTER_TOP },
		           .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
		           .attachTo = CLAY_ATTACH_TO_PARENT,
		       } }) {
			Text(CLAY_STRING("SILENCER ALIAS"),
			     { .size = TextSize::Title,
			       .effect = TextEffect::LegacyPalette(0) });
		}
		CLAY({ .id = CLAY_ID("CharacterAliasInputFloat"),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(kAliasInputFrameW),
		                       CLAY_SIZING_FIXED(kRowH) },
		       },
		       .floating = {
		           .offset = { static_cast<float>(kAliasInputX),
		                       static_cast<float>(kAliasInputY) },
		           .zIndex = 3,
		           .attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_TOP,
		                             .parent = CLAY_ATTACH_POINT_LEFT_TOP },
		           .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE,
		           .attachTo = CLAY_ATTACH_TO_PARENT,
		       } }) {
			TextInput(CLAY_STRING("CharacterAliasInput"),
			          alias,
			          TextInputOpts{ .widthPx = kAliasInputFrameW,
			                         .heightPx = kRowH,
			                         .textSize = TextSize::Body,
			                         .showCaret = focused && blink,
			                         .contentInsetX = static_cast<Uint16>(kAliasInputFrameW - kAliasInputW) },
			          TextInputHandle{ nullptr,
			                           kActionAlias,
			                           "Alias",
			                           &interactions,
			                           kAliasInputUid,
			                           16,
			                           true });
		}
		if(focusAliasRequested){
			ctx.game.UiInteractions().FocusTextInputByUid(kAliasInputUid);
			focusAliasRequested = false;
		}
	}
}

void CharacterCreateScreen::BuildSelectAgency(ScreenContext & ctx,
                                              silencer::ui::UiInteractionRegistry& interactions)
{
	(void)ctx;
	using namespace character_create_screen_detail;
	const int initialDetailAgencyIndex = previewAgencyIndex >= 0
		? previewAgencyIndex
		: AgencyIndex(selectedAgency);
	Uint8 detailAgency = AgencyByIndex(initialDetailAgencyIndex).agency;

	CLAY({ .id = CLAY_ID("CharacterAgencyColumn"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(kLeftColumnW),
	                       CLAY_SIZING_GROW(0) },
	           .childGap = kRowGap,
	           .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		CLAY({ .id = CLAY_ID("CharacterAgencyTitle"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kTitleH) },
		           .padding = { kTitleTextPadLeft, 0, 0, 0 },
		           .childAlignment = { .x = CLAY_ALIGN_X_CENTER,
		                               .y = CLAY_ALIGN_Y_CENTER },
		} }) {
			Title(CLAY_STRING("SELECT AGENCY"));
		}
		CLAY({ .id = CLAY_ID("CharacterAgencyTitleSpacer"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kTitleToRowsGap) },
		       } }) {}
		CLAY({ .id = CLAY_ID("CharacterAgencyRows"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kAgencyRowsH) },
		           .childGap = kRowGap,
		           .layoutDirection = CLAY_TOP_TO_BOTTOM,
		       } }) {
			for(int i = 0; i < 5; ++i){
				const std::string id = std::string("CharacterAgencyRow") + std::to_string(i);
				const std::string action = AgencyActionId(i);
				bool hovered = false;
				const bool selected = kAgencies[i].agency == selectedAgency;
				const bool selectedVisual = selected && previewAgencyIndex == i;
				ListButton(FromStd(id),
				           FromCStr(kAgencies[i].displayName),
				           action.c_str(),
				           interactions,
				           selected,
				           selectedVisual,
				           &hovered,
				           waitingForCreate);
				const auto * row = interactions.FindById(action);
				if(!waitingForCreate && (hovered || (row && row->focused))){
					detailAgency = kAgencies[i].agency;
				}
			}
		}
	}

	const AgencyDef& agency = SelectedAgency(detailAgency);
	CLAY({ .id = CLAY_ID("CharacterAgencyInfo"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(kRightColumnW),
	                       CLAY_SIZING_GROW(0) },
	           .padding = { 0, 0, kDetailTopPad, 0 },
	           .childGap = 8,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       } }) {
		DetailHeading(CLAY_STRING("Advantages"));
		AdvantageLines(CLAY_STRING("CharacterAgencyAdvantageList"),
		               agency.advantages);
		DetailHeading(CLAY_STRING("Description"));
		CLAY({ .id = CLAY_ID("CharacterAgencyDescription"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
		       } }) {
			DetailParagraph(FromCStr(agency.description));
		}
	}
}
