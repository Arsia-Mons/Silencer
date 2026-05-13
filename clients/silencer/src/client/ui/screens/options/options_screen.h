#ifndef OPTIONS_SCREEN_H
#define OPTIONS_SCREEN_H

#include "screen.h"

class OptionsScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Draw(ScreenContext & ctx, Surface & dst, float frametime) override;
	void Destroy(ScreenContext & ctx) override;

	void NotifyGoBackClicked() { goBackClicked = true; }
	void NotifyControlsClicked() { controlsClicked = true; }
	void NotifyDisplayClicked() { displayClicked = true; }
	void NotifyAudioClicked() { audioClicked = true; }

private:
	bool goBackClicked = false;
	bool controlsClicked = false;
	bool displayClicked = false;
	bool audioClicked = false;
};

#endif
