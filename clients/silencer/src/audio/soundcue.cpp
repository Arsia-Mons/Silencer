#include "soundcue.h"
#include "soundcuelibrary.h"
#include "resources.h"
#include <algorithm>
#include <cstdlib>
#include <numeric>

// ---------------------------------------------------------------------------
// SoundCue::Evaluate
// ---------------------------------------------------------------------------
SoundCueResult SoundCue::Evaluate(
    Resources& res,
    std::unordered_map<std::string, int>& seqCounters,
    std::unordered_map<std::string, int>& randomLastPick,
    const std::unordered_map<std::string, bool>& params) const
{
    if (outputNodeId.empty()) return {};
    const auto it = nodes.find(outputNodeId);
    if (it == nodes.end()) return {};
    if (it->second.inputs.empty()) return {};
    return EvalNode(it->second.inputs[0], res, seqCounters, randomLastPick, params);
}

// ---------------------------------------------------------------------------
// SoundCue::EvalNode — recursive graph walk
// ---------------------------------------------------------------------------
SoundCueResult SoundCue::EvalNode(
    const std::string& nodeId,
    Resources& res,
    std::unordered_map<std::string, int>& seqCounters,
    std::unordered_map<std::string, int>& randomLastPick,
    const std::unordered_map<std::string, bool>& params) const
{
    const auto it = nodes.find(nodeId);
    if (it == nodes.end()) return {};
    const SoundCueNode& node = it->second;

    switch (node.type) {

    case SoundCueNode::Type::WavePlayer: {
        SoundCueResult r;
        auto sit = res.soundbank.find(node.file);
        if (sit != res.soundbank.end()) r.chunk = sit->second;
        return r;
    }

    case SoundCueNode::Type::Random: {
        if (node.inputs.empty()) return {};
        // Collect weights; zero out last-picked index to prevent repeats.
        const int lastPick = [&]() {
            auto pit = randomLastPick.find(nodeId);
            return pit != randomLastPick.end() ? pit->second : -1;
        }();
        float totalWeight = 0.f;
        std::vector<float> weights;
        weights.reserve(node.inputs.size());
        for (size_t i = 0; i < node.inputs.size(); ++i) {
            float w = 0.f;
            if (node.inputs.size() == 1 || static_cast<int>(i) != lastPick) {
                const auto cit = nodes.find(node.inputs[i]);
                w = (cit != nodes.end()) ? cit->second.weight : 1.f;
                if (w <= 0.f) w = 1.f;
            }
            weights.push_back(w);
            totalWeight += w;
        }
        // Weighted random pick.
        float draw = (static_cast<float>(std::rand()) / RAND_MAX) * totalWeight;
        float acc = 0.f;
        for (size_t i = 0; i < node.inputs.size(); ++i) {
            acc += weights[i];
            if (draw < acc) {
                randomLastPick[nodeId] = static_cast<int>(i);
                return EvalNode(node.inputs[i], res, seqCounters, randomLastPick, params);
            }
        }
        randomLastPick[nodeId] = static_cast<int>(node.inputs.size()) - 1;
        return EvalNode(node.inputs.back(), res, seqCounters, randomLastPick, params);
    }

    case SoundCueNode::Type::Sequence: {
        if (node.inputs.empty()) return {};
        int& counter = seqCounters[nodeId];
        int idx = counter % static_cast<int>(node.inputs.size());
        ++counter;
        return EvalNode(node.inputs[idx], res, seqCounters, randomLastPick, params);
    }

    case SoundCueNode::Type::Mixer: {
        if (node.inputs.empty()) return {};
        SoundCueResult r = EvalNode(node.inputs[0], res, seqCounters, randomLastPick, params);
        for (size_t i = 0; i < node.inputs.size() && i < node.mixerVolumes.size(); ++i) {
            if (i == 0) r.volume *= node.mixerVolumes[i];
        }
        return r;
    }

    case SoundCueNode::Type::Delay: {
        if (node.inputs.empty()) return {};
        SoundCueResult r = EvalNode(node.inputs[0], res, seqCounters, randomLastPick, params);
        float range = node.maxSec - node.minSec;
        if (range < 0.f) range = 0.f;
        r.delaySec += node.minSec + (static_cast<float>(std::rand()) / RAND_MAX) * range;
        return r;
    }

    case SoundCueNode::Type::Volume: {
        if (node.inputs.empty()) return {};
        SoundCueResult r = EvalNode(node.inputs[0], res, seqCounters, randomLastPick, params);
        r.volume *= node.scalar;
        return r;
    }

    case SoundCueNode::Type::Pitch: {
        if (node.inputs.empty()) return {};
        SoundCueResult r = EvalNode(node.inputs[0], res, seqCounters, randomLastPick, params);
        r.pitch += node.semitones;
        return r;
    }

    case SoundCueNode::Type::Modulator: {
        if (node.inputs.empty()) return {};
        SoundCueResult r = EvalNode(node.inputs[0], res, seqCounters, randomLastPick, params);
        auto frand = [](float lo, float hi) -> float {
            if (hi <= lo) return lo;
            return lo + (hi - lo) * (static_cast<float>(std::rand()) / RAND_MAX);
        };
        r.volume *= frand(node.volumeMin, node.volumeMax);
        r.pitch += frand(node.pitchMin, node.pitchMax);
        return r;
    }

    case SoundCueNode::Type::Concatenator: {
        // Sequential playback requires stateful audio tracking; at runtime
        // we evaluate the indexed input only.
        if (node.inputs.empty()) return {};
        int& counter = seqCounters[nodeId];
        int idx = counter % static_cast<int>(node.inputs.size());
        ++counter;
        return EvalNode(node.inputs[idx], res, seqCounters, randomLastPick, params);
    }

    case SoundCueNode::Type::Looping: {
        if (node.inputs.empty()) return {};
        return EvalNode(node.inputs[0], res, seqCounters, randomLastPick, params);
    }

    case SoundCueNode::Type::Branch: {
        if (node.inputs.size() < 2) return {};
        bool val = false;
        auto pit = params.find(node.paramName);
        if (pit != params.end()) val = pit->second;
        return EvalNode(node.inputs[val ? 1 : 0], res, seqCounters, randomLastPick, params);
    }

    case SoundCueNode::Type::Output:
        return {};
    }
    return {};
}

// ---------------------------------------------------------------------------
// ResolveSound
// ---------------------------------------------------------------------------
SoundCueResult ResolveSound(const std::string& slot, Resources& res) {
    if (slot.size() > 4 && slot.compare(0, 4, "cue:") == 0) {
        return SoundCueLibrary::Get().Evaluate(slot.substr(4), res);
    }
    SoundCueResult r;
    auto it = res.soundbank.find(slot);
    if (it != res.soundbank.end()) r.chunk = it->second;
    return r;
}
