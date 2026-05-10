#include "audio.h"
#include "world.h"
#include "player.h"
#include "config.h"
#include "game.h"
#include "gasloader.h"
#include "map.h"
#include "platform.h"
#include <math.h>

Audio::Audio(){
	enabled = false;
	effectvolume = 1;
	musicvolume = MIX_MAX_VOLUME;
	mixer = nullptr;
	musictrack = nullptr;
	SDL_zero(mixerspec);
	lastx = 0;
	lasty = 0;
	for(int i = 0; i < maxchannels; i++){
		tracks[i] = nullptr;
		channelobject[i] = 0;
		channelvolume[i] = 128;
		occlusionCache[i] = 1.0f;
		filterAlpha[i] = 1.0f;
		filterState[i][0] = 0.0f;
		filterState[i][1] = 0.0f;
		filterState[i][2] = 0.0f;
		filterState[i][3] = 0.0f;
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
			MIX_SetTrackCookedCallback(tracks[i], FilterCallback, (void*)(intptr_t)i);
		}
	}
	musictrack = MIX_CreateTrack(mixer);
	MIX_GetMixerFormat(mixer, &mixerspec);
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
				// Reset per-channel spatial state before reuse so stale occlusion
				// from a previous sound never bleeds into the new one.
				occlusionCache[i] = 1.0f;
				filterAlpha[i]    = 1.0f;
				filterState[i][0] = filterState[i][1] = filterState[i][2] = filterState[i][3] = 0.0f;
				MIX_SetTrackStereo(tracks[i], nullptr);

				MIX_SetTrackAudio(tracks[i], chunk);
				MIX_SetTrackGain(tracks[i], (volume / 128.0f) * effectvolume);
				SDL_PropertiesID options = 0;
				if(loop){
					options = SDL_CreateProperties();
					SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
				}
				MIX_PlayTrack(tracks[i], options);
				if(options) SDL_DestroyProperties(options);
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
	Sint64 fade_frames = 0;
	if(fadeoutms > 0){
		fade_frames = MIX_TrackMSToFrames(tracks[channel], fadeoutms);
		if(fade_frames < 0) fade_frames = 0;
	}
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
		UpdateVolume(world, channel, lastx, lasty, GASLoader::Get().world.audioRange);
	}
	return channel;
}

void Audio::UpdateVolume(World & world, int channel, Sint16 x, Sint16 y, int radius){
	Uint16 objectid = channelobject[channel];
	if(!objectid) return;
	Object * object = world.GetObjectFromId(objectid);
	if(!object) return;

	// Local player's own sounds are never attenuated, filtered, or panned.
	Player * localplayer = world.GetPeerPlayer(world.localpeerid);
	if(localplayer && objectid == localplayer->id){
		MIX_SetTrackGain(tracks[channel], (channelvolume[channel] / 128.0f) * effectvolume);
		filterAlpha[channel] = 1.0f;
		occlusionCache[channel] = 1.0f;
		lastx = x; lasty = y;
		return;
	}

	float dx = float(signed(x)) - float(object->x);
	float dy = float(signed(y)) - float(object->y);
	float distance = sqrtf(dx*dx + dy*dy);
	float distVolume = 1.0f - (distance / float(radius));
	if(distVolume < 0.0f) distVolume = 0.0f;

	const WorldDef & cfg = GASLoader::Get().world;

	// Phase 1: ray-based occlusion
	// Offset ray endpoints to the top of each character's hurtbox so the ray
	// doesn't clip through the floor platform they're both standing on.
	int rayYOffset = GASLoader::Get().world.occlusionRayYOffset;
	float targetOcclusion = 1.0f;
	float targetFilterOcclusion = 1.0f; // rect-only factor used for filter threshold
	if(cfg.soundOcclusionEnabled && distVolume > 0.0f && distance > 32.0f){
		targetOcclusion = ComputeOcclusion(world.map,
			int(x),          int(y)          - rayYOffset,
			int(object->x),  int(object->y)  - rayYOffset,
			cfg.occlusionDampenRect, cfg.occlusionDampenStairs,
			&targetFilterOcclusion);
	}
	occlusionCache[channel] += (targetOcclusion - occlusionCache[channel]) * cfg.occlusionLerpSpeed;
	float occlusion = occlusionCache[channel];

	// Phase 2: per-track IIR low-pass coefficient (applied in FilterCallback).
	// Only solid RECTANGLE walls trigger the muffle filter — staircase crossings
	// reduce volume but don't muffle frequency (fixed staircase false-positive).
	if(cfg.soundFilterEnabled && targetFilterOcclusion < cfg.occlusionMuffleThreshold){
		float t = targetFilterOcclusion / cfg.occlusionMuffleThreshold;
		float cutoffHz = cfg.occlusionMuffleMinHz + t * (cfg.occlusionMuffleMaxHz - cfg.occlusionMuffleMinHz);
		float sampleRate = float(mixerspec.freq > 0 ? mixerspec.freq : 44100);
		filterAlpha[channel] = 1.0f - expf(-2.0f * 3.14159265f * cutoffHz / sampleRate);
	} else {
		filterAlpha[channel] = 1.0f; // passthrough — FilterCallback skips processing
	}

	int oldvolume = channelvolume[channel];
	MIX_SetTrackGain(tracks[channel], ((oldvolume * distVolume * occlusion) / 128.0f) * effectvolume);

	// Phase 3: stereo pan from horizontal offset
	if(cfg.soundPanningEnabled && radius > 0){
		float pan = -(dx / float(radius)) * 0.8f; // -1=left, +1=right
		if(pan < -1.0f) pan = -1.0f;
		if(pan >  1.0f) pan =  1.0f;
		MIX_StereoGains gains;
		gains.left  = 1.0f - (pan > 0.0f ? pan : 0.0f);
		gains.right = 1.0f - (pan < 0.0f ? -pan : 0.0f);
		MIX_SetTrackStereo(tracks[channel], &gains);
	}

	lastx = x;
	lasty = y;
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
	if(!MIX_TrackPlaying(musictrack) && !MIX_TrackPaused(musictrack)){
		SDL_PropertiesID options = SDL_CreateProperties();
		SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
		MIX_SetTrackAudio(musictrack, music);
		MIX_SetTrackGain(musictrack, musicvolume / 128.0f);
		MIX_PlayTrack(musictrack, options);
		SDL_DestroyProperties(options);
		return true;
	}
	return false;
}

