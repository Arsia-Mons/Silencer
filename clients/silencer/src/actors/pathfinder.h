#pragma once
/**
 * pathfinder.h — Platform-set BFS for NPC navigation.
 *
 * Provides a generic BFS that finds the shortest platform-set path
 * between two points in the world. Each NPC type supplies its own
 * canLink predicate; ground-walkers use LadderCanLink.
 */
#include <deque>
#include <functional>

struct World;
struct PlatformSet;

namespace Pathfinder {

// BFS over the world's platform-set graph.
// canLink(world, from, to) returns true if the NPC can traverse the edge.
// Returns an ordered path [from, ..., to], or an empty deque if unreachable.
std::deque<PlatformSet*> FindPath(
    World& world,
    PlatformSet& from,
    PlatformSet& to,
    std::function<bool(World&, PlatformSet&, PlatformSet&)> canLink
);

// Link predicate for ground-walking NPCs (guards, robots, civilians).
// Returns true if there is at least one ladder connecting from → to.
bool LadderCanLink(World& world, PlatformSet& from, PlatformSet& to);

} // namespace Pathfinder
