#include "behaviortree.h"
#include <algorithm>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dirent.h>
#endif

// ── BehaviorTree::fromJson ────────────────────────────────────────────────────
BehaviorTree BehaviorTree::fromJson(const json& j) {
    BehaviorTree bt;
    bt.rootId_ = j.value("rootId", std::string{});
    const auto& nodes = j.at("nodes");
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        Node n;
        n.id       = it.key();
        n.type     = it.value().value("type", std::string{});
        n.label    = it.value().value("label", n.id);
        n.props    = it.value().value("props", json::object());
        if (it.value().count("children")) {
            for (auto& c : it.value().at("children"))
                n.children.push_back(c.get<std::string>());
        }
        bt.nodes_[n.id] = std::move(n);
    }
    return bt;
}

// ── BehaviorTree::tick ────────────────────────────────────────────────────────
BTResult BehaviorTree::tick(BTContext& ctx) const {
    if (rootId_.empty()) return BTResult::Failure;
    return tickNode(rootId_, ctx);
}

BTResult BehaviorTree::tickNode(const std::string& id, BTContext& ctx) const {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) return BTResult::Failure;
    const Node& n = it->second;

    if (n.type == "Selector")       return tickSelector(n, ctx);
    if (n.type == "Sequence")       return tickSequence(n, ctx);
    if (n.type == "Parallel")       return tickParallel(n, ctx);
    if (n.type == "RandomSelector") return tickRandomSelector(n, ctx);
    if (n.type == "Inverter")       return tickInverter(n, ctx);
    if (n.type == "Cooldown")       return tickCooldown(n, ctx);
    if (n.type == "Repeat")         return tickRepeat(n, ctx);
    if (n.type == "Timeout")        return tickTimeout(n, ctx);
    if (n.type == "ForceSuccess")   return tickForceSuccess(n, ctx);
    if (n.type == "Wait")           return tickWait(n, ctx);
    if (n.type == "Leaf")           return tickLeaf(n, ctx);
    if (n.type == "Condition")      return tickCondition(n, ctx);
    return BTResult::Failure;
}

