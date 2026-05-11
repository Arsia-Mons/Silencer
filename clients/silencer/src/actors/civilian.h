#ifndef CIVILIAN_H
#define CIVILIAN_H

#include "shared.h"
#include "object.h"
#include "behaviortree.h"

class Civilian : public Object
{
public:
	Civilian();
	void Serialize(bool write, Serializer & data, Serializer * old = 0);
	void Tick(World & world);
	void HandleHit(World & world, Uint8 x, Uint8 y, Object & projectile);
	bool AddTract(Uint16 teamid);
	Uint8 suitcolor;
	Uint8 speed;
	Uint16 tractteamid;
	static const Uint8 defaultsuitcolor = (7 << 4) + 11;

private:
	bool Look(World & world);
	bool CheckTractVictim(World & world);
	void InitBT();
	enum {NEW, STANDING, WALKING, RUNNING, DYINGFORWARD, DYINGBACKWARD, DYINGEXPLODE, DEAD};
	Uint8 state;
	Uint8 state_i;
	const BehaviorTree* bt_;
	BTContext btctx_;

	// Activity flags — synced from state each tick.
	bool is_walking  = false;
	bool is_running  = false;
	bool is_dying    = false;
	bool is_dead     = false;
};

#endif