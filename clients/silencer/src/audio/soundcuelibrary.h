#pragma once
#include <string>
#include <unordered_map>
#include "soundcue.h"

class Resources;

// ---------------------------------------------------------------------------
// SoundCueLibrary — singleton.
// Loads every *.json in shared/assets/gas/sound-cues/ at startup.
// Call SoundCueLibrary::Get().Evaluate(cueId, res) to play a cue.
// ---------------------------------------------------------------------------
class SoundCueLibrary {
public:
    static SoundCueLibrary& Get();

    // Load all *.json files from the given directory.
    // Called once during Resources::Load (after sounds are loaded so the
    // WavePlayer nodes can resolve their Mix_Chunk* immediately).
    void Load(const std::string& dir, Resources& res);

    // Evaluate cue by id. Returns an empty SoundCueResult (chunk==nullptr)
    // if the cue is not found.
    SoundCueResult Evaluate(const std::string& cueId, Resources& res);

    bool IsLoaded() const { return loaded_; }

private:
    SoundCueLibrary() = default;

    std::unordered_map<std::string, SoundCue> cues_;
    // Per-cue Sequence counters: cueId → (nodeId → counter)
    std::unordered_map<std::string, std::unordered_map<std::string, int>> seqCounters_;
    // Per-cue Random last-pick: cueId → (nodeId → last chosen index)
    std::unordered_map<std::string, std::unordered_map<std::string, int>> randomLastPick_;
    bool loaded_ = false;

    static SoundCue ParseCue(const std::string& path);
};
