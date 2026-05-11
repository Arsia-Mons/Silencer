#ifndef SILENCER_UI_V2_UI_STATE_H
#define SILENCER_UI_V2_UI_STATE_H

#include "shared.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ui {
namespace v2 {

// Persistent UI state, threaded through the render + dispatch passes via
// `Context::state`. One map per state type ("anim" today; "text" / "focus"
// when the surfaces that need them land). Keys are 64-bit hashes of
// `Node::key` strings XOR'd with a per-attribute tag constant — explicit
// keys, not React-style position-in-call-order. See the design doc's
// "State model" section for rationale.
//
// Rebuilt-every-frame discipline: callers call `BeginFrame()` before the
// render pass, `EndFrame()` after. EndFrame GCs any IDs that weren't
// touched this frame, so screens that vanish from the tree don't leak
// state forever.
//
// `BeginFrame()` does NOT clear the maps — only `EndFrame()` does, after
// it has compared the seen-set to existing entries. The maps survive
// across frames; that's the whole point.

constexpr uint64_t TAG_HOT    = 0x483a8e21d8c5c0d3ULL;
constexpr uint64_t TAG_ACTIVE = 0x6c2bbe5d4a107fb1ULL;

// FNV-1a 64-bit. Stable across runs and platforms; small + branchless.
inline uint64_t Hash64(const char * s, size_t n) {
	uint64_t h = 0xcbf29ce484222325ULL;
	for(size_t i = 0; i < n; i++){
		h ^= (unsigned char)s[i];
		h *= 0x100000001b3ULL;
	}
	return h;
}

inline uint64_t HashKey(const std::string & key) {
	return Hash64(key.data(), key.size());
}

// Exponentially approach `target` from `current`. `rate` is the
// per-second rate constant; `dt` in seconds. Snaps when within an
// integer-mapping epsilon so callers that floor `t * 4` actually
// reach the endpoint instead of asymptoting at 0.999.
inline float Approach(float current, float target, float rate, float dt) {
	if(dt <= 0.0f) return current;
	float k = 1.0f - std::exp(-rate * dt);
	float next = current + (target - current) * k;
	if(std::fabs(target - next) < 1e-3f) next = target;
	return next;
}

struct UIState {
	std::unordered_map<uint64_t, float>   anim;
	std::unordered_set<uint64_t>          seen_this_frame;

	void BeginFrame() {
		seen_this_frame.clear();
	}

	void EndFrame() {
		for(auto it = anim.begin(); it != anim.end(); ){
			if(seen_this_frame.find(it->first) == seen_this_frame.end()){
				it = anim.erase(it);
			}else{
				++it;
			}
		}
	}

	// Get-or-create a float slot keyed by `id`. Records the visit so
	// EndFrame doesn't GC it.
	float & AnimSlot(uint64_t id, float initial = 0.0f) {
		seen_this_frame.insert(id);
		auto it = anim.find(id);
		if(it == anim.end()){
			it = anim.emplace(id, initial).first;
		}
		return it->second;
	}
};

}  // namespace v2
}  // namespace ui

#endif
