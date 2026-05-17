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

	activationTicks  = 7200; // 2 minutes at 60fps
	secretTriggerN   = 2;
	deathSpawnCount  = 3;
	deathSpawnType   = 0;
	deathSpawnRadius = 64;

	originalx        = 0;
	originaly        = 0;
	originalmirrored = false;
	bt_  = nullptr;
}

void Magistrate::InitBT(){
	const EnemyDef* m = GASLoader::Get().GetEnemyDef("magistrate");
	std::string treeId = (m && !m->behaviorTree.empty()) ? m->behaviorTree : "magistrate";
	bt_ = BehaviorTreeLibrary::instance().get(treeId);
	if(!bt_) return;

	btctx_.actions["Patrol"] = [this](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		if(state == STANDING){
			state   = WALKING;
			state_i = 0;
		} else if(state == WALKING){
			if(DistanceToEnd(*this, world) <= world.minwalldistance){
				mirrored = !mirrored;
				state    = STANDING;
				state_i  = 0;
			}
		}
		return BTResult::Running;
	};

	// Generic data-driven leaves (shared with civilian/guard)
	btctx_.actions["SetBlackboard"] = [](BTContext& ctx) -> BTResult {
		if(!ctx.props || !ctx.props->contains("key") || !ctx.props->contains("value"))
			return BTResult::Failure;
		ctx.bbSet(ctx.props->value("key", std::string{}), (*ctx.props)["value"]);
		return BTResult::Success;
	};
	btctx_.actions["RandomChance"] = [](BTContext& ctx) -> BTResult {
		float chance = ctx.props ? ctx.props->value("chance", 0.5f) : 0.5f;
		return ((float)rand() / (float)RAND_MAX) < chance ? BTResult::Success : BTResult::Failure;
	};
	btctx_.actions["PlayAnim"] = [this](BTContext& ctx) -> BTResult {
		if(!ctx.props) return BTResult::Failure;
		int  bank  = ctx.props->value("bank",   0);
		int  frames= ctx.props->value("frames", 1);
		bool loop  = ctx.props->value("loop",   true);
		res_bank  = bank;
		res_index = loop ? (state_i % std::max(frames, 1))
		                 : std::min((int)state_i, std::max(frames - 1, 0));
		if(!loop && state_i >= frames) return BTResult::Success;
		return BTResult::Running;
	};
	btctx_.actions["EmitSound"] = [this](BTContext& ctx) -> BTResult {
		if(!ctx.props) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		std::string snd = ctx.props->value("sound", std::string{});
		int vol = ctx.props->value("volume", 100);
		if(snd.empty()) return BTResult::Failure;
		EmitSound(world, world.resources.soundbank[snd], vol);
		return BTResult::Success;
	};
	btctx_.actions["SetFacing"] = [this](BTContext& ctx) -> BTResult {
		std::string dir = ctx.props ? ctx.props->value("dir", std::string{"flip"}) : "flip";
		if     (dir == "left")  mirrored = true;
		else if(dir == "right") mirrored = false;
		else                    mirrored = !mirrored;
		return BTResult::Success;
	};
	btctx_.actions["SetSpeed"] = [this](BTContext& ctx) -> BTResult {
		if(!ctx.props) return BTResult::Failure;
		int spd = ctx.props->value("speed", -1);
		if(spd < 0) return BTResult::Failure;
		speed = (Uint8)spd;
		return BTResult::Success;
	};
}

void Magistrate::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state,           old);
	data.Serialize(write, state_i,         old);
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

		obj->x        = ox;
		obj->y        = oy;
		obj->mirrored = (world.Random() % 2) == 0;

		if(deathSpawnType == 0){
			Guard* g = static_cast<Guard*>(obj);
			g->patrol           = true;
			g->originalx        = ox;
			g->originaly        = oy;
			g->originalmirrored = obj->mirrored;
		}
	}
}

void Magistrate::Tick(World & world){
	Hittable::Tick(*this, world);
	Bipedal::Tick(*this, world);

	const EnemyDef* m = GASLoader::Get().GetEnemyDef("magistrate");

	// DORMANT: wait for timer or secret trigger; skip BT entirely
	if(state == DORMANT){
		bool timerFired  = world.tickcount >= activationTicks;
		bool secretFired = secretTriggerN > 0 && world.secretsBeamed >= (int)secretTriggerN;
		if(timerFired || secretFired){
			draw       = true;
			collidable = true;
			state      = NEW;
			state_i    = -1;
			if(m && !m->soundActivate.empty()){
				EmitSound(world, world.resources.soundbank[m->soundActivate], 128);
			}
		}
		state_i++;
		return;
	}

	// DYING / DEAD: handle death sequence; skip BT
	if(state == DYING){
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
		state_i++;
		return;
	}
	if(state == DEAD){
		draw       = false;
		collidable = false;
		state_i++;
		return;
	}

	// NEW: land on a platform then hand off to BT
	if(state == NEW){
		currentplatformid = 0;
		if(FindCurrentPlatform(*this, world)){
			state   = STANDING;
			state_i = -1;
		}else{
			yv += world.gravity;
			if(yv > world.maxyvelocity) yv = world.maxyvelocity;
			int xe = x, ye = y + yv;
			Platform* pl = world.map.TestLine(x, y, xe, ye, &xe, &ye,
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
		state_i++;
		return;
	}

	// Init BT on first active tick
	if(!bt_) InitBT();

	// Tick the BT
	if(bt_){
		btctx_.userData = &world;
		bt_->tick(btctx_);
	}

	// State machine: animation + physics (BT sets state transitions via leaf actions)
	switch(state){
		case STANDING:{
			yv       = 0;
			xv       = 0;
			res_bank  = 207;
			res_index = 0;
		}break;

		case WALKING:{
			xv        = mirrored ? -(Sint8)speed : (Sint8)speed;
			res_bank  = 207;
			res_index = state_i % 21;
			FollowGround(*this, world, xv);
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


