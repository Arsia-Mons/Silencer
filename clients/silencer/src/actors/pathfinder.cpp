#include "pathfinder.h"
#include <map>
#include "../world/world.h"
#include "../world/map.h"
#include "../objects/platformset.h"

namespace Pathfinder {

std::deque<PlatformSet*> FindPath(
    World& world,
    PlatformSet& from,
    PlatformSet& to,
    std::function<bool(World&, PlatformSet&, PlatformSet&)> canLink
) {
    if (&from == &to) return {&from};

    std::map<PlatformSet*, PlatformSet*> parent; // node → predecessor
    std::deque<PlatformSet*> queue;
    queue.push_back(&from);
    parent[&from] = nullptr;

    while (!queue.empty()) {
        PlatformSet* cur = queue.front(); queue.pop_front();
        for (auto& sp : world.map.platformsets) {
            PlatformSet* next = sp.get();
            if (parent.count(next)) continue;
            if (!canLink(world, *cur, *next)) continue;
            parent[next] = cur;
            if (next == &to) {
                std::deque<PlatformSet*> path;
                for (PlatformSet* n = next; n != nullptr; n = parent[n])
                    path.push_front(n);
                return path;
            }
            queue.push_back(next);
        }
    }
    return {}; // unreachable
}

bool LadderCanLink(World& world, PlatformSet& from, PlatformSet& to) {
    return !world.map.LaddersToPlatform(from, to).empty();
}

} // namespace Pathfinder
