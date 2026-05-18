#ifndef SABOTAGE_MODE_H
#define SABOTAGE_MODE_H

#include "gamemode.h"

class SabotageMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_SABOTAGE; }
	const char* Name() const override { return "Sabotage"; }
	bool IsMatchOver(const World&) const override { return false; }
};

#endif
