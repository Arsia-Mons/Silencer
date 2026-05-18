#ifndef DEATHMATCH_MODE_H
#define DEATHMATCH_MODE_H

#include "gamemode.h"

class DeathmatchMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_DEATHMATCH; }
	const char* Name() const override { return "Deathmatch"; }
	bool IsMatchOver(const World&) const override { return false; }
};

#endif
