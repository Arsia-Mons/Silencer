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

	activationTicks  = m ? m->activationTicks : 7200; // 2 minutes at 60fps
	secretTriggerN   = 2;
	deathSpawnEntries = 0x03; // default: 3 blaster guards
	deathSpawnRadius  = 64;

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

	// ── Activation ────────────────────────────────────────────────────────────
	// Waits for timer or secret trigger; transitions DORMANT→NEW.
	// Returns Running while waiting, Success on the tick it activates.
	btctx_.actions["CheckActivation"] = [this](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		bool timerFired  = world.tickcount >= activationTicks;
		bool secretFired = secretTriggerN > 0 && world.secretsBeamed >= (int)secretTriggerN;
		if(timerFired || secretFired){
			draw       = true;
			collidable = true;
			state      = NEW;
			state_i    = 0;
			return BTResult::Success; // lets the next sibling (EmitSpawnSound) run this tick
		}
		return BTResult::Running;
	};

	// Plays the spawn sound once (idempotent via BB flag).
	// Reads sound name from props "sound"; falls back to GAS soundActivate. Always Success.
	btctx_.actions["EmitSpawnSound"] = [this](BTContext& ctx) -> BTResult {
		if(!spawnSoundFired_){
			spawnSoundFired_ = true;
			World& world = *static_cast<World*>(ctx.userData);
			std::string snd = ctx.props ? ctx.props->value("sound", std::string{}) : std::string{};
			if(snd.empty()){
				const EnemyDef* md = GASLoader::Get().GetEnemyDef("magistrate");
				if(md) snd = md->soundActivate;
			}
			if(!snd.empty())
				EmitGlobalSound(world, world.resources.soundbank[snd], 128);
		}
		return BTResult::Success;
	};

	// ── Movement ──────────────────────────────────────────────────────────────
	// Walk in current facing direction. Returns Running while clear, Failure at wall.
	btctx_.actions["Walk"] = [this](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		if(state != WALKING){ state = WALKING; state_i = 0; }
		if(DistanceToEnd(*this, world) <= world.minwalldistance)
			return BTResult::Failure;
		return BTResult::Running;
	};

	// Stand in place for `duration` ticks (default 60). Returns Running then Success.
	btctx_.actions["Stand"] = [this](BTContext& ctx) -> BTResult {
		int duration = ctx.props ? ctx.props->value("duration", 60) : 60;
		if(state != STANDING){ state = STANDING; state_i = 0; }
		if(state_i >= duration) return BTResult::Success;
		return BTResult::Running;
	};

	// Flip facing direction immediately. Always Success.
	btctx_.actions["TurnAround"] = [this](BTContext& ctx) -> BTResult {
		(void)ctx;
		mirrored = !mirrored;
		return BTResult::Success;
	};

	// Legacy composite patrol leaf (kept for backwards-compat with older saved trees).
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

	// ── Death leaves ──────────────────────────────────────────────────────────
	// Play the death sound once. Reads "sound" from props; falls back to GAS soundDeath. Always Success.
	btctx_.actions["EmitDeathSound"] = [this](BTContext& ctx) -> BTResult {
		if(!deathSoundFired_){
			deathSoundFired_ = true;
			World& world = *static_cast<World*>(ctx.userData);
			std::string snd = ctx.props ? ctx.props->value("sound", std::string{}) : std::string{};
			if(snd.empty()){
				const EnemyDef* md = GASLoader::Get().GetEnemyDef("magistrate");
				if(md) snd = md->soundDeath;
			}
			if(!snd.empty())
				EmitGlobalSound(world, world.resources.soundbank[snd], 128);
		}
		return BTResult::Success;
	};

	// Spawn guards/robots at death position. Idempotent; always Success.
	btctx_.actions["SpawnDeathGuards"] = [this](BTContext& ctx) -> BTResult {
		if(!deathActorsFired_){
			deathActorsFired_ = true;
			World& world = *static_cast<World*>(ctx.userData);
			SpawnDeathActors(world);
		}
		return BTResult::Success;
	};

	// Death fade-out. Runs for `duration` ticks then sets state=DEAD and hides.
	// Returns Running while playing, Success when done.
	btctx_.actions["DeathFade"] = [this](BTContext& ctx) -> BTResult {
		int duration = ctx.props ? ctx.props->value("duration", 10) : 10;
		collidable = false;
		res_bank   = 207;
		res_index  = 0;
		if(state_i >= duration){
			draw  = false;
			state = DEAD;
			state_i = 0;
			return BTResult::Success;
		}
		return BTResult::Running;
	};

	// ── Generic data-driven leaves ────────────────────────────────────────────
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
	data.Serialize(write, deathSpawnEntries, old);
	data.Serialize(write, deathSpawnRadius,  old);
	data.Serialize(write, originalx,       old);
	data.Serialize(write, originaly,       old);
	data.Serialize(write, originalmirrored,old);
}

