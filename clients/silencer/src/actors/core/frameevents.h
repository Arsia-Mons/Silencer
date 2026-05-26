#pragma once
#include "actordef.h"
#include "object.h"
#include <string>

class World;

// ---------------------------------------------------------------------------
// FireFrameEvent — resolve and play a frame animation event.
//
// The `event` field on a FrameDef is a semantic tag describing what just
// happened at this frame. The actual sound is resolved from context:
//
//   "footstep:L"   → left-foot step; looks up physics material footstepL
//                    (or actorDef.footstepL override if set)
//   "footstep:R"   → right-foot step; same for footstepR
//   "cue:some_id"  → plays the named cue directly (no material lookup)
//   anything else  → passed to ResolveSound (bare cue name or legacy WAV)
//
// adef     — nullable; checked for per-actor footstep overrides
// platformId — currentplatformid of the calling actor (0 = default material)
// actor    — source Object, used for positional EmitSound
// world    — for platform and resource lookups
// baseVol  — SDL volume scalar (0–128); scaled by cue volume node output
// ---------------------------------------------------------------------------
void FireFrameEvent(const std::string& event,
                    const ActorDef*    adef,
                    Uint16             platformId,
                    Object&            actor,
                    World&             world,
                    int                baseVol = 64);
