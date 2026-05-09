#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include "TriggerDef.h"
#include <functional>
#include <unordered_map>
#include <vector>

// Payload carried with every emitted event.
struct GameEvent {
    EventType type      = EventType::NONE;
    Uint16 actor_id     = 0;   // source actor (terminal id, killed actor id, etc.)
    Uint16 player_id    = 0;   // player involved (hacker, collector, etc.)
    Uint8  team         = 0;
    Uint16 objective_id = 0;   // for OBJECTIVE_COMPLETE
    Sint32 value        = 0;   // damage amount, timer id, etc.
};

// Simple single-frame event queue.
// Emit events during Tick(); call Flush() once per frame to deliver them.
class EventBus {
public:
    using Handler = std::function<void(const GameEvent &)>;

    void Subscribe(EventType type, Handler handler);
    void Emit(const GameEvent & event);

    // Deliver all queued events to subscribers, then clear the queue.
    void Flush();

    void Clear();

private:
    std::vector<GameEvent> queue_;
    std::unordered_map<Uint8, std::vector<Handler>> subscribers_;
};

#endif
