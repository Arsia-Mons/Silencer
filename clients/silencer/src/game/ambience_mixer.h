#ifndef AMBIENCE_MIXER_H
#define AMBIENCE_MIXER_H

#include "shared.h"

class World;
class Renderer;
class LobbyGame;
class MapDownloader;

// Three looped background audio channels (base / ambient / outside) and the
// in-game music selection. Reads Game's `fade_i` by reference so FadedIn()
// gates music plays during the fade-in transition. Borrows MapDownloader for
// directory enumeration when picking a random music file.
class AmbienceMixer
{
public:
	AmbienceMixer(World & world, Renderer & renderer, MapDownloader & mapDownloader, const Uint8 & fadeI);
	~AmbienceMixer() = default;

	AmbienceMixer(const AmbienceMixer &) = delete;
	AmbienceMixer & operator=(const AmbienceMixer &) = delete;

	void GetGameChannelName(LobbyGame & lobbygame, char * name);
	void CreateAmbienceChannels(void);
	void UpdateAmbienceChannels(void);
	bool FadedIn(void);
	void LoadRandomGameMusic(void);
	void PlayMusic(Mix_Music * music);

	enum {BG_AMBIENT = 0, BG_BASE, BG_OUTSIDE};

	int    bgchannel[3];
	Uint8  oldambiencelevel  = 0;
	Uint32 lastmusicplaytime = 0;
	char   currentmusictrack[256];

private:
	World &         world;
	Renderer &      renderer;
	MapDownloader & mapDownloader;
	const Uint8 &   fade_i;
};

#endif
