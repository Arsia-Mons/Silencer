#include "EventBus.h"

void EventBus::Subscribe(EventType type, Handler handler) {
    subscribers_[static_cast<Uint8>(type)].push_back(std::move(handler));
}

void EventBus::Emit(const GameEvent & event) {
    queue_.push_back(event);
}

void EventBus::Flush() {
    // Iterate by index — handlers may emit further events; those get queued
    // for the next Flush() call (we only process events present at start).
    std::vector<GameEvent> current;
    current.swap(queue_);
    for (const GameEvent & ev : current) {
        auto it = subscribers_.find(static_cast<Uint8>(ev.type));
        if (it != subscribers_.end()) {
            for (auto & handler : it->second) {
                handler(ev);
            }
        }
    }
}

void EventBus::Clear() {
    queue_.clear();
}
