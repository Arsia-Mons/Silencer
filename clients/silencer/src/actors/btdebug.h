#pragma once
/**
 * btdebug.h — Live blackboard WebSocket broadcaster (debug-only, port 9339)
 *
 * Provides a single-file TCP/WS server that pushes each NPC's behavior-tree
 * blackboard as JSON after every tick.  The BT editor connects via
 * ws://localhost:9339 and displays it in a live panel.
 *
 * Usage:
 *   // After each bt_->tick(btctx_):
 *   BTDebug::broadcast("guard", id, btctx_.blackboard);
 *
 * The server is lazily initialised on first broadcast call and is a no-op
 * when no WebSocket client is connected, so it is always compiled in.
 */
#include <string>
#include <unordered_map>
#include "json.hpp"

using json = nlohmann::json;

struct BTDebug {
    // Broadcast the blackboard for one NPC.  Lazily opens port 9339 on first
    // call.  Safe to call every tick; cheap when no client is connected.
    static void broadcast(const std::string& actorType,
                          unsigned           actorId,
                          const std::unordered_map<std::string, json>& blackboard);

    // Close the listen socket and all client connections (call on shutdown).
    static void shutdown();
};
