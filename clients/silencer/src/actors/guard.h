#ifndef GUARD_H
#define GUARD_H

#include "shared.h"
#include "object.h"
#include "behaviortree.h"

class Guard : public Object
{
public:
	Guard();
	void Serialize(bool write, Serializer & data, Serializer * old = 0);
	void Tick(World & world);
	void HandleHit(World & world, Uint8 x, Uint8 y, Object & projectile);
	Uint8 weapon;
	bool patrol;
	Sint16 originalx;
	Sint16 originaly;
	bool originalmirrored;
	Uint8 cooldowntime;
	
private:
	Object * Look(World & world, Uint8 direction);
	void Fire(World & world, Uint8 direction);
	bool CooledDown(World & world);
	bool ShouldTarget(Object & object, World & world);
	void InitBT();
	enum {NEW, STANDING, CROUCHING, CROUCHED, SHOOTCROUCHED, UNCROUCHING, LOOKING, WALKING, SHOOTSTANDING,
		SHOOTUP, SHOOTDOWN, SHOOTUPANGLE, SHOOTDOWNANGLE, SHOOTLADDERUP, SHOOTLADDERDOWN, LADDER, HIT, DYING,
		DYINGEXPLODE, DEAD};
	Uint8 state;
	Uint8 state_i;
	Uint8 speed;
	Uint16 chasing;
	Uint16 maxshield; // Hittable tracks maxhealth; guard tracks maxshield separately for respawn
	Uint8 respawnseconds;
	Uint32 lastspoke;
	Uint32 lastshot;
	const BehaviorTree* bt_;
	BTContext btctx_;
	int bt_walk_ticks_ = 0;      // non-serialized alert/search timer for BT
	int bt_ladder_cooldown_ = 0; // ticks to wait before attempting another ladder climb

	// Activity flags — synced from state each tick; BT leaves will write these directly
	// once the state machine is removed in Phase 9.
	bool is_walking     = false;
	bool is_crouching   = false; // true during CROUCHING or CROUCHED
	bool is_crouched    = false; // true only when fully crouched (CROUCHED)
	bool is_shooting    = false;
	bool is_on_ladder   = false;
	bool is_hit         = false;
	bool is_dying       = false;
	bool is_dead        = false;
	uint8_t shoot_direction = 0; // 0=standing,1=up,2=down,3=up-angle,4=down-angle,5=ladder-up,6=ladder-down
};

#endif
