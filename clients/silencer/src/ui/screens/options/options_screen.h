#ifndef OPTIONS_SCREEN_H
#define OPTIONS_SCREEN_H

#include "screen.h"

class OptionsScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;
};

#endif
