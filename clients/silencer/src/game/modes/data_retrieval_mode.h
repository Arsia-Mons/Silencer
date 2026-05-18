#ifndef DATA_RETRIEVAL_MODE_H
#define DATA_RETRIEVAL_MODE_H

#include "gamemode.h"

// Deliver secretsNeededToWin secrets to base. Win detection delegates to the
// existing Team::Tick logic — zero behavior change from pre-framework code.
class DataRetrievalMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_DATA_RETRIEVAL; }
	const char* Name() const override { return "Data Retrieval"; }
	bool   IsMatchOver(const World& w) const override;
	Uint16 WinningTeamId(const World& w) const override;
};

#endif
