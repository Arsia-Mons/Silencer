#ifndef CONFIG_H
#define CONFIG_H

#include "shared.h"

class Config
{
public:
	Config();
	static Config & GetInstance(void);
	void Save(void);
	bool Load(void);
	void LoadDefaults(void);
	bool fullscreen;
	bool scalefilter;
	bool teamcolors;
	bool music;
	Uint8 musicvolume;
	Uint8 defaultagency;
	char lobbyhost[256];
	int lobbyport;
	char mapapiurl[512];
	char adminapiurl[512];
	char defaultgamename[64];
	Uint32 defaulttechchoices[5];
	char active_keybind_profile[64];

private:
	bool CompareString(const char * str1, const char * str2);
	void WriteString(SDL_IOStream * file, const char * variable, const char * string);
	void ReadString(const char * data, char * variable, int length);
	char * RWgets(SDL_IOStream * file, char * buffer, int count);
};

#endif
