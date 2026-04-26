#include "audio.h"
#include "world.h"
#include "config.h"
#include "game.h"
#include <math.h>

Audio::Audio(){
	enabled = false;
	effectvolume = 1;
	musicvolume = MIX_MAX_VOLUME;
	mixer = nullptr;
	musictrack = nullptr;
	SDL_zero(mixerspec);
	for(int i = 0; i < maxchannels; i++){
		tracks[i] = nullptr;
		channelobject[i] = 0;
		channelvolume[i] = 128;
	}
}

Audio::~Audio(){
	Close();
}

Audio & Audio::GetInstance(void){
	static Audio instance;
	return instance;
}

bool Audio::Init(Game * game){
	mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
	if(!mixer) return false;

	for(int i = 0; i < maxchannels; i++){
		tracks[i] = MIX_CreateTrack(mixer);
		if(tracks[i]){
			MIX_SetTrackStoppedCallback(tracks[i], TrackStoppedCallback, (void*)(intptr_t)i);
		}
	}
	musictrack = MIX_CreateTrack(mixer);
	MIX_GetMixerFormat(mixer, &mixerspec);
	// TODO: Post-mix callback for ffmpeg replay audio export was removed in SDL3_mixer 3.x
	enabled = true;
	return true;
}

void Audio::Close(void){
	enabled = false;
	if(mixer){
		for(int i = 0; i < maxchannels; i++){
			if(tracks[i]){ MIX_DestroyTrack(tracks[i]); tracks[i] = nullptr; }
		}
		if(musictrack){ MIX_DestroyTrack(musictrack); musictrack = nullptr; }
		MIX_DestroyMixer(mixer);
		mixer = nullptr;
	}
}

int Audio::Play(MIX_Audio * chunk, int volume, bool loop){
	if(enabled && chunk){
		for(int i = 0; i < maxchannels; i++){
			if(!MIX_TrackPlaying(tracks[i]) && !MIX_TrackPaused(tracks[i])){
				MIX_SetTrackAudio(tracks[i], chunk);
				MIX_SetTrackLoops(tracks[i], loop ? -1 : 0);
				MIX_SetTrackGain(tracks[i], (volume / 128.0f) * effectvolume);
				MIX_PlayTrack(tracks[i], 0);
				channelobject[i] = 0;
				channelvolume[i] = volume;
				return i;
			}
		}
	}
	return -1;
}

void Audio::Stop(int channel, int fadeoutms){
	if(!enabled || channel < 0 || channel >= maxchannels) return;
	Sint64 fade_frames = fadeoutms ? MIX_MSToFrames(mixerspec.freq, fadeoutms) : 0;
	MIX_StopTrack(tracks[channel], fade_frames);
}

void Audio::StopAll(int fadeoutms){
	if(!enabled) return;
	MIX_StopAllTracks(mixer, fadeoutms);
}

void Audio::Pause(int channel){
	if(!enabled || channel < 0 || channel >= maxchannels) return;
	MIX_PauseTrack(tracks[channel]);
}

void Audio::Resume(int channel){
	if(!enabled || channel < 0 || channel >= maxchannels) return;
	MIX_ResumeTrack(tracks[channel]);
}

bool Audio::Paused(int channel){
	if(!enabled || channel < 0 || channel >= maxchannels) return false;
	return MIX_TrackPaused(tracks[channel]);
}

int Audio::EmitSound(World & world, Uint16 objectid, MIX_Audio * chunk, int volume, bool loop){
	int channel = Play(chunk, volume * effectvolume, loop);
	if(channel != -1){
		channelobject[channel] = objectid;
		UpdateVolume(world, channel, lastx, lasty, 500);
	}
	return channel;
}

void Audio::UpdateVolume(World & world, int channel, Sint16 x, Sint16 y, int radius){
	Uint16 objectid = channelobject[channel];
	if(objectid){
		Object * object = world.GetObjectFromId(objectid);
		if(object){
			int diffx = abs(signed(x) - object->x);
			int diffy = abs(signed(y) - object->y);
			float distance = abs(sqrt(float((diffx * diffx) + (diffy * diffy))));
			float volume = 1 - (distance / radius);
			if(volume < 0) volume = 0;
			if(volume > 1) volume = 1;
			int oldvolume = channelvolume[channel];
			MIX_SetTrackGain(tracks[channel], ((oldvolume * volume) / 128.0f) * effectvolume);
			lastx = x;
			lasty = y;
		}
	}
}

void Audio::UpdateAllVolumes(World & world, Sint16 x, Sint16 y, int radius){
	for(int i = 0; i < maxchannels; i++){
		UpdateVolume(world, i, x, y, radius);
	}
}

void Audio::SetVolume(int channel, int volume){
	if(!enabled || channel < 0 || channel >= maxchannels) return;
	MIX_SetTrackGain(tracks[channel], (volume / 128.0f) * effectvolume);
}

void Audio::Mute(int volume){
	if(!enabled) return;
	float percent = volume / 128.0f;
	effectvolume = percent;
	for(int i = 0; i < maxchannels; i++){
		int oldvolume = channelvolume[i];
		MIX_SetTrackGain(tracks[i], (oldvolume / 128.0f) * percent);
	}
	MIX_SetTrackGain(musictrack, (musicvolume / 128.0f) * percent);
}

void Audio::Unmute(void){
	if(!enabled) return;
	effectvolume = 1;
	for(int i = 0; i < maxchannels; i++){
		int oldvolume = channelvolume[i];
		MIX_SetTrackGain(tracks[i], oldvolume / 128.0f);
	}
	MIX_SetTrackGain(musictrack, musicvolume / 128.0f);
}

bool Audio::PlayMusic(MIX_Audio * music){
	if(!enabled) return false;
	if(!Config::GetInstance().music) return false;
	if(!MIX_TrackPlaying(musictrack) || !MIX_TrackPaused(musictrack)){
		if(!MusicPaused()){
			MIX_SetTrackAudio(musictrack, music);
			MIX_SetTrackLoops(musictrack, -1);
			MIX_SetTrackGain(musictrack, musicvolume / 128.0f);
			MIX_PlayTrack(musictrack, 0);
			return true;
		}
	}
	return false;
}

void Audio::StopMusic(void){
	if(!enabled) return;
	MIX_StopTrack(musictrack, MIX_MSToFrames(mixerspec.freq, 700));
}

void Audio::PauseMusic(void){
	if(!enabled) return;
	MIX_PauseTrack(musictrack);
}

void Audio::ResumeMusic(void){
	if(!enabled) return;
	MIX_ResumeTrack(musictrack);
}

bool Audio::MusicPaused(void){
	if(!enabled) return false;
	return MIX_TrackPaused(musictrack);
}

void Audio::SetMusicVolume(int volume){
	if(!enabled){
		musicvolume = volume;
		return;
	}
	MIX_SetTrackGain(musictrack, volume / 128.0f);
	musicvolume = volume;
}

void Audio::TrackStoppedCallback(void *userdata, MIX_Track *track){
	int channel = (int)(intptr_t)userdata;
	Audio::GetInstance().channelobject[channel] = 0;
}

void Audio::MixingFunction(void * udata, Uint8 * stream, int len){
	// TODO: Post-mix ffmpeg callback not available in SDL3_mixer 3.x; replay audio export disabled
}