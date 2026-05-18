#ifndef TEAM_DEATHMATCH_MODE_H
#define TEAM_DEATHMATCH_MODE_H

#include "gamemode.h"

class TeamDeathmatchMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_TEAM_DEATHMATCH; }
	const char* Name() const override { return "Team Deathmatch"; }
	bool IsMatchOver(const World&) const override { return false; }
};

#endif
