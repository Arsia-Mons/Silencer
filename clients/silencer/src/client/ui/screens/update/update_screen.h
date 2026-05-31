#ifndef UPDATE_SCREEN_H
#define UPDATE_SCREEN_H

#include "screen.h"

#include <string>

class UpdateScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	bool BuildElement(ScreenContext & ctx, ::ui::UiElement * out) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;

private:
	std::string statusText_;
	std::string progressText_;
};

#endif
