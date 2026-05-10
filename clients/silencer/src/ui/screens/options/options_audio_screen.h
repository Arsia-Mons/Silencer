#ifndef OPTIONS_AUDIO_SCREEN_H
#define OPTIONS_AUDIO_SCREEN_H

#include "screen.h"

class OptionsAudioScreen : public Screen
{
public:
	void Build(ScreenContext & ctx) override;
	void Tick(ScreenContext & ctx) override;
	void Destroy(ScreenContext & ctx) override;
};

#endif
