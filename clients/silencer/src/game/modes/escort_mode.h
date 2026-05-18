#ifndef ESCORT_MODE_H
#define ESCORT_MODE_H

#include "gamemode.h"

class EscortMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_ESCORT; }
	const char* Name() const override { return "Escort"; }
	bool IsMatchOver(const World&) const override { return false; }
};

#endif
