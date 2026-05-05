#ifndef TRIGGER_DEF_H
#define TRIGGER_DEF_H

#include "shared.h"
#include <vector>
#include <string>

// ── Event types ──────────────────────────────────────────────────────────────
enum class EventType : Uint8 {
    NONE = 0,
    TRIGGER_ENTER_ZONE,
    TERMINAL_ACTIVATED,
    ACTOR_KILLED,
    ACTOR_DAMAGED,
    OBJECTIVE_COMPLETE,
    ITEM_COLLECTED,
    PLAYER_DIED,
    ALL_PLAYERS_DIED,
    TIMER_EXPIRED,
    GAME_START,
};

// ── Action types ─────────────────────────────────────────────────────────────
enum class ActionType : Uint8 {
    NONE = 0,
    OPEN_DOOR,
    LOCK_DOOR,
    UNLOCK_DOOR,
    PLAY_SOUND,
    SHOW_OBJECTIVE,
    PAN_CAMERA,
    SPAWN_ACTOR,
    END_MISSION,
    DESTROY_ACTOR,
    MOVE_ACTOR,
    APPLY_DAMAGE_IN_ZONE,
    ENABLE_TRIGGER,
    DISABLE_TRIGGER,
};

// ── Condition types ───────────────────────────────────────────────────────────
enum class ConditionType : Uint8 {
    NONE = 0,
    TEAM_CHECK,
    OBJECTIVE_STATE,
    PLAYER_COUNT,
    HEALTH_THRESHOLD,
};

enum class ConditionLogic : Uint8 { ALL_OF = 0, ANY_OF = 1 };

// ── Data structs (serialized into .sil trigger section) ───────────────────────

struct TriggerCondition {
    ConditionType type  = ConditionType::NONE;
    Uint8 team          = 0;    // TEAM_CHECK: team id; unused otherwise
    Uint16 objective_id = 0;    // OBJECTIVE_STATE
    Uint8 player_count  = 0;    // PLAYER_COUNT threshold
    Uint8 health_pct    = 0;    // HEALTH_THRESHOLD 0-100
};

struct TriggerAction {
    ActionType type     = ActionType::NONE;
    Uint16 actor_id     = 0;    // target actor (door id, spawn id, etc.)
    float delay         = 0.f;  // seconds before firing (0 = immediate)
    Sint16 param_x      = 0;    // MOVE_ACTOR: target X; PAN_CAMERA: X; SPAWN_ACTOR: spawn X
    Sint16 param_y      = 0;    // MOVE_ACTOR: target Y; PAN_CAMERA: Y; SPAWN_ACTOR: spawn Y
    float param_f       = 0.f;  // MOVE_ACTOR: duration secs; APPLY_DAMAGE_IN_ZONE: radius/damage
    Uint8 param_u8      = 0;    // SPAWN_ACTOR: actor type; APPLY_DAMAGE_IN_ZONE: damage amount
    char sound[64]      = {};   // PLAY_SOUND: sound filename
    char message[128]   = {};   // SHOW_OBJECTIVE: text
};

struct TriggerNode {
    Uint16 id                         = 0;
    EventType trigger_event           = EventType::NONE;
    Uint16 actor_id                   = 0;   // 0 = any actor fires this event
    bool one_shot                     = true;
    bool enabled                      = true;
    ConditionLogic condition_logic    = ConditionLogic::ALL_OF;
    float timer_seconds               = 0.f; // > 0 for TIMER_EXPIRED nodes
    std::vector<TriggerCondition> conditions;
    std::vector<TriggerAction> actions;
};

// ── Objective record ──────────────────────────────────────────────────────────
struct ObjectiveDef {
    Uint16 id        = 0;
    bool required    = true;
    bool complete    = false;
    char text[128]   = {};
};

#endif
