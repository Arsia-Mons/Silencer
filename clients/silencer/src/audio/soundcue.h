#pragma once
#include "shared.h"
#include <string>
#include <vector>
#include <unordered_map>

class Resources;

// ---------------------------------------------------------------------------
// SoundCueResult — returned by SoundCue::Evaluate and ResolveSound.
// Callers apply volume/pitch/delaySec on top of the plain EmitSound call.
// ---------------------------------------------------------------------------
struct SoundCueResult {
    Mix_Chunk* chunk = nullptr;
    float volume   = 1.0f;  // composite scalar from Volume nodes (multiply with caller's base volume)
    float pitch    = 0.0f;  // semitone offset from Pitch nodes
    float delaySec = 0.0f;  // seconds to wait before playing (from Delay nodes)
};

// ---------------------------------------------------------------------------
// SoundCueNode — one node in the graph, loaded from JSON.
// ---------------------------------------------------------------------------
struct SoundCueNode {
    enum class Type {
        WavePlayer,
        Random,
        Sequence,
        Mixer,
        Delay,
        Volume,
        Pitch,
        Output,
    };

    std::string id;
    Type type = Type::WavePlayer;

    // WavePlayer
    std::string file;
    float weight = 1.0f;

    // Random / Sequence / Mixer: list of input node ids (in port order)
    std::vector<std::string> inputs;

    // Sequence
    bool shuffle = false;

    // Mixer: per-port volume scalars (parallel to inputs; defaults to 1.0)
    std::vector<float> mixerVolumes;

    // Delay
    float minSec = 0.0f;
    float maxSec = 0.0f;

    // Volume
    float scalar = 1.0f;

    // Pitch
    float semitones = 0.0f;
};

// ---------------------------------------------------------------------------
// SoundCue — an immutable graph loaded once from JSON.
// Evaluate() walks the graph from Output → leaves and returns a result.
// seqCounters is per-instance mutable state (Sequence round-robin index).
// ---------------------------------------------------------------------------
class SoundCue {
public:
    std::string id;
    // Loaded nodes keyed by id; Output node id stored separately.
    std::unordered_map<std::string, SoundCueNode> nodes;
    std::string outputNodeId;

    // Evaluate the graph. seqCounters tracks round-robin state per Sequence
    // node id and must persist across calls (owned by SoundCueLibrary per cue).
    // randomLastPick tracks the last chosen input index per Random node (no-repeat).
    SoundCueResult Evaluate(
        Resources& res,
        std::unordered_map<std::string, int>& seqCounters,
        std::unordered_map<std::string, int>& randomLastPick) const;

private:
    SoundCueResult EvalNode(
        const std::string& nodeId,
        Resources& res,
        std::unordered_map<std::string, int>& seqCounters,
        std::unordered_map<std::string, int>& randomLastPick) const;
};

// ---------------------------------------------------------------------------
// ResolveSound — drop-in replacement for soundbank[] lookups.
//
//   slot = "futstonl.wav"         → looks up soundbank directly
//   slot = "cue:footstep_concrete" → evaluates through SoundCueLibrary
//
// Returns a SoundCueResult. chunk may be nullptr if the file/cue is missing.
// ---------------------------------------------------------------------------
SoundCueResult ResolveSound(const std::string& slot, Resources& res);
