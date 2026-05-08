#include "ActionSystem.h"
#include "world.h"
#include "object.h"
#include "objecttypes.h"
#include "basedoor.h"
#include "hittable.h"
#include "guard.h"
#include "robot.h"
#include "TriggerGraph.h"
#include <cstring>
#include <cstdio>

void ActionSystem::Schedule(const TriggerAction & action) {
    pending_.push_back({action, action.delay});
}

void ActionSystem::Tick(World & world, float dt) {
    std::vector<PendingAction> still_pending;
    for (PendingAction & pa : pending_) {
        pa.remaining -= dt;
        if (pa.remaining <= 0.f) {
            Execute(pa.action, world);
        } else {
            still_pending.push_back(pa);
        }
    }
    pending_.swap(still_pending);
}

void ActionSystem::Clear() {
    pending_.clear();
}

void ActionSystem::Execute(const TriggerAction & action, World & world) {
    switch (action.type) {
        case ActionType::OPEN_DOOR: {
            Object * obj = world.GetObjectFromId(action.actor_id);
            if (obj && obj->type == ObjectTypes::BASEDOOR) {
                BaseDoor * door = static_cast<BaseDoor *>(obj);
                if (!door->locked) door->Respawn();
            }
            break;
        }
        case ActionType::LOCK_DOOR: {
            Object * obj = world.GetObjectFromId(action.actor_id);
            if (obj && obj->type == ObjectTypes::BASEDOOR) {
                static_cast<BaseDoor *>(obj)->locked = true;
            }
            break;
        }
        case ActionType::UNLOCK_DOOR: {
            Object * obj = world.GetObjectFromId(action.actor_id);
            if (obj && obj->type == ObjectTypes::BASEDOOR) {
                static_cast<BaseDoor *>(obj)->locked = false;
            }
            break;
        }
        case ActionType::PLAY_SOUND: {
            if (action.sound[0]) {
                world.SendSound(action.sound);
            }
            break;
        }
        case ActionType::SHOW_OBJECTIVE: {
            if (action.message[0]) {
                world.ShowMessage(action.message, 255, 0, true);
            }
            break;
        }
        case ActionType::PAN_CAMERA: {
            world.pancameraactive = true;
            world.pancamerax = action.param_x;
            world.pancameray = action.param_y;
            world.BroadcastCamera(action.param_x, action.param_y);
            break;
        }
        case ActionType::SPAWN_ACTOR: {
            Object * obj = world.CreateObject(action.param_u8);
            if (obj) {
                obj->x = action.param_x;
                obj->y = action.param_y;
                if (obj->type == ObjectTypes::GUARD) {
                    Guard * g = static_cast<Guard *>(obj);
                    g->weapon    = action.actor_id & 0xFF; // 0=blaster, 1=laser, 2=rocket
                    g->patrol    = action.param_f != 0.f;
                    g->originalx = action.param_x;
                    g->originaly = action.param_y;
                } else if (obj->type == ObjectTypes::ROBOT) {
                    Robot * r = static_cast<Robot *>(obj);
                    r->originalx = action.param_x;
                    r->originaly = action.param_y;
                }
            }
            break;
        }
        case ActionType::END_MISSION: {
            // param_u8: 0=neutral, 1=win, 2=lose
            world.quitstate = (action.param_u8 == 0) ? 1 : action.param_u8;
            break;
        }
        case ActionType::DESTROY_ACTOR: {
            world.MarkDestroyObject(action.actor_id);
            break;
        }
        case ActionType::MOVE_ACTOR: {
            Object * obj = world.GetObjectFromId(action.actor_id);
            if (obj) {
                obj->move_target_x  = action.param_x;
                obj->move_target_y  = action.param_y;
                obj->move_duration  = action.param_f;
                obj->move_elapsed   = 0.f;
                obj->move_origin_x  = obj->x;
                obj->move_origin_y  = obj->y;
                obj->is_moving      = true;
            }
            break;
        }
        case ActionType::APPLY_DAMAGE_IN_ZONE: {
            int cx = action.param_x, cy = action.param_y;
            int r  = static_cast<int>(action.param_f);
            std::vector<Uint8> types;
            for (Uint8 t = 0; t < ObjectTypes::MAX_OBJECT_TYPE; t++) types.push_back(t);
            std::vector<Object *> hits = world.TestAABB(cx - r, cy - r, cx + r, cy + r, types);
            for (Object * hit : hits) {
                if (hit->ishittable) {
                    static_cast<Hittable *>(hit)->ApplyDamage(action.param_u8);
                }
            }
            break;
        }
        case ActionType::ENABLE_TRIGGER: {
            world.triggerGraph.SetEnabled(action.actor_id, true);
            break;
        }
        case ActionType::DISABLE_TRIGGER: {
            world.triggerGraph.SetEnabled(action.actor_id, false);
            break;
        }
        case ActionType::COMPLETE_OBJECTIVE: {
            world.triggerGraph.CompleteObjective(action.actor_id, world);
            break;
        }
        case ActionType::LOCK_INPUT: {
            world.input_locked = true;
            break;
        }
        case ActionType::UNLOCK_INPUT: {
            world.input_locked = false;
            // Release cinematic camera pan so main camera returns to following the player.
            world.pancameraactive = false;
            world.BroadcastCamera(0, 0);
            break;
        }
        case ActionType::SET_FLAG: {
            world.triggerGraph.SetFlag(action.param_u8, action.param_f != 0.f);
            break;
        }
        default:
            break;
    }
}
