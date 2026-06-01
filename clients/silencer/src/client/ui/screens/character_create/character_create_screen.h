#ifndef SILENCER_CLIENT_UI_CHARACTER_CREATE_SCREEN_H
#define SILENCER_CLIENT_UI_CHARACTER_CREATE_SCREEN_H

#include "client/ui/hooks/use_lobby.h"
#include "client/ui/retained/RetainedFrame.h"
#include "screen.h"
#include "shared.h"

#include <string>
#include <vector>

class Surface;

namespace silencer {
namespace client_ui {
struct Navigation;
}
}

class CharacterCreateScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx,
	             Surface & dst,
	             float frametime,
	             const silencer::ui::UiInputState& input,
	             Uint8 hudPhase,
	             silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleBack(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;
	const ::ui::DrawCommandList * RetainedDrawCommands() const override;

private:
	enum class Step : Uint8 {
		SelectAgent,
		EnterAlias,
		SelectAgency,
	};

	void SelectCurrentAgent(const silencer::client_ui::LobbyModel & lobby,
	                        const silencer::client_ui::Navigation & navigation);
	void CreateCurrentAgent(const silencer::client_ui::LobbyModel & lobby,
	                        const silencer::client_ui::Navigation & navigation);
	void StartRenameAgent(const silencer::client_ui::LobbyModel & lobby,
	                      int agentIndex);
	void RenameCurrentAgent(const silencer::client_ui::LobbyModel & lobby,
	                        const silencer::client_ui::Navigation & navigation);
	void RebuildAgentRows(const silencer::client_ui::LobbyModel & lobby);
	void CopyAlias(const char * value);
	void AdvanceAliasStep();
	bool IsRenaming() const { return renameCharacterId != 0; }

	Step step = Step::SelectAgent;
	int selectedAgentIndex = 0;
	int previewAgentIndex = -1;
	Uint16 agentScroll = 0;
	int agentScrollDelta = 0;
	Uint8 selectedAgency = 0;
	int previewAgencyIndex = -1;
	size_t characterCountOnEntry = 0;
	bool waitingForCreate = false;
	bool waitingForRename = false;
	Uint32 renameCharacterId = 0;
	bool focusAliasRequested = false;
	char alias[17] = {};
	std::vector<std::string> agentRows;
	std::vector<silencer::client_ui::LobbyAgentSummary> agents;
	silencer::client_ui::RetainedFrame retainedFrame_;
};

#endif
