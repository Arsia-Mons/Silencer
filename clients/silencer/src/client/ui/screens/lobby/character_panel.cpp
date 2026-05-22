#include "character_panel.h"

#include "clay/clay.h"
#include "clay_ui_payloads.h"
#include "runtime/UiInteractionRegistry.h"
#include "primitives/button.h"
#include "primitives/text.h"

#include "config.h"
#include "lobby.h"
#include "resources.h"
#include "user.h"
#include "world.h"

#include <algorithm>
#include <cstring>
#include <string>

using silencer::ui::primitives::Text;
using silencer::ui::primitives::TextEffect;
using silencer::ui::primitives::TextOpts;
using silencer::ui::primitives::TextAdvance;
using silencer::ui::primitives::TextLineHeight;
using silencer::ui::primitives::MeasureText;
using silencer::ui::primitives::TextSize;
using silencer::ui::primitives::TextWrap;
using silencer::ui::primitives::Button;
using silencer::ui::primitives::ButtonHandle;
using silencer::ui::primitives::ButtonOpts;
using silencer::ui::primitives::ButtonSize;
using silencer::ui::primitives::ButtonVariant;

namespace silencer::client_ui::lobby {

namespace character_panel_detail {

constexpr const char * kActionAgents = "lobby.character.agents";
constexpr uint16_t kPanelPad = 6;
constexpr uint16_t kBandGap = 4;
constexpr uint16_t kStatRowGap = 2;
constexpr uint16_t kEmblemGap = 10;
constexpr uint16_t kDetailsGap = 8;
constexpr uint16_t kButtonHeight = 21;
constexpr int kActionButtonMinWidth = 92;
constexpr int kActionButtonPaddingX = 12;
constexpr uint16_t kAgencySpriteBank = 181;
// Emblem occupies a fixed slice of the inner width and grows to the body
// height; the IMAGE compositor scales the sprite (Contain) to fit, so a big
// crest anchors the left instead of a lost native-size icon.
constexpr int kEmblemWidthPct = 31;
constexpr int kEmblemMinWidth = 48;
constexpr int kEmblemMaxWidth = 112;

// Per-frame text buffers. The layout pass keeps pointers to these for the
// duration of the layout, so they MUST live past BuildCharacterPanelTree's
// return. Static-lifetime works because the layout consumes them
// synchronously inside the caller's BeginLayout/EndLayout window.
struct StatsBuffers {
	std::string name;
	std::string level;
	std::string wins;
	std::string losses;
};
StatsBuffers g_stats;

Clay_String FromStd(const std::string & s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(s.size());
	cs.chars = s.c_str();
	return cs;
}

Clay_String FromCStr(const char * s) {
	Clay_String cs;
	cs.isStaticallyAllocated = false;
	cs.length = static_cast<int32_t>(std::strlen(s));
	cs.chars = s;
	return cs;
}

int ClampInt(int value, int lo, int hi) {
	return std::max(lo, std::min(value, hi));
}

std::string FitMiddleEllipsis(const std::string& text,
                              TextSize size,
                              int availablePx) {
	const int advance = std::max(1, static_cast<int>(TextAdvance(size)));
	const int maxChars = availablePx / advance;
	if(maxChars <= 0) return "";
	if(static_cast<int>(text.size()) <= maxChars) return text;
	if(maxChars <= 3) return text.substr(0, static_cast<size_t>(maxChars));

	const int kept = maxChars - 3;
	const int front = (kept + 1) / 2;
	const int back = kept - front;
	return text.substr(0, static_cast<size_t>(front)) + "..." +
	       text.substr(text.size() - static_cast<size_t>(back));
}

// One stat row: a fixed-width label column followed by its value, kept as a
// tight pair (the Game Options label/value convention). The fixed label
// column aligns all three values into a clean column. Rows grow to fill the
// table height; styling is default Body text.
void StatRow(int index,
             const char * label,
             const std::string& value,
             int labelColumnWidth) {
	CLAY({ .id = CLAY_IDI("CharacterPanelStatRow", index),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0) },
	           .childGap = 8,
	           .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
	                               .y = CLAY_ALIGN_Y_CENTER },
	           .layoutDirection = CLAY_LEFT_TO_RIGHT,
	       },
	       .clip = { .horizontal = true } }) {
		CLAY({ .id = CLAY_IDI("CharacterPanelStatLabel", index),
		       .layout = {
		           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(labelColumnWidth)),
		                       CLAY_SIZING_GROW(0) },
		           .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
		                               .y = CLAY_ALIGN_Y_CENTER },
		       },
		       .clip = { .horizontal = true } }) {
			Text(FromCStr(label),
			     TextOpts{ .size = TextSize::Body, .wrap = TextWrap::None });
		}
		Text(FromStd(value),
		     TextOpts{ .size = TextSize::Body, .wrap = TextWrap::None });
	}
}

