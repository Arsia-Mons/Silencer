#ifndef UPDATE_SCREEN_H
#define UPDATE_SCREEN_H

#include "screen.h"

class UpdateScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;
};

#endif