// RandomSelector: shuffle children each tick, then behave like Selector
BTResult BehaviorTree::tickRandomSelector(const Node& n, BTContext& ctx) const {
    std::vector<size_t> order(n.children.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    for (size_t i = order.size(); i > 1; --i) {
        size_t j = (size_t)rand() % i;
        std::swap(order[i - 1], order[j]);
    }
    for (size_t i : order) {
        BTResult r = tickNode(n.children[i], ctx);
        if (r == BTResult::Success || r == BTResult::Running) return r;
    }
    return BTResult::Failure;
}

// Timeout: fail child after `duration` seconds
BTResult BehaviorTree::tickTimeout(const Node& n, BTContext& ctx) const {
    if (n.children.empty()) return BTResult::Failure;
    float duration = n.props.value("duration", 5.0f);
    std::string key = n.id + "_to";

    float elapsed = 0.0f;
    auto it = ctx.state.find(key);
    if (it != ctx.state.end()) elapsed = it->second.get<float>();

    elapsed += ctx.dt;
    if (elapsed >= duration) {
        ctx.state.erase(key);
        return BTResult::Failure;
    }

    BTResult r = tickNode(n.children[0], ctx);
    if (r == BTResult::Running) {
        ctx.state[key] = elapsed;
        return BTResult::Running;
    }
    ctx.state.erase(key);
    return r;
}

// ForceSuccess: run child, always return Success
BTResult BehaviorTree::tickForceSuccess(const Node& n, BTContext& ctx) const {
    if (!n.children.empty()) tickNode(n.children[0], ctx);
    return BTResult::Success;
}

// Wait: return Running for `duration` seconds, then Success
BTResult BehaviorTree::tickWait(const Node& n, BTContext& ctx) const {
    float duration = n.props.value("duration", 1.0f);
    std::string key = n.id + "_wait";

    float elapsed = 0.0f;
    auto it = ctx.state.find(key);
    if (it != ctx.state.end()) elapsed = it->second.get<float>();

    elapsed += ctx.dt;
    if (elapsed >= duration) {
        ctx.state.erase(key);
        return BTResult::Success;
    }
    ctx.state[key] = elapsed;
    return BTResult::Running;
}

// Selector: return Success on first child that succeeds; Failure if all fail
BTResult BehaviorTree::tickSelector(const Node& n, BTContext& ctx) const {
    for (const auto& cid : n.children) {
        BTResult r = tickNode(cid, ctx);
        if (r == BTResult::Success || r == BTResult::Running) return r;
    }
    return BTResult::Failure;
}

// Sequence (memory): return Failure on first child that fails; Success if all succeed
BTResult BehaviorTree::tickSequence(const Node& n, BTContext& ctx) const {
    // Resume from last Running child
    int start = 0;
    auto sit = ctx.state.find(n.id + "_seq");
    if (sit != ctx.state.end()) start = sit->second.get<int>();

    for (int i = start; i < (int)n.children.size(); ++i) {
        BTResult r = tickNode(n.children[i], ctx);
        if (r == BTResult::Running) {
            ctx.state[n.id + "_seq"] = i;
            return BTResult::Running;
        }
        if (r == BTResult::Failure) {
            ctx.state.erase(n.id + "_seq");
            return BTResult::Failure;
        }
    }
    ctx.state.erase(n.id + "_seq");
    return BTResult::Success;
}

// Parallel: tick all children; succeed if ≥ threshold succeed (default: all)
BTResult BehaviorTree::tickParallel(const Node& n, BTContext& ctx) const {
    int threshold = n.props.value("threshold", (int)n.children.size());
    int successes = 0;
    for (const auto& cid : n.children) {
        BTResult r = tickNode(cid, ctx);
        if (r == BTResult::Success) ++successes;
    }
    if (successes >= threshold) return BTResult::Success;
    return BTResult::Running;
}

// Inverter: invert child result (Running passes through)
BTResult BehaviorTree::tickInverter(const Node& n, BTContext& ctx) const {
    if (n.children.empty()) return BTResult::Failure;
    BTResult r = tickNode(n.children[0], ctx);
    if (r == BTResult::Success) return BTResult::Failure;
    if (r == BTResult::Failure) return BTResult::Success;
    return BTResult::Running;
}

// Cooldown: block child for `duration` seconds after it returns Success
BTResult BehaviorTree::tickCooldown(const Node& n, BTContext& ctx) const {
    float duration = n.props.value("duration", 1.0f);
    std::string timerKey = n.id + "_cd";

    auto it = ctx.state.find(timerKey);
    if (it != ctx.state.end()) {
        float remaining = it->second.get<float>() - ctx.dt;
        if (remaining > 0.0f) {
            ctx.state[timerKey] = remaining;
            return BTResult::Failure; // still on cooldown
        }
        ctx.state.erase(timerKey);
    }

    if (n.children.empty()) return BTResult::Failure;
    BTResult r = tickNode(n.children[0], ctx);
    if (r == BTResult::Success) ctx.state[timerKey] = duration;
    return r;
}

// Repeat: repeat child up to `count` times (0 = infinite); returns Running while repeating
BTResult BehaviorTree::tickRepeat(const Node& n, BTContext& ctx) const {
    int maxCount = n.props.value("count", 0);
    std::string cntKey = n.id + "_rep";
    int done = 0;
    auto it = ctx.state.find(cntKey);
    if (it != ctx.state.end()) done = it->second.get<int>();

    if (maxCount > 0 && done >= maxCount) {
        ctx.state.erase(cntKey);
        return BTResult::Success;
    }
    if (n.children.empty()) return BTResult::Failure;

    BTResult r = tickNode(n.children[0], ctx);
    if (r != BTResult::Running) {
        ctx.state[cntKey] = done + 1;
        if (maxCount > 0 && done + 1 >= maxCount) {
            ctx.state.erase(cntKey);
            return BTResult::Success;
        }
        return BTResult::Running; // loop again next tick
    }
    return BTResult::Running;
}

// Leaf: dispatch to registered action handler
BTResult BehaviorTree::tickLeaf(const Node& n, BTContext& ctx) const {
    std::string action = n.props.value("action", std::string{});
    auto it = ctx.actions.find(action);
    if (it == ctx.actions.end()) return BTResult::Failure;
    return it->second(ctx);
}

// Condition: compare blackboard value
BTResult BehaviorTree::tickCondition(const Node& n, BTContext& ctx) const {
    std::string key = n.props.value("key", std::string{});
    std::string op  = n.props.value("op",  std::string{"=="});
    json        val = n.props.value("value", json{});

    auto it = ctx.blackboard.find(key);
    if (it == ctx.blackboard.end()) return BTResult::Failure;

    const json& actual = it->second;
    bool result = false;
    try {
        if      (op == "==") result = (actual == val);
        else if (op == "!=") result = (actual != val);
        else if (op == ">" ) result = (actual.get<double>() >  val.get<double>());
        else if (op == "<" ) result = (actual.get<double>() <  val.get<double>());
        else if (op == ">=") result = (actual.get<double>() >= val.get<double>());
        else if (op == "<=") result = (actual.get<double>() <= val.get<double>());
    } catch (...) {
        return BTResult::Failure;
    }
    return result ? BTResult::Success : BTResult::Failure;
}

// ── BehaviorTreeLibrary ───────────────────────────────────────────────────────
BehaviorTreeLibrary& BehaviorTreeLibrary::instance() {
    static BehaviorTreeLibrary lib;
    return lib;
}

const BehaviorTree* BehaviorTreeLibrary::get(const std::string& id) const {
    auto it = trees_.find(id);
    return it == trees_.end() ? nullptr : &it->second;
}

void BehaviorTreeLibrary::loadDir(const std::string& dir) {
#if defined(_WIN32)
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*.json").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::string name(fd.cFileName);
        std::string id = name.substr(0, name.size() - 5);
        std::ifstream f(dir + "\\" + name);
        if (!f) continue;
        try {
            json j = json::parse(f);
            trees_[id] = BehaviorTree::fromJson(j);
        } catch (...) {}
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name(ent->d_name);
        if (name.size() < 5 || name.substr(name.size() - 5) != ".json") continue;
        std::string id = name.substr(0, name.size() - 5);
        std::ifstream f(dir + "/" + name);
        if (!f) continue;
        try {
            json j = json::parse(f);
            trees_[id] = BehaviorTree::fromJson(j);
        } catch (...) {}
    }
    closedir(d);
#endif
}
