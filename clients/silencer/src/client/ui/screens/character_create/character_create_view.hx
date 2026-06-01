#pragma once

#include "ui/components/common.h"
#include "ui/runtime/react.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace silencer::client_ui {

constexpr int kCharacterCreateVisibleAgentRows = 8;
constexpr int kCharacterCreateAgencyCount = 5;
constexpr int kCharacterCreateStatLineCount = 4;

enum class CharacterCreateViewStep {
	SelectAgent,
	EnterAlias,
	SelectAgency,
};

struct CharacterCreateContextValue {
	struct State {
		CharacterCreateViewStep step = CharacterCreateViewStep::SelectAgent;
		const std::vector<std::string> * agent_rows = nullptr;
		int agent_scroll = 0;
		int selected_agent_index = 0;
		int preview_agent_index = -1;
		int detail_agent_index = -1;
		int detail_agency_index = 0;
		bool detail_rename_available = false;
		std::array<const char *, kCharacterCreateStatLineCount> detail_stats = {};
		const char * alias = "";
		bool alias_renaming = false;
		bool waiting = false;
		int selected_agency_index = 0;
		int preview_agency_index = -1;
	};

	struct Actions {
		std::function<void(int index)> preview_agent = {};
		std::function<void(int index)> activate_agent = {};
		std::function<void(int index)> rename_agent = {};
		std::function<void(const std::string&)> set_alias = {};
		std::function<void()> submit_alias = {};
		std::function<void(int index)> preview_agency = {};
		std::function<void(int index)> activate_agency = {};
	};

	State state = {};
	Actions actions = {};
};

const CharacterCreateContextValue& UseCharacterCreate();

struct CharacterCreateFrameProps {
	const char * key = nullptr;
};

::ui::UiElement CharacterCreateFrame(const CharacterCreateFrameProps& props);

struct CharacterCreateScreenViewProps {
	const char * key = nullptr;
	const CharacterCreateContextValue * value = nullptr;
};

::ui::UiElement CharacterCreateScreenView(const CharacterCreateScreenViewProps& props);

}  // namespace silencer::client_ui
