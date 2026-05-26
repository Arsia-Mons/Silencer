#ifndef OPTIONS_SCREEN_H
#define OPTIONS_SCREEN_H

#include "screen.h"

#include <functional>

class OptionsScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;

private:
	std::function<void()> goBack;
	std::function<void()> openControls;
	std::function<void()> openDisplay;
	std::function<void()> openAudio;
};

#endif