// Render the agency crest as a scaling IMAGE element: it fills a fixed-width,
// body-height box and the compositor scales the sprite up (Contain, crisp
// nearest sampling) in its own palette — no green tint, no native-size cap.
void AgencyEmblem(Uint8 agency, int boxWidth) {
	CLAY({ .id = CLAY_ID("CharacterPanelAgencyEmblem"),
	       .layout = {
	           .sizing = { CLAY_SIZING_FIXED(static_cast<float>(boxWidth)),
	                       CLAY_SIZING_GROW(0) },
	       },
	       .image = { .imageData = silencer::clay_bridge::PackImageContain(
	           static_cast<Uint8>(kAgencySpriteBank), agency) } }) {}
}

}  // namespace character_panel_detail

void CharacterPanelInit(CharacterPanelState & state) {
	state.selectedAgency = Config::GetInstance().defaultagency;
	state.lastReconciled = -1;  // forces first-frame reconcile pass
	state.newCharacterRequested = false;
}

void CharacterPanelTick(CharacterPanelState & state, World & world) {
	state.selectedAgency = world.lobby.GetSelectedAgencyOrDefault(Config::GetInstance().defaultagency);
	if(static_cast<int>(state.selectedAgency) != state.lastReconciled){
		state.lastReconciled = state.selectedAgency;
		if(world.IsConnected()){
			world.SetAgency(state.selectedAgency);
		}
	}
}

bool CharacterPanelHandleUiIntent(CharacterPanelState & state,
	const silencer::ui::UiAction & action) {
	if(action.kind != silencer::ui::UiActionKind::Activate) return false;
	if(action.id == character_panel_detail::kActionAgents){
		state.newCharacterRequested = true;
		return true;
	}
	return false;
}

