#ifndef VANTA_H
#define VANTA_H

#include "shared.h"
#include "object.h"
#include "behaviortree.h"

class Vanta : public Object
{
public:
	Vanta();
	void Serialize(bool write, Serializer & data, Serializer * old = 0);
	void Tick(World & world);
	void HandleHit(World & world, Uint8 x, Uint8 y, Object & projectile);

	Uint32 activationTicks;
	Uint8  secretTriggerN;
	Uint32 deathSpawnEntries;
	Uint8  deathSpawnRadius;

	Sint16 originalx, originaly;
	bool   originalmirrored;

private:
	void InitBT();
	void SpawnDeathActors(World & world);

	enum { DORMANT, NEW, STANDING, WALKING, DYING, DEAD };
	Uint8  state, state_i;
	Uint16 maxhealth, maxshield;
	Uint8  speed;

	const BehaviorTree* bt_;
	BTContext            btctx_;

	bool spawnSoundFired_   = false;
	bool deathSoundFired_   = false;
	bool deathActorsFired_  = false;
	bool prevDraw_          = false;
	bool healthAlive_       = false;
};

#endif
