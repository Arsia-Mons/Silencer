# src/events

Runtime trigger-and-event system. Drives scripted level behaviour: doors opening, objectives completing, boss encounters, cutscene camera pans, etc. Only the **authority peer** evaluates triggers and schedules actions.

## Files

| File | Purpose |
|---|---|
| `TriggerDef.h` | All enums and plain-data structs. No logic, no includes beyond `shared.h`. |
| `EventBus.h/cpp` | Single-frame in-process event queue. |
| `ActionSystem.h/cpp` | Delayed action scheduler. Consumes `TriggerAction`s, calls `World`. |
| `TriggerGraph.h/cpp` | Runtime graph. Owns `EventBus` + `ActionSystem`. Loaded from the `.sil` level file. |

## Data types (`TriggerDef.h`)

```
EventType   — what happened (ACTOR_KILLED, TERMINAL_ACTIVATED, TIMER_EXPIRED, …)
ActionType  — what to do   (OPEN_DOOR, SPAWN_ACTOR, END_MISSION, SET_FLAG, …)
ConditionType — guard check (TEAM_CHECK, OBJECTIVE_STATE, COUNT_REACHED, FLAG_SET, …)

TriggerNode     — id, trigger event, conditions[], actions[], one_shot flag, timer
TriggerCondition / TriggerAction — serialized into the .sil trigger section
ObjectiveDef    — id, required flag, completion text (128 bytes)
TriggerZone     — axis-aligned rect; overlap emits TRIGGER_ENTER_ZONE
```

## EventBus

```cpp
bus.Subscribe(EventType::ACTOR_KILLED, [](const GameEvent& e) { … });
bus.Emit({ .type = EventType::ACTOR_KILLED, .actor_id = id });
bus.Flush();   // deliver queue → subscribers, then clear — call once per frame
bus.Clear();   // reset subscriptions + queue (level unload)
```

- Emit during `Tick()`; Flush once at end of frame.
- Handlers may not call `Emit` (no re-entrant delivery).

## ActionSystem

```cpp
actions.Schedule(action);          // enqueue with action.delay seconds
actions.Tick(world, dt);           // decrement timers, fire ready actions
actions.Clear();
```

## TriggerGraph (owned by `World`)

```cpp
graph.Load(nodes, objectives);     // called by WorldObjectRegistry on level load
graph.LoadZones(zones);
graph.Tick(world, dt);             // authority-only; evaluates conditions, fires actions

graph.CompleteObjective(id, world);
graph.IsObjectiveComplete(id);
graph.SetEnabled(node_id, enabled);
graph.SetFlag(flag_id, value);     // flags 0-255, used by SET_FLAG / FLAG_SET
graph.Bus();                       // access EventBus for manual Emit

// Network sync (authority → peers):
graph.SerializeState(data);
graph.ApplySerializedState(data);
graph.IsStateDirty() / ClearDirty();
```

## Rules

- `TriggerDef.h` must stay pure data — no forward declarations of game classes.
- Only the authority peer calls `TriggerGraph::Tick`. Peers receive state via `ApplySerializedState`.
- Objective completion is replicated through the normal snapshot mechanism, not a separate RPC.
- Add new `EventType`/`ActionType` values to `TriggerDef.h`; wire them up in `TriggerGraph::Tick` / `ActionSystem::Execute`.
