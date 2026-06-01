#pragma once

#include "client/ui/hooks/use_lobby.h"
#include "ui/runtime/element.h"

#include <cstdint>
#include <string>
#include <vector>

namespace silencer {
namespace client_ui {

enum class CharacterCreateFrameStep : std::uint8_t {
	SelectAgent,
	EnterAlias,
	SelectAgency,
};

struct CharacterCreateFrameProps {
	const char * key = nullptr;
	CharacterCreateFrameStep step = CharacterCreateFrameStep::SelectAgent;
	const std::vector<std::string> * agent_rows = nullptr;
	const std::vector<LobbyAgentSummary> * agents = nullptr;
	int agent_scroll = 0;
	int selected_agent = 0;
	int preview_agent = -1;
	const char * alias = nullptr;
	bool alias_renaming = false;
	bool waiting_for_create = false;
	int selected_agency = 0;
	int preview_agency = -1;
	std::uint8_t agency_ids[5] = {};
};

::ui::UiElement CharacterCreateFrame(const CharacterCreateFrameProps& props);

}  // namespace client_ui
}  // namespace silencer
