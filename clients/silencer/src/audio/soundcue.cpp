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
    std::unordered_map<std::string, int>& seqCounters) const
{
    if (outputNodeId.empty()) return {};
    const auto it = nodes.find(outputNodeId);
    if (it == nodes.end()) return {};
    // The Output node has exactly one input.
    if (it->second.inputs.empty()) return {};
    return EvalNode(it->second.inputs[0], res, seqCounters);
}

// ---------------------------------------------------------------------------
// SoundCue::EvalNode — recursive graph walk
// ---------------------------------------------------------------------------
SoundCueResult SoundCue::EvalNode(
    const std::string& nodeId,
    Resources& res,
    std::unordered_map<std::string, int>& seqCounters) const
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
        // Collect weights from child WavePlayer nodes (or treat non-WavePlayer as weight 1).
        float totalWeight = 0.f;
        std::vector<float> weights;
        weights.reserve(node.inputs.size());
        for (const auto& inputId : node.inputs) {
            const auto cit = nodes.find(inputId);
            float w = (cit != nodes.end()) ? cit->second.weight : 1.f;
            if (w <= 0.f) w = 1.f;
            weights.push_back(w);
            totalWeight += w;
        }
        // Weighted random pick.
        float draw = (static_cast<float>(std::rand()) / RAND_MAX) * totalWeight;
        float acc = 0.f;
        for (size_t i = 0; i < node.inputs.size(); ++i) {
            acc += weights[i];
            if (draw < acc) return EvalNode(node.inputs[i], res, seqCounters);
        }
        return EvalNode(node.inputs.back(), res, seqCounters);
    }

    case SoundCueNode::Type::Sequence: {
        if (node.inputs.empty()) return {};
        int& counter = seqCounters[nodeId];
        int idx = counter % static_cast<int>(node.inputs.size());
        ++counter;
        if (node.shuffle && counter % static_cast<int>(node.inputs.size()) == 0) {
            // Shuffle order for next cycle by incrementing counter randomly.
            // Simple: just let the caller experience natural variety via rand() elsewhere.
        }
        return EvalNode(node.inputs[idx], res, seqCounters);
    }

    case SoundCueNode::Type::Mixer: {
        // Evaluate the first connected input as the primary chunk; additional
        // inputs are returned as separate results via SoundCueLibrary's multi-
        // result path (not implemented here — Mixer in a single-chunk path
        // just picks input 0 and accumulates volumes).
        if (node.inputs.empty()) return {};
        SoundCueResult r = EvalNode(node.inputs[0], res, seqCounters);
        for (size_t i = 0; i < node.inputs.size() && i < node.mixerVolumes.size(); ++i) {
            if (i == 0) r.volume *= node.mixerVolumes[i];
        }
        return r;
    }

    case SoundCueNode::Type::Delay: {
        if (node.inputs.empty()) return {};
        SoundCueResult r = EvalNode(node.inputs[0], res, seqCounters);
        float range = node.maxSec - node.minSec;
        if (range < 0.f) range = 0.f;
        r.delaySec += node.minSec + (static_cast<float>(std::rand()) / RAND_MAX) * range;
        return r;
    }

    case SoundCueNode::Type::Volume: {
        if (node.inputs.empty()) return {};
        SoundCueResult r = EvalNode(node.inputs[0], res, seqCounters);
        r.volume *= node.scalar;
        return r;
    }

    case SoundCueNode::Type::Pitch: {
        if (node.inputs.empty()) return {};
        SoundCueResult r = EvalNode(node.inputs[0], res, seqCounters);
        r.pitch += node.semitones;
        return r;
    }

    case SoundCueNode::Type::Output:
        // Should not recurse into Output from EvalNode.
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
