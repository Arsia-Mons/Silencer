#ifndef SILENCER_CLIENT_UI_CHARACTER_CREATE_SCREEN_H
#define SILENCER_CLIENT_UI_CHARACTER_CREATE_SCREEN_H

#include "screen.h"
#include "shared.h"
#include "clay/clay.h"

#include <array>
#include <string>
#include <vector>

class Surface;

class CharacterCreateScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx,
	             Surface & dst,
	             float frametime,
	             silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleBack(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;

private:
	enum class Step : Uint8 {
		SelectAgent,
		EnterAlias,
		SelectAgency,
	};

	void BuildSelectAgent(ScreenContext & ctx, silencer::ui::UiInteractionRegistry& interactions);
	void BuildEnterAlias(ScreenContext & ctx, silencer::ui::UiInteractionRegistry& interactions);
	void BuildSelectAgency(ScreenContext & ctx, silencer::ui::UiInteractionRegistry& interactions);
	void SelectCurrentAgent(ScreenContext & ctx);
	void CreateCurrentAgent(ScreenContext & ctx);
	void RebuildAgentRows(ScreenContext & ctx);
	void CopyAlias(const std::string& value);
	void AdvanceAliasStep(ScreenContext & ctx);

	Step step = Step::SelectAgent;
	int selectedAgentIndex = 0;
	Uint16 agentScroll = 0;
	int agentScrollDelta = 0;
	Uint8 selectedAgency = 0;
	size_t characterCountOnEntry = 0;
	bool waitingForCreate = false;
	bool focusAliasRequested = false;
	char alias[17] = {};
	std::vector<std::string> agentRows;
	std::array<Clay_String, 32> agentItems{};
};

#endif
