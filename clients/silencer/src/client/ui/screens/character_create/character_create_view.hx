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
	std::function<void(int index)> on_agent_focus = {};
	std::function<void(int index)> on_agent_activate = {};
	std::function<void(int index)> on_rename = {};
	std::function<void(const std::string&)> on_alias_change = {};
	std::function<void(const ::ui::ActivationEvent&)> on_alias_submit = {};
	std::function<void(int index)> on_agency_focus = {};
	std::function<void(int index)> on_agency_activate = {};
};

extern ::ReactContext CharacterCreateContext;

const CharacterCreateContextValue& UseCharacterCreate();

struct CharacterCreateScreenViewProps {
	const char * key = nullptr;
	const CharacterCreateContextValue * value = nullptr;
};

::ui::UiElement CharacterCreateScreenView(const CharacterCreateScreenViewProps& props);

}  // namespace silencer::client_ui
