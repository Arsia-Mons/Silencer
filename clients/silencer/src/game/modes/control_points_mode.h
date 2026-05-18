#ifndef CONTROL_POINTS_MODE_H
#define CONTROL_POINTS_MODE_H

#include "gamemode.h"

class ControlPointsMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_CONTROL_POINTS; }
	const char* Name() const override { return "Control Points"; }
	bool IsMatchOver(const World&) const override { return false; }
};

#endif
