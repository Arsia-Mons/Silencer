#pragma once
/**
 * behaviortree.h — Tick-based behavior tree interpreter (C++14)
 *
 * Return values: Success, Failure, Running
 * Node types:   Selector, Sequence, Parallel, Inverter, Cooldown, Repeat,
 *               Leaf (action dispatch), Condition (blackboard compare)
 *
 * Blackboard values use nlohmann::json (already a project dep via actordef).
 * Per-actor-instance state lives in BTContext so trees are stateless/shared.
 */
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <stdexcept>
#include "json.hpp"

using json = nlohmann::json;

// ── Result ────────────────────────────────────────────────────────────────────
enum class BTResult { Success, Failure, Running };

// ── Forward declarations ──────────────────────────────────────────────────────
struct BTContext;

// ── Leaf action handler ───────────────────────────────────────────────────────
// Return Success/Failure/Running; may read/write ctx.blackboard
using BTLeafFn = std::function<BTResult(BTContext&)>;

// ── Per-instance runtime state ────────────────────────────────────────────────
struct BTContext {
    // Shared blackboard — keyed by blackboard key name
    std::unordered_map<std::string, json> blackboard;

    // Per-node running state (cooldown timers, repeat counters)
    std::unordered_map<std::string, json> state;

    // Action registry — maps action name → handler
    std::unordered_map<std::string, BTLeafFn> actions;

    // Delta-time for this tick (seconds)
    float dt = 0.0f;

    // Helper to read a blackboard value with a typed default
    template<typename T>
    T bb(const std::string& key, T def = T{}) const {
        auto it = blackboard.find(key);
        if (it == blackboard.end()) return def;
        try { return it->second.get<T>(); } catch (...) { return def; }
    }

    void bbSet(const std::string& key, json val) {
        blackboard[key] = std::move(val);
    }
};

// ── BehaviorTree ──────────────────────────────────────────────────────────────
class BehaviorTree {
public:
    // Load from actor def JSON (the behaviortree JSON file contents)
    static BehaviorTree fromJson(const json& j);

    // Tick the tree. Returns Success, Failure, or Running.
    BTResult tick(BTContext& ctx) const;

    bool valid() const { return !rootId_.empty(); }

private:
    struct Node {
        std::string id;
        std::string type;    // Selector|Sequence|Parallel|Inverter|Cooldown|Repeat|Leaf|Condition
        std::string label;
        std::vector<std::string> children;
        json props;
    };

    std::string rootId_;
    std::unordered_map<std::string, Node> nodes_;

    BTResult tickNode(const std::string& id, BTContext& ctx) const;
    BTResult tickSelector(const Node& n, BTContext& ctx) const;
    BTResult tickSequence(const Node& n, BTContext& ctx) const;
    BTResult tickParallel(const Node& n, BTContext& ctx) const;
    BTResult tickInverter(const Node& n, BTContext& ctx) const;
    BTResult tickCooldown(const Node& n, BTContext& ctx) const;
    BTResult tickRepeat(const Node& n, BTContext& ctx) const;
    BTResult tickLeaf(const Node& n, BTContext& ctx) const;
    BTResult tickCondition(const Node& n, BTContext& ctx) const;
};

// ── BehaviorTreeLibrary ───────────────────────────────────────────────────────
// Loads and caches all .json files from shared/assets/behaviortrees/
class BehaviorTreeLibrary {
public:
    static BehaviorTreeLibrary& instance();

    void loadDir(const std::string& dir);
    const BehaviorTree* get(const std::string& id) const;

private:
    std::unordered_map<std::string, BehaviorTree> trees_;
};
