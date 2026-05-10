#ifndef OPTIONS_DISPLAY_SCREEN_H
#define OPTIONS_DISPLAY_SCREEN_H

#include "screen.h"

class OptionsDisplayScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;
};

#endif
