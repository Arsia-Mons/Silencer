#ifndef ACTORDEF_H
#define ACTORDEF_H

#include <string>
#include <vector>
#include <unordered_map>

// Axis-aligned hurtbox relative to object (x, y) origin.
// x1/y1 = top-left offset, x2/y2 = bottom-right offset.
// y is positive downward; y=0 is the character's feet.
// Mirroring: negate x1/x2 and swap them when object.mirrored is true.
struct FrameHurtbox {
	int x1, y1, x2, y2;
};

struct FrameDef {
	int bank;
	int index;
	int duration; // ticks this frame holds (must be >= 1)
	FrameHurtbox hurtbox;
	std::string sound;  // optional: sound file to play on first tick of this frame
	int soundVolume;    // 0 = use default (128)

	FrameDef() : bank(0), index(0), duration(1), hurtbox{}, soundVolume(0) {}
};

struct AnimSequence {
	std::vector<FrameDef> frames;
	bool loop;

	// Pure lookup — does NOT advance any counter.
	// Returns the FrameDef active at tick `state_i`, or nullptr when
	// state_i is past the end of a non-looping sequence.
	const FrameDef* Resolve(int state_i) const;

	// Total tick-duration of one complete cycle.
	int TotalDuration() const;

	// Returns true (and fills outFile/outVolume) if state_i is the FIRST tick
	// of a frame that has a sound attached. Call once per tick.
	bool GetFrameSound(int state_i, std::string& outFile, int& outVolume) const;

	// Returns true if the Nth sprite frame (0-based index into frames[]) has
	// a sound. Use when the state machine drives sprite index directly (e.g.
	// res_index = state_i % N) rather than by tick-accumulated durations.
	bool GetFrameSoundByIndex(int frameIdx, std::string& outFile, int& outVolume) const;
};

struct ActorDef {
	std::string id;

	// sequences keyed by player-state name (e.g. "CROUCHING").
	// Only states whose animation can be fully driven from state_i live here;
	// complex states (STANDINGSHOOT etc.) remain hardcoded for now.
	std::unordered_map<std::string, AnimSequence> sequences;

	bool HasSequence(const std::string& name) const {
		return sequences.count(name) > 0;
	}
	const AnimSequence* GetSequence(const std::string& name) const {
		auto it = sequences.find(name);
		return it != sequences.end() ? &it->second : nullptr;
	}
};

// Load all actor definitions from JSON files in `dir`.
// Returns the number successfully loaded. Logs warnings for invalid files.
int LoadActorDefs(const std::string& dir,
                  std::unordered_map<std::string, ActorDef>& out);

// Fetch all actor definitions from the admin API and merge into `out`.
// URL format: {apiBase}/api/actors  (unauthenticated public endpoint)
// Returns the number successfully fetched. Falls back gracefully on error.
int FetchActorDefs(const char* apiBase,
                   std::unordered_map<std::string, ActorDef>& out);

// Mirror a hurtbox: negate and swap x components.
inline FrameHurtbox MirrorHurtbox(const FrameHurtbox& hb) {
	return { -hb.x2, hb.y1, -hb.x1, hb.y2 };
}

#endif
