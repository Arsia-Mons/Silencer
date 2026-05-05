#include "TriggerGraph.h"
#include "world.h"
#include "objecttypes.h"

void TriggerGraph::Load(const std::vector<TriggerNode> & nodes,
                        const std::vector<ObjectiveDef> & objectives) {
    nodes_      = nodes;
    objectives_ = objectives;
    fired_.assign(nodes_.size(), false);
    timer_elapsed_.assign(nodes_.size(), 0.f);

    // Subscribe once per event type that any node listens to.
    bus_.Clear();
    for (size_t i = 0; i < nodes_.size(); i++) {
        const TriggerNode & node = nodes_[i];
        bus_.Subscribe(node.trigger_event, [this, i](const GameEvent & ev) {
            pending_events_.push_back({static_cast<Uint16>(i), ev});
        });
    }
}

void TriggerGraph::Clear() {
    nodes_.clear();
    objectives_.clear();
    fired_.clear();
    timer_elapsed_.clear();
    pending_events_.clear();
    bus_.Clear();
    actions_.Clear();
}

void TriggerGraph::Tick(World & world, float dt) {
    if (!world.IsAuthority()) {
        // Replicas only run the action timer (for camera/sound driven by
        // authority-replicated state); trigger evaluation stays on authority.
        actions_.Tick(world, dt);
        return;
    }

    bus_.Flush();

    // Advance and fire TIMER_EXPIRED nodes.
    for (size_t i = 0; i < nodes_.size(); i++) {
        TriggerNode & node = nodes_[i];
        if (node.trigger_event != EventType::TIMER_EXPIRED) continue;
        if (!node.enabled) continue;
        if (node.one_shot && fired_[i]) continue;
        if (node.timer_seconds <= 0.f) continue;

        timer_elapsed_[i] += dt;
        if (timer_elapsed_[i] >= node.timer_seconds) {
            timer_elapsed_[i] = 0.f;
            GameEvent ev;
            ev.type     = EventType::TIMER_EXPIRED;
            ev.actor_id = node.id;
            pending_events_.push_back({static_cast<Uint16>(i), ev});
        }
    }

    for (auto & pe : pending_events_) {
        Uint16 idx        = pe.node_idx;
        const GameEvent & ev = pe.event;
        TriggerNode & node = nodes_[idx];

        if (!node.enabled)            continue;
        if (node.one_shot && fired_[idx]) continue;

        // actor_id filter: 0 means "any actor"
        if (node.actor_id != 0 && node.actor_id != ev.actor_id) continue;

        if (!EvalConditions(node, ev, world)) continue;

        // Fire all actions
        for (const TriggerAction & action : node.actions) {
            actions_.Schedule(action);
        }

        fired_[idx] = true;
        if (!node.one_shot) {
            // repeatable: reset after firing so it can fire again
            fired_[idx] = false;
        }
        state_dirty_ = true;
    }
    pending_events_.clear();

    actions_.Tick(world, dt);

    if (state_dirty_) {
        world.BroadcastTriggerState();
        state_dirty_ = false;
    }
}

void TriggerGraph::CompleteObjective(Uint16 id, World & world) {
    for (ObjectiveDef & obj : objectives_) {
        if (obj.id == id) {
            obj.complete = true;
            state_dirty_ = true;
            GameEvent ev;
            ev.type        = EventType::OBJECTIVE_COMPLETE;
            ev.objective_id = id;
            bus_.Emit(ev);
            break;
        }
    }
}

bool TriggerGraph::IsObjectiveComplete(Uint16 id) const {
    for (const ObjectiveDef & obj : objectives_) {
        if (obj.id == id) return obj.complete;
    }
    return false;
}

void TriggerGraph::SetEnabled(Uint16 node_id, bool enabled) {
    for (TriggerNode & node : nodes_) {
        if (node.id == node_id) {
            node.enabled = enabled;
            state_dirty_ = true;
            return;
        }
    }
}

void TriggerGraph::SerializeState(Serializer & data) const {
    Uint16 num_objectives = static_cast<Uint16>(objectives_.size());
    data.Put(num_objectives);
    for (const ObjectiveDef & obj : objectives_) {
        data.Put(const_cast<Uint16 &>(obj.id));
        Uint8 complete = obj.complete ? 1 : 0;
        data.Put(complete);
    }
    Uint16 num_nodes = static_cast<Uint16>(nodes_.size());
    data.Put(num_nodes);
    for (size_t i = 0; i < nodes_.size(); i++) {
        data.Put(const_cast<Uint16 &>(nodes_[i].id));
        Uint8 flags = 0;
        if (nodes_[i].enabled) flags |= 0x01;
        if (i < fired_.size() && fired_[i]) flags |= 0x02;
        data.Put(flags);
    }
}

void TriggerGraph::ApplySerializedState(Serializer & data) {
    Uint16 num_objectives = 0;
    data.Get(num_objectives);
    for (Uint16 i = 0; i < num_objectives; i++) {
        Uint16 id = 0;
        Uint8 complete = 0;
        data.Get(id);
        data.Get(complete);
        for (ObjectiveDef & obj : objectives_) {
            if (obj.id == id) { obj.complete = (complete != 0); break; }
        }
    }
    Uint16 num_nodes = 0;
    data.Get(num_nodes);
    for (Uint16 i = 0; i < num_nodes; i++) {
        Uint16 id = 0;
        Uint8 flags = 0;
        data.Get(id);
        data.Get(flags);
        for (size_t j = 0; j < nodes_.size(); j++) {
            if (nodes_[j].id == id) {
                nodes_[j].enabled = (flags & 0x01) != 0;
                if (j < fired_.size()) fired_[j] = (flags & 0x02) != 0;
                break;
            }
        }
    }
}

bool TriggerGraph::EvalConditions(const TriggerNode & node, const GameEvent & ev,
                                   const World & world) const {
    if (node.conditions.empty()) return true;

    bool all_pass = true;
    bool any_pass = false;

    for (const TriggerCondition & cond : node.conditions) {
        bool pass = false;
        switch (cond.type) {
            case ConditionType::TEAM_CHECK:
                pass = (ev.team == cond.team);
                break;
            case ConditionType::OBJECTIVE_STATE:
                pass = IsObjectiveComplete(cond.objective_id);
                break;
            case ConditionType::PLAYER_COUNT: {
                // Count active players — scan objectlist directly (public).
                int count = 0;
                for (const Object * obj : world.objectlist) {
                    if (obj->type == ObjectTypes::PLAYER) count++;
                }
                pass = (count >= cond.player_count);
                break;
            }
            case ConditionType::HEALTH_THRESHOLD: {
                // GetObjectFromId is non-const; cast away const for the lookup.
                Object * obj = const_cast<World &>(world).GetObjectFromId(node.actor_id);
                if (obj && obj->ishittable && obj->GetMaxHealth() > 0) {
                    Uint8 pct = static_cast<Uint8>(
                        (static_cast<float>(obj->GetHealth()) / obj->GetMaxHealth()) * 100.f);
                    pass = (pct <= cond.health_pct);
                }
                break;
            }
            default:
                pass = true;
                break;
        }
        if (!pass) all_pass = false;
        if (pass)  any_pass = true;
    }

    return (node.condition_logic == ConditionLogic::ALL_OF) ? all_pass : any_pass;
}
