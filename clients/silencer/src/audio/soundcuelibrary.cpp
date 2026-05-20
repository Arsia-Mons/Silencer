#include "soundcuelibrary.h"
#include "resources.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
SoundCueLibrary& SoundCueLibrary::Get() {
    static SoundCueLibrary instance;
    return instance;
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------
void SoundCueLibrary::Load(const std::string& dir, Resources& /*res*/) {
    cues_.clear();
    seqCounters_.clear();

    if (!fs::exists(dir)) return;

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".json") continue;
        try {
            SoundCue cue = ParseCue(entry.path().string());
            if (!cue.id.empty()) {
                seqCounters_[cue.id] = {};
                cues_[cue.id] = std::move(cue);
            }
        } catch (...) {
            // Malformed cue — skip silently.
        }
    }
    loaded_ = true;
}

// ---------------------------------------------------------------------------
// Evaluate
// ---------------------------------------------------------------------------
SoundCueResult SoundCueLibrary::Evaluate(const std::string& cueId, Resources& res) {
    auto it = cues_.find(cueId);
    if (it == cues_.end()) return {};
    return it->second.Evaluate(res, seqCounters_[cueId]);
}

// ---------------------------------------------------------------------------
// ParseCue — parse one JSON file into a SoundCue
// ---------------------------------------------------------------------------
SoundCue SoundCueLibrary::ParseCue(const std::string& path) {
    std::ifstream f(path);
    json j = json::parse(f);

    SoundCue cue;
    cue.id = j.value("id", "");

    // Parse nodes
    for (const auto& jn : j.at("nodes")) {
        SoundCueNode node;
        node.id   = jn.at("id").get<std::string>();
        std::string typeStr = jn.at("type").get<std::string>();

        if      (typeStr == "WavePlayer") node.type = SoundCueNode::Type::WavePlayer;
        else if (typeStr == "Random")     node.type = SoundCueNode::Type::Random;
        else if (typeStr == "Sequence")   node.type = SoundCueNode::Type::Sequence;
        else if (typeStr == "Mixer")      node.type = SoundCueNode::Type::Mixer;
        else if (typeStr == "Delay")      node.type = SoundCueNode::Type::Delay;
        else if (typeStr == "Volume")     node.type = SoundCueNode::Type::Volume;
        else if (typeStr == "Pitch")      node.type = SoundCueNode::Type::Pitch;
        else if (typeStr == "Output")     node.type = SoundCueNode::Type::Output;

        if (jn.contains("data") && !jn["data"].is_null()) {
            const auto& d = jn["data"];
            node.file      = d.value("file",      "");
            node.weight    = d.value("weight",    1.0f);
            node.shuffle   = d.value("shuffle",   false);
            node.scalar    = d.value("scalar",    1.0f);
            node.semitones = d.value("semitones", 0.0f);
            node.minSec    = d.value("minSec",    0.0f);
            node.maxSec    = d.value("maxSec",    0.0f);
            if (d.contains("mixerVolumes") && d["mixerVolumes"].is_array()) {
                for (float v : d["mixerVolumes"]) node.mixerVolumes.push_back(v);
            }
        }

        if (node.type == SoundCueNode::Type::Output) cue.outputNodeId = node.id;
        cue.nodes[node.id] = std::move(node);
    }

    // Parse edges — build input lists on target nodes (ordered by targetHandle port index)
    // targetHandle format: "in-0", "in-1", ... or "in" (Output)
    struct EdgeEntry { int port; std::string sourceId; };
    std::unordered_map<std::string, std::vector<EdgeEntry>> edgeMap;

    for (const auto& je : j.at("edges")) {
        std::string src    = je.at("source").get<std::string>();
        std::string tgt    = je.at("target").get<std::string>();
        std::string tgtH   = je.value("targetHandle", "in");
        int port = 0;
        if (tgtH.size() > 3 && tgtH.compare(0, 3, "in-") == 0) {
            port = std::stoi(tgtH.substr(3));
        }
        edgeMap[tgt].push_back({ port, src });
    }

    for (auto& [tgtId, entries] : edgeMap) {
        std::sort(entries.begin(), entries.end(), [](const EdgeEntry& a, const EdgeEntry& b) {
            return a.port < b.port;
        });
        auto nit = cue.nodes.find(tgtId);
        if (nit != cue.nodes.end()) {
            for (auto& e : entries) nit->second.inputs.push_back(e.sourceId);
        }
    }

    return cue;
}
