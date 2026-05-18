#ifndef MANHUNT_MODE_H
#define MANHUNT_MODE_H

#include "gamemode.h"

class ManhuntMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_MANHUNT; }
	const char* Name() const override { return "Manhunt"; }
	bool IsMatchOver(const World&) const override { return false; }
};

#endif
