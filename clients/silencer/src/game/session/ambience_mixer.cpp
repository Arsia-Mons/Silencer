#include "ambience_mixer.h"
#include "world.h"
#include "renderer.h"
#include "lobbygame.h"
#include "audio.h"
#include "config.h"
#include "gasloader.h"
#include "map_downloader.h"
#include "os.h"
#include "player.h"
#include <cstring>
#include <stdio.h>

AmbienceMixer::AmbienceMixer(World & w, Renderer & r, MapDownloader & md, const Uint8 & f)
	: world(w), renderer(r), mapDownloader(md), fade_i(f) {
	for(int i = 0; i < 3; i++) bgchannel[i] = -1;
	currentmusictrack[0] = '\0';
}

void AmbienceMixer::CreateAmbienceChannels(void){
	const WorldDef& wd = GASLoader::Get().world;
	const std::string bgchannelbanks[3] = {wd.soundAmbience1, wd.soundAmbience2, wd.soundAmbience3};
	for(int i = 0; i < sizeof(bgchannel) / sizeof(int); i++){
		if(bgchannel[i] == -1){
			bgchannel[i] = Audio::GetInstance().Play(world.resources.soundbank[bgchannelbanks[i]], 0, true);
		}
	}
}

void AmbienceMixer::UpdateAmbienceChannels(void){
	Player * localplayer = world.GetPeerPlayer(world.peers.localpeerid);
	if(localplayer){
		int columns = 5;
		int rows = 5;
		int w = 640;
		int h = 480;
		int outsideamount = 0;
		int maxamount = columns * rows;
		for(int x = 0; x < columns; x++){
			for(int y = 0; y < rows; y++){
				int x1 = (w * (x / float(columns))) - (w / 2);
				x1 += - renderer.camera.GetXOffset();
				int x2 = (w * ((x + 1) / float(columns))) - (w / 2);
				x2 += - renderer.camera.GetXOffset();
				int y1 = (h * (y / float(rows))) - (h / 2);
				y1 += - renderer.camera.GetYOffset();
				int y2 = (h * ((y + 1) / float(rows))) - (h / 2);
				y2 += - renderer.camera.GetYOffset();
				if(world.map.TestAABB(x1, y1, x2, y2, Platform::OUTSIDEROOM)){
					outsideamount++;
				}
			}
		}
		if(localplayer->InBase(world)){
			Audio::GetInstance().SetVolume(bgchannel[BG_BASE], 32);
			Audio::GetInstance().SetVolume(bgchannel[BG_AMBIENT], 0);
			Audio::GetInstance().SetVolume(bgchannel[BG_OUTSIDE], 0);
		}else{
			Audio::GetInstance().SetVolume(bgchannel[BG_BASE], 0);
			Audio::GetInstance().SetVolume(bgchannel[BG_AMBIENT], 8 * (1 - (outsideamount / float(maxamount))));
			Audio::GetInstance().SetVolume(bgchannel[BG_OUTSIDE], 8 * (outsideamount / float(maxamount)));
		}
	}
}

bool AmbienceMixer::FadedIn(void){
	return fade_i == 16;
}

void AmbienceMixer::LoadRandomGameMusic(void){
	if(!Config::GetInstance().music){
		return;
	}
	if(world.resources.gamemusic){
		Audio::GetInstance().StopMusic();
		MIX_DestroyAudio(world.resources.gamemusic);
		world.resources.gamemusic = 0;
	}
	if(!world.resources.gamemusic){
		const char * directory = "music";
		CDDataDir();
		std::vector<std::string> files = mapDownloader.ListFiles(directory);
		if(files.size() == 0){
			CDResDir();
			files = mapDownloader.ListFiles(directory);
		}
		if(files.size() > 0){
			char filename[1024];
			strcpy(filename, directory);
			strcat(filename, "/");
			const char * randomfile = files[rand() % files.size()].c_str();
			strcat(filename, randomfile);
			strncpy(currentmusictrack, randomfile, sizeof(currentmusictrack) - 1);
			world.resources.gamemusic = MIX_LoadAudio(Audio::GetInstance().GetMixer(), filename, false);
		}
	}
}

void AmbienceMixer::PlayMusic(Mix_Music * music){
	if(music && Audio::GetInstance().PlayMusic(music)){
		lastmusicplaytime = world.tickcount;
		char text[256];
		snprintf(text, sizeof(text) - 1, "Playing: %s", currentmusictrack);
		world.ShowTopMessage(text);
	}
}
