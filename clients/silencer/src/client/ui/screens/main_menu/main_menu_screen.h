#ifndef MAIN_MENU_SCREEN_H
#define MAIN_MENU_SCREEN_H

#include "screen.h"

#include <string>

class MainMenuScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	bool BuildElement(ScreenContext & ctx, ::ui::UiElement * out) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleBack(ScreenContext & ctx) override;

private:
	std::string versionText_;
};

#endif
