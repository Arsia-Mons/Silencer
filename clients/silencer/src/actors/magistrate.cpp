#include "magistrate.h"
#include "objecttypes.h"
#include "guard.h"
#include "robot.h"
#include "gasloader.h"
#include "world.h"
#include "hittable.h"
#include "bipedal.h"

Magistrate::Magistrate() : Object(ObjectTypes::MAGISTRATE){
	requiresauthority = true;
	state    = DORMANT;
	state_i  = 0;
	draw     = false;
	collidable = false;

	const EnemyDef* m = GASLoader::Get().GetEnemyDef("magistrate");
	speed       = m ? m->speed  : 3;
	maxhealth   = m ? m->health : 100;
	health      = maxhealth;
	maxshield   = m ? m->shield : 0;
	shield      = maxshield;

	res_bank    = 207;
	res_index   = 0;
	renderpass  = 2;
	ishittable  = true;
	isbipedal   = true;
	isphysical  = true;

	// Per-instance defaults (can be overridden via Serialize when loading from level)
	activationTicks  = 7200; // 2 minutes at 60fps
	secretTriggerN   = 2;    // also activate on 2nd secret beamed
	deathSpawnCount  = 3;
	deathSpawnType   = 0;    // 0 = guard
	deathSpawnRadius = 64;

	originalx       = 0;
	originaly       = 0;
	originalmirrored = false;
}

void Magistrate::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state,          old);
	data.Serialize(write, state_i,        old);
	data.Serialize(write, activationTicks, old);
	data.Serialize(write, secretTriggerN,  old);
	data.Serialize(write, deathSpawnCount, old);
	data.Serialize(write, deathSpawnType,  old);
	data.Serialize(write, deathSpawnRadius,old);
	data.Serialize(write, originalx,       old);
	data.Serialize(write, originaly,       old);
	data.Serialize(write, originalmirrored,old);
}

void Magistrate::SpawnDeathActors(World & world){
	if(!world.IsAuthority()) return;

	for(int i = 0; i < deathSpawnCount; i++){
		Sint16 ox = x + (Sint16)((world.Random() % (deathSpawnRadius * 2 + 1)) - deathSpawnRadius);
		Sint16 oy = y;

		Object * obj = nullptr;
		if(deathSpawnType == 1){
			obj = world.CreateObject(ObjectTypes::ROBOT);
		}else{
			obj = world.CreateObject(ObjectTypes::GUARD);
		}
		if(!obj) continue;

		obj->x = ox;
		obj->y = oy;
		obj->mirrored = (world.Random() % 2) == 0;

		if(deathSpawnType == 0){
			Guard * g = static_cast<Guard *>(obj);
			g->patrol       = true;
			g->originalx    = ox;
			g->originaly    = oy;
			g->originalmirrored = obj->mirrored;
		}
	}
}

void Magistrate::Tick(World & world){
	Hittable::Tick(*this, world);
	Bipedal::Tick(*this, world);

	const EnemyDef* m = GASLoader::Get().GetEnemyDef("magistrate");

	switch(state){
		case DORMANT:{
			bool timerFired  = world.tickcount >= activationTicks;
			bool secretFired = secretTriggerN > 0 && world.secretsBeamed >= secretTriggerN;
			if(timerFired || secretFired){
				draw      = true;
				collidable = true;
				state     = NEW;
				state_i   = -1;
				if(m && !m->soundActivate.empty()){
					EmitSound(world, world.resources.soundbank[m->soundActivate], 128);
				}
			}
		}break;

		case NEW:{
			currentplatformid = 0;
			if(FindCurrentPlatform(*this, world)){
				state   = STANDING;
				state_i = -1;
			}else{
				// Fall until landing
				yv += world.gravity;
				if(yv > world.maxyvelocity) yv = world.maxyvelocity;
				int xe = x, ye = y + yv;
				Platform * pl = world.map.TestLine(x, y, xe, ye, &xe, &ye,
					Platform::RECTANGLE | Platform::STAIRSUP | Platform::STAIRSDOWN);
				if(pl){
					currentplatformid = pl->id;
					state   = STANDING;
					state_i = -1;
				}
				x = xe;
				y = ye;
			}
			res_bank  = 207;
			res_index = 0;
		}break;

		case STANDING:{
			yv = 0;
			xv = 0;
			res_bank  = 207;
			res_index = 0;
			// Stand briefly then start walking in the current direction
			if(state_i >= 60){
				mirrored = !mirrored;
				state   = WALKING;
				state_i = -1;
			}
		}break;

		case WALKING:{
			xv = mirrored ? -(Sint8)speed : (Sint8)speed;
			res_bank  = 207;
			res_index = state_i % 21;
			bool atEnd = DistanceToEnd(*this, world) <= world.minwalldistance;
			if(atEnd){
				// Hit a wall — turn around and pause briefly
				state   = STANDING;
				state_i = -1;
			}else{
				FollowGround(*this, world, xv);
			}
		}break;

		case DYING:{
			collidable = false;
			if(state_i == 0){
				if(m && !m->soundDeath.empty()){
					EmitSound(world, world.resources.soundbank[m->soundDeath], 128);
				}
				SpawnDeathActors(world);
			}
			res_bank  = 207;
			res_index = 0;
			if(state_i >= 10){
				draw    = false;
				state   = DEAD;
				state_i = -1;
			}
		}break;

		case DEAD:{
			draw      = false;
			collidable = false;
		}break;
	}

	state_i++;
}

void Magistrate::HandleHit(World & world, Uint8 hitx, Uint8 hity, Object & projectile){
	Hittable::HandleHit(*this, world, hitx, hity, projectile);
	if(health == 0 && state != DYING && state != DEAD){
		state   = DYING;
		state_i = -1;
	}
}
