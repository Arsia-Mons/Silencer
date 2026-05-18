#ifndef ASSASSINATION_MODE_H
#define ASSASSINATION_MODE_H

#include "gamemode.h"

class AssassinationMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_ASSASSINATION; }
	const char* Name() const override { return "Assassination"; }
	bool IsMatchOver(const World&) const override { return false; }
};

#endif
