#ifndef EXTRACTION_MODE_H
#define EXTRACTION_MODE_H

#include "gamemode.h"

class ExtractionMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_EXTRACTION; }
	const char* Name() const override { return "Extraction"; }
	bool IsMatchOver(const World&) const override { return false; }
};

#endif
