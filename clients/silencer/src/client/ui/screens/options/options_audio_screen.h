#ifndef OPTIONS_AUDIO_SCREEN_H
#define OPTIONS_AUDIO_SCREEN_H

#include "screen.h"

class OptionsAudioScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Draw(ScreenContext & ctx, Surface & dst, float frametime) override;
	void Destroy(ScreenContext & ctx) override;

	void NotifyMusicClicked() { musicClicked = true; }
	void NotifySaveClicked() { saveClicked = true; }
	void NotifyCancelClicked() { cancelClicked = true; }

private:
	bool musicClicked = false;
	bool saveClicked = false;
	bool cancelClicked = false;
};

#endif
