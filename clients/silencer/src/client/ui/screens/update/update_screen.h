#ifndef UPDATE_SCREEN_H
#define UPDATE_SCREEN_H

#include "screen.h"

class UpdateScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;

private:
	bool updateClicked = false;
	bool cancelClicked = false;
	bool retryClicked = false;
	bool downloadClicked = false;
};

#endif
