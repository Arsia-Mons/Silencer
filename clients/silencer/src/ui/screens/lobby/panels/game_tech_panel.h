#ifndef GAME_TECH_PANEL_H
#define GAME_TECH_PANEL_H

#include "panel.h"

class LobbyScreen;

// Right-side tech-choice panel on the LobbyScreen: per-team-peer tech
// checkboxes (4 columns x N tech-slot items), a tech-name overlay + 8
// description lines, the "Tech slots left:" overlay (uid 70), and the
// "Back To Teams" button (uid 28). Built by LobbyScreen::ShowGameTech and
// torn down on Back To Teams (which calls LobbyScreen::ShowGameJoin) or
// when GoBack tears down the joined-game lobby surface.
//
// Per-frame Tick replicates the legacy Game::UpdateTechInterface: refreshes
// the slots-left text, syncs each team peer's checkboxes (active row vs
// peer-display row), wires the description-overlay click handler, and the
// "tech ON for self" checkbox click that calls World::SetTech.
class GameTechPanel : public Panel
{
public:
	explicit GameTechPanel(LobbyScreen & owner);
	void Build(ScreenContext & ctx, Interface * parent) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;

private:
	LobbyScreen & owner;
};

#endif
