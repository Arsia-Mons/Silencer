#ifndef ACTION_SYSTEM_H
#define ACTION_SYSTEM_H

#include "TriggerDef.h"
#include <vector>

class World;

// Executes TriggerActions, respecting per-action delay.
class ActionSystem {
public:
    void Schedule(const TriggerAction & action);
    void Tick(World & world, float dt);
    void Clear();

private:
    struct PendingAction {
        TriggerAction action;
        float remaining; // seconds until fire
    };
    std::vector<PendingAction> pending_;

    void Execute(const TriggerAction & action, World & world);
};

#endif
