#ifndef TRIGGER_GRAPH_H
#define TRIGGER_GRAPH_H

#include "TriggerDef.h"
#include "EventBus.h"
#include "ActionSystem.h"
#include <vector>
#include <set>
#include <array>
#include <unordered_map>

class World;
class Serializer;

// Runtime trigger graph.  Owned by World.
// Only the AUTHORITY peer evaluates triggers and schedules actions.
class TriggerGraph {
public:
    void Load(const std::vector<TriggerNode> & nodes,
              const std::vector<ObjectiveDef> & objectives);
    void LoadZones(const std::vector<TriggerZone> & zones);
    void Clear();

    void Tick(World & world, float dt);

    // Called by AUTHORITY to mark an objective complete; replicates via
    // objective_state_ and the normal snapshot mechanism.
    void CompleteObjective(Uint16 id, World & world);

    bool IsObjectiveComplete(Uint16 id) const;

    // Enable/disable a trigger by its node id (used by DISABLE/ENABLE_TRIGGER).
    void SetEnabled(Uint16 node_id, bool enabled);

    // Flag accessors (flag_id 0-255).
    void SetFlag(Uint8 flag_id, bool value);
    bool GetFlag(Uint8 flag_id) const;

    EventBus & Bus() { return bus_; }

    const std::vector<ObjectiveDef> & Objectives() const { return objectives_; }

    // Network sync helpers — called by World::BroadcastTriggerState and the receive handler.
    void SerializeState(Serializer & data) const;
    void ApplySerializedState(Serializer & data);

    bool IsStateDirty() const { return state_dirty_; }
    void ClearDirty()         { state_dirty_ = false; }

private:
    bool EvalConditions(const TriggerNode & node, const GameEvent & ev,
                        const World & world) const;

    struct PendingEvent { Uint16 node_idx; GameEvent event; };

    EventBus bus_;
    ActionSystem actions_;
    std::vector<TriggerNode> nodes_;
    std::vector<ObjectiveDef> objectives_;
    std::vector<TriggerZone> zones_;
    std::vector<PendingEvent> pending_events_;
    std::vector<float> timer_elapsed_; // per-node elapsed seconds (for TIMER_EXPIRED)

    // per-node fired flag (for one_shot)
    std::vector<bool> fired_;

    // per-node fire count (for COUNT_REACHED condition)
    std::unordered_map<Uint16, Uint16> hit_counts_;

    // flags 0-255 (for SET_FLAG / FLAG_SET)
    std::array<bool, 256> flags_{};

    // tracks which (zone_id, player_object_id) pairs are currently inside a zone
    std::set<std::pair<Uint16, Uint16>> zone_occupants_;

    bool state_dirty_ = false;
};

#endif
