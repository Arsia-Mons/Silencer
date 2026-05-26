#ifndef OPTIONS_AUDIO_SCREEN_H
#define OPTIONS_AUDIO_SCREEN_H

#include "screen.h"

#include <functional>

class OptionsAudioScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void BuildUi(ScreenContext & ctx, Surface & dst, float frametime, silencer::ui::UiInteractionRegistry& interactions) override;
	void Destroy(ScreenContext & ctx) override;
	bool HandleUiIntent(ScreenContext & ctx, const silencer::ui::UiAction & action) override;

private:
	std::function<void()> toggleMusic;
	std::function<void()> save;
	std::function<void()> cancel;
};

#endif
