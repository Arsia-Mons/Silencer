#ifndef MAGISTRATE_H
#define MAGISTRATE_H

#include "shared.h"
#include "object.h"
#include "behaviortree.h"

class Magistrate : public Object
{
public:
	Magistrate();
	void Serialize(bool write, Serializer & data, Serializer * old = 0);
	void Tick(World & world);
	void HandleHit(World & world, Uint8 x, Uint8 y, Object & projectile);

	// Per-instance properties — editable by map designers via the level editor.
	Uint32 activationTicks;   // world ticks after game start before activating (default 7200 = 2 min @ 60fps)
	Uint8  secretTriggerN;    // also activate when this many secrets have been beamed (0 = disabled)
	// Up to 4 death-spawn entries packed into a Uint32 (one byte each).
	// Each byte: high nibble = type (0=guard-blaster,1=laser,2=rocket,3=robot), low nibble = count (1-15; 0=unused slot).
	Uint32 deathSpawnEntries; // default: 0x03 = 3 blaster guards in slot 0
	Uint8  deathSpawnRadius;  // spread radius in pixels (default 64)

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

	bool spawnSoundFired_  = false;
	bool deathSoundFired_  = false;
	bool deathActorsFired_ = false;
};

#endif