void BuildCharacterPanelTree(CharacterPanelState & state,
                             Uint16 panelWidth,
                             World & world,
                             Resources & resources,
                             silencer::ui::UiInteractionRegistry& interactions) {
	// Refresh display strings each frame. Clay rebuilds this compact panel
	// from scratch, so the buffers only need to remain stable through the
	// current layout pass.
	const Uint8 a = state.selectedAgency;
	const Lobby::Character * ch = world.lobby.GetSelectedCharacter();
	const int headerWidth = std::max(0,
		static_cast<int>(panelWidth) -
		2 * static_cast<int>(character_panel_detail::kPanelPad));
	character_panel_detail::g_stats.name =
		character_panel_detail::FitMiddleEllipsis(
			ch ? std::string(ch->name) : std::string("No Agent"),
			TextSize::Heading,
			headerWidth);

	User * user = world.lobby.GetUserInfo(world.lobby.accountid);
	if(user && !user->retrieving){
		character_panel_detail::g_stats.level = std::to_string(user->agency[a].level);
		character_panel_detail::g_stats.wins = std::to_string(user->agency[a].wins);
		character_panel_detail::g_stats.losses = std::to_string(user->agency[a].losses);
	}else{
		character_panel_detail::g_stats.level = "0";
		character_panel_detail::g_stats.wins = "0";
		character_panel_detail::g_stats.losses = "0";
	}

	const int innerWidth = std::max(1,
		static_cast<int>(panelWidth) -
		2 * static_cast<int>(character_panel_detail::kPanelPad));
	const int emblemBoxW = character_panel_detail::ClampInt(
		innerWidth * character_panel_detail::kEmblemWidthPct / 100,
		character_panel_detail::kEmblemMinWidth,
		character_panel_detail::kEmblemMaxWidth);
	// Fixed label column = widest label, so the three values align in a clean
	// column without stretching label and value apart.
	const int labelColumnWidth = static_cast<int>(
		MeasureText(CLAY_STRING("LOSSES"), TextSize::Body).width) + 4;

	// Two content bands: name plus a body row. The right column owns the
	// record stats and its action button so the button reads as part of the
	// details area instead of a detached footer.
	// growing body centers its content vertically, so any extra panel height
	// becomes balanced breathing room.
	// The parent LobbyCharacterBox is supplied by the lobby shell; this
	// function emits only content.
	CLAY({ .id = CLAY_ID("CharacterPanelContent"),
	       .layout = {
	           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
	           .padding = { character_panel_detail::kPanelPad, character_panel_detail::kPanelPad, character_panel_detail::kPanelPad, character_panel_detail::kPanelPad },
	           .childGap = character_panel_detail::kBandGap,
	           .layoutDirection = CLAY_TOP_TO_BOTTOM,
	       },
	       .clip = { .horizontal = true, .vertical = true } }) {
		CLAY({ .id = CLAY_ID("CharacterPanelNameHeader"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0),
		                       CLAY_SIZING_FIXED(static_cast<float>(
		                           TextLineHeight(TextSize::Heading))) },
		           .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
		                               .y = CLAY_ALIGN_Y_TOP },
		       },
		       .clip = { .horizontal = true } }) {
			Text(character_panel_detail::FromStd(character_panel_detail::g_stats.name),
			     TextOpts{ .size = TextSize::Heading,
			               .wrap = TextWrap::None,
			               .effect = TextEffect::LegacyPalette(200) });
		}

		CLAY({ .id = CLAY_ID("CharacterPanelBody"),
		       .layout = {
		           .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
		           .childGap = character_panel_detail::kEmblemGap,
		           .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
		                               .y = CLAY_ALIGN_Y_CENTER },
		           .layoutDirection = CLAY_LEFT_TO_RIGHT,
		       },
		       .clip = { .horizontal = true, .vertical = true } }) {
			character_panel_detail::AgencyEmblem(a, emblemBoxW);

			CLAY({ .id = CLAY_ID("CharacterPanelDetailsColumn"),
			       .layout = {
			           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
			           .childGap = character_panel_detail::kDetailsGap,
			           .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
			                               .y = CLAY_ALIGN_Y_CENTER },
			           .layoutDirection = CLAY_TOP_TO_BOTTOM,
			       },
			       .clip = { .horizontal = true } }) {
				CLAY({ .id = CLAY_ID("CharacterPanelStatTable"),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
				           .childGap = character_panel_detail::kStatRowGap,
				           .layoutDirection = CLAY_TOP_TO_BOTTOM,
				       },
				       .clip = { .horizontal = true } }) {
					character_panel_detail::StatRow(0, "LEVEL", character_panel_detail::g_stats.level, labelColumnWidth);
					character_panel_detail::StatRow(1, "WINS", character_panel_detail::g_stats.wins, labelColumnWidth);
					character_panel_detail::StatRow(2, "LOSSES", character_panel_detail::g_stats.losses, labelColumnWidth);
				}

				CLAY({ .id = CLAY_ID("CharacterPanelActionsRow"),
				       .layout = {
				           .sizing = { CLAY_SIZING_FIT(0),
				                       CLAY_SIZING_FIXED(character_panel_detail::kButtonHeight) },
				           .childAlignment = { .x = CLAY_ALIGN_X_LEFT,
				                               .y = CLAY_ALIGN_Y_CENTER },
				       },
				       .clip = { .horizontal = true } }) {
					Button(CLAY_STRING("CharacterPanelAgentsButton"),
					       CLAY_STRING("Agents"),
					       ButtonOpts{ .variant = ButtonVariant::Chrome,
					                   .size = ButtonSize::Auto,
					                   .minWidth = character_panel_detail::kActionButtonMinWidth,
					                   .paddingX = character_panel_detail::kActionButtonPaddingX },
					       ButtonHandle{ nullptr, character_panel_detail::kActionAgents, &interactions });
				}
			}
		}
	}
}

}  // namespace silencer::client_ui::lobby
