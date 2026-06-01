#include "character_create_screen.h"

#include "client/ui/hooks/use_lobby.h"
#include "client/ui/screens/character_create/character_create_frame.h"
#include "clay_ui_compositor.h"
#include "game.h"
#include "renderer.h"
#include "runtime/UiInteractionRegistry.h"
#include "screen_context.h"
#include "surface.h"

#include <algorithm>

namespace character_create_screen_detail {

constexpr int kAgentRowsH = 272;
constexpr int kRowH = 27;
constexpr int kRowGap = 5;
constexpr int kAliasInputUid = 31;

}  // namespace character_create_screen_detail

void CharacterCreateScreen::BuildUi(ScreenContext & ctx,
                                    Surface & dst,
                                    float frametime,
                                    silencer::ui::UiInteractionRegistry& interactions)
{
	(void)frametime;

	const silencer::client_ui::LobbyModel lobby =
		silencer::client_ui::use_lobby(
			silencer::client_ui::MakeLobbyProvider(ctx));
	RebuildAgentRows(lobby);
	if(agentScrollDelta != 0){
		int next = static_cast<int>(agentScroll) + agentScrollDelta;
		const int visible = character_create_screen_detail::kAgentRowsH /
		                    (character_create_screen_detail::kRowH +
		                     character_create_screen_detail::kRowGap);
		const int maxScroll = std::max(
			0, static_cast<int>(agentRows.size()) - visible);
		if(next < 0) next = 0;
		if(next > maxScroll) next = maxScroll;
		agentScroll = static_cast<Uint16>(next);
		agentScrollDelta = 0;
	}

	const silencer::client_ui::LobbyCharacterModel character = lobby.character;
	silencer::client_ui::CharacterCreateFrameStep frameStep =
		silencer::client_ui::CharacterCreateFrameStep::SelectAgent;
	if(step == Step::EnterAlias){
		frameStep = silencer::client_ui::CharacterCreateFrameStep::EnterAlias;
	}else if(step == Step::SelectAgency){
		frameStep = silencer::client_ui::CharacterCreateFrameStep::SelectAgency;
	}

	silencer::client_ui::CharacterCreateFrameProps props{
		.key = "character-create",
		.step = frameStep,
		.agent_rows = &agentRows,
		.agents = &agents,
		.agent_scroll = static_cast<int>(agentScroll),
		.selected_agent = selectedAgentIndex,
		.preview_agent = previewAgentIndex,
		.alias = alias,
		.alias_renaming = IsRenaming(),
		.waiting_for_create = waitingForCreate,
		.selected_agency = selectedAgency,
		.preview_agency = previewAgencyIndex,
		.agency_ids = {
			character.agency_for_index(0),
			character.agency_for_index(1),
			character.agency_for_index(2),
			character.agency_for_index(3),
			character.agency_for_index(4),
		},
	};

	const float uiScale = silencer::clay_bridge::UiScale();
	const int virtualW = std::max(1, static_cast<int>(dst.w / uiScale));
	const int virtualH = std::max(1, static_cast<int>(dst.h / uiScale));
	retainedFrame_.Build([&]() {
		                     return silencer::client_ui::CharacterCreateFrame(props);
	                     },
	                     virtualW,
	                     virtualH,
	                     interactions);

	if(frameStep == silencer::client_ui::CharacterCreateFrameStep::EnterAlias &&
	   !interactions.IsTextInputFocused(character_create_screen_detail::kAliasInputUid)){
		interactions.FocusTextInputByUid(character_create_screen_detail::kAliasInputUid);
	}
	if(focusAliasRequested){
		focusAliasRequested = false;
	}
}

const ::ui::DrawCommandList * CharacterCreateScreen::RetainedDrawCommands() const
{
	return &retainedFrame_.Commands();
}