void Magistrate::SpawnDeathActors(World & world){
	if(!world.IsAuthority()) return;

	for(int slot = 0; slot < 4; slot++){
		Uint8 entry = (deathSpawnEntries >> (slot * 8)) & 0xFF;
		int count = entry & 0x0F;
		int type  = (entry >> 4) & 0x0F;
		if(count == 0) continue;

		for(int i = 0; i < count; i++){
			Sint16 ox = x + (Sint16)((world.Random() % (deathSpawnRadius * 2 + 1)) - deathSpawnRadius);

			Object * obj = nullptr;
			if(type == 3){
				obj = world.CreateObject(ObjectTypes::ROBOT);
			}else{
				obj = world.CreateObject(ObjectTypes::GUARD);
			}
			if(!obj) continue;

			obj->x        = ox;
			obj->y        = y;
			obj->mirrored = (world.Random() % 2) == 0;

			if(type <= 2){
				Guard* g = static_cast<Guard*>(obj);
				g->weapon           = (Uint8)type; // 0=blaster, 1=laser, 2=rocket
				g->patrol           = true;
				g->originalx        = ox;
				g->originaly        = y;
				g->originalmirrored = obj->mirrored;
			}
		}
	}
}

void Magistrate::Tick(World & world){
	Hittable::Tick(*this, world);
	Bipedal::Tick(*this, world);

	// DEAD: nothing left to do
	if(state == DEAD){
		draw       = false;
		collidable = false;
		state_i++;
		return;
	}

	// NEW: drop to platform, then transition to STANDING for BT to take over
	if(state == NEW){
		currentplatformid = 0;
		if(FindCurrentPlatform(*this, world)){
			state   = STANDING;
			state_i = -1;
		} else {
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
		if(state == STANDING && !spawnSoundFired_){
			spawnSoundFired_ = true;
			const EnemyDef* md = GASLoader::Get().GetEnemyDef("magistrate");
			std::string snd = md ? md->soundActivate : std::string{};
			if(!snd.empty())
				EmitGlobalSound(world, world.resources.soundbank[snd], 128);
		}
		res_bank  = 207;
		res_index = 0;
		state_i++;
		return;
	}

	// Init BT once (runs for DORMANT, STANDING, WALKING, DYING)
	if(!bt_) InitBT();

	// Tick BT — writes BB keys, then tree drives all behavior
	if(bt_){
		btctx_.userData = &world;
		btctx_.bbSet("is_dormant", state == DORMANT);
		btctx_.bbSet("is_dying",   state == DYING);
		btctx_.bbSet("health_pct", maxhealth > 0 ? (float)health / (float)maxhealth : 0.0f);
		bt_->tick(btctx_);
	}

	// Animation + physics — BT leaves set state/xv; this applies them
	switch(state){
		case DORMANT:
			break;
		case STANDING:
			yv        = 0;
			xv        = 0;
			res_bank  = 207;
			res_index = 0;
			break;
		case WALKING:
			xv        = mirrored ? -(Sint8)speed : (Sint8)speed;
			res_bank  = 207;
			res_index = state_i % 21;
			FollowGround(*this, world, xv);
			{
				auto it = world.resources.actordefs.find("magistrate");
				if(it != world.resources.actordefs.end()){
					const AnimSequence* seq = it->second.GetSequence("WALKING");
					std::string snd; int vol;
					if(seq && seq->GetFrameSoundByIndex(res_index, snd, vol))
						EmitSound(world, world.resources.soundbank[snd], vol);
				}
			}
			break;
		case DYING:
			collidable = false;
			res_bank   = 207;
			res_index  = 0;
			break;
		default:
			break;
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


