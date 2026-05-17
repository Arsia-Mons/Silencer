#ifndef MAGISTRATE_H
#define MAGISTRATE_H

#include "shared.h"
#include "object.h"

class Magistrate : public Object
{
public:
	Magistrate();
	void Serialize(bool write, Serializer & data, Serializer * old = 0);
	void Tick(World & world);
	void HandleHit(World & world, Uint8 x, Uint8 y, Object & projectile);

	// Per-instance properties — editable by map designers via the level editor.
	Uint32 activationTicks;  // world ticks after game start before activating (default 7200 = 2 min @ 60fps)
	Uint8  secretTriggerN;   // also activate when this many secrets have been beamed (0 = disabled)
	Uint8  deathSpawnCount;  // number of actors to spawn on death (default 3)
	Uint8  deathSpawnType;   // 0 = guard, 1 = robot
	Uint8  deathSpawnRadius; // spread radius in pixels (default 64)

	Sint16 originalx, originaly;
	bool   originalmirrored;

private:
	void SpawnDeathActors(World & world);

	enum { DORMANT, NEW, STANDING, WALKING, DYING, DEAD };
	Uint8  state, state_i;
	Uint16 maxhealth, maxshield;
	Uint8  speed;
};

#endif