void Audio::StopMusic(void){
	if(!enabled) return;
	MIX_StopTrack(musictrack, MIX_TrackMSToFrames(musictrack, 700));
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

float Audio::ComputeOcclusion(Map & map, int lx, int ly, int ex, int ey, float dampenRect, float dampenStairs, float * outFilterFactor){
	float factor = 1.0f;
	float filterFactor = 1.0f; // rect-only: stairs reduce volume but don't muffle frequency
	int cx = lx, cy = ly;
	int origdx = ex - lx, origdy = ey - ly;

	for(int pass = 0; pass < 16; pass++){
		int xe = ex, ye = ey;
		Platform * hit = map.TestLine(cx, cy, ex, ey, &xe, &ye,
			Platform::RECTANGLE | Platform::STAIRSUP | Platform::STAIRSDOWN);
		if(!hit) break;

		bool isRect = (hit->type & Platform::RECTANGLE) != 0;
		factor *= isRect ? dampenRect : dampenStairs;
		if(isRect) filterFactor *= dampenRect;
		if(factor < 0.001f){
			if(outFilterFactor) *outFilterFactor = filterFactor;
			return 0.0f;
		}

		// Advance 2px past the hit point along the ray direction.
		int dx = ex - cx, dy = ey - cy;
		float len = sqrtf(float(dx*dx + dy*dy));
		if(len < 2.0f) break;
		cx = xe + int(float(dx) / len * 2.0f + 0.5f);
		cy = ye + int(float(dy) / len * 2.0f + 0.5f);

		// Stop if the new start has passed or reached the emitter.
		int rdx = ex - cx, rdy = ey - cy;
		if(rdx * origdx + rdy * origdy <= 0) break;
	}
	if(outFilterFactor) *outFilterFactor = filterFactor;
	return factor;
}

void Audio::FilterCallback(void * userdata, MIX_Track * /*track*/, const SDL_AudioSpec * spec, float * pcm, int samples){
	int channel = (int)(intptr_t)userdata;
	Audio & audio = Audio::GetInstance();
	float alpha = audio.filterAlpha[channel];
	if(alpha >= 1.0f) return; // no occlusion — passthrough

	int numCh = spec->channels < 2 ? 1 : 2;
	for(int s = 0; s < samples; s++){
		for(int c = 0; c < numCh; c++){
			float x = pcm[s * spec->channels + c];
			// Pass 1
			float & y1 = audio.filterState[channel][c];
			y1 = alpha * x + (1.0f - alpha) * y1;
			// Pass 2 (cascaded — 12dB/oct rolloff)
			float & y2 = audio.filterState[channel][2 + c];
			y2 = alpha * y1 + (1.0f - alpha) * y2;
			pcm[s * spec->channels + c] = y2;
		}
	}
}

void Audio::TrackStoppedCallback(void *userdata, MIX_Track *track){
	int channel = (int)(intptr_t)userdata;
	Audio & audio = Audio::GetInstance();
	audio.channelobject[channel] = 0;
	audio.occlusionCache[channel] = 1.0f;
	audio.filterAlpha[channel] = 1.0f;
	audio.filterState[channel][0] = 0.0f;
	audio.filterState[channel][1] = 0.0f;
	audio.filterState[channel][2] = 0.0f;
	audio.filterState[channel][3] = 0.0f;
	MIX_SetTrackStereo(track, nullptr); // clear forced-stereo pan
}

void Audio::MixingFunction(void * udata, Uint8 * stream, int len){
	// TODO: Post-mix ffmpeg callback not available in SDL3_mixer 3.x; replay audio export disabled
}