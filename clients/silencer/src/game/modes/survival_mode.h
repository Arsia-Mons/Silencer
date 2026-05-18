#ifndef SURVIVAL_MODE_H
#define SURVIVAL_MODE_H

#include "gamemode.h"

class SurvivalMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_SURVIVAL; }
	const char* Name() const override { return "Survival"; }
	bool IsMatchOver(const World&) const override { return false; }
};

#endif
