#include "civilian.h"
#include "projectile.h"
#include "bodypart.h"
#include "player.h"
#include "plasmaprojectile.h"
#include "gasloader.h"
#include "gasloader.h"

Civilian::Civilian() : Object(ObjectTypes::CIVILIAN){
	requiresauthority = true;
	state = NEW;
	state_i = 0;
	const EnemyDef* c = GASLoader::Get().GetEnemyDef("civilian");
	speed = c ? c->speed : 4;
	res_bank = 121;
	res_index = 0;
	suitcolor = defaultsuitcolor;
	renderpass = 2;
	ishittable = true;
	isbipedal = true;
	isphysical = true;
	{ const EnemyDef* _cd = GASLoader::Get().GetEnemyDef("civilian");
	  snapshotinterval = _cd ? _cd->snapshotInterval : 72; }
	tractteamid = 0;
	bt_ = nullptr;
}

void Civilian::InitBT(){
	bt_ = BehaviorTreeLibrary::instance().get("civilian");
	if(!bt_) return;

	// ThreatLook: scan AABB for projectiles; orient mirrored away from threat. Success = threat found.
	btctx_.actions["ThreatLook"] = [this](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		std::vector<Uint8> types = {
			ObjectTypes::BLASTERPROJECTILE, ObjectTypes::LASERPROJECTILE,
			ObjectTypes::ROCKETPROJECTILE,  ObjectTypes::FLAMERPROJECTILE,
			ObjectTypes::PLASMAPROJECTILE,  ObjectTypes::WALLPROJECTILE,
			ObjectTypes::FLAREPROJECTILE
		};
		const EnemyDef* cd = GASLoader::Get().GetEnemyDef("civilian");
		int dx = cd ? cd->threatDetectX : 200;
		int dy = cd ? cd->threatDetectY : 100;
		std::vector<Object*> objects = world.TestAABB(x - dx, y - dy, x + dx, y + dy, types);
		if (objects.empty()) return BTResult::Failure;
		mirrored = (objects[0]->x > x);
		return BTResult::Success;
	};

	// Run: enter RUNNING state.
	btctx_.actions["Run"] = [this](BTContext&) -> BTResult {
		if(state != RUNNING){ state = RUNNING; state_i = -1; }
		return BTResult::Success;
	};

	// ReturnToWalk: after runDurationTicks in RUNNING with no threat, return to WALKING.
	btctx_.actions["ReturnToWalk"] = [this](BTContext&) -> BTResult {
		if(state != RUNNING) return BTResult::Failure;
		const EnemyDef* cd = GASLoader::Get().GetEnemyDef("civilian");
		if(state_i < (cd ? cd->runDurationTicks : 150)) return BTResult::Failure;
		state = WALKING; state_i = -1;
		return BTResult::Success;
	};

	// Wander: civilian keeps walking — nothing to do.
	btctx_.actions["Wander"] = [](BTContext&) -> BTResult {
		return BTResult::Success;
	};
}

void Civilian::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state, old);
	data.Serialize(write, state_i, old);
	data.Serialize(write, tractteamid, old);
}

void Civilian::Tick(World & world){
	Hittable::Tick(*this, world);
	Bipedal::Tick(*this, world);
	InitBT();

	// NEW: find starting platform
	if(state == NEW){
		if(FindCurrentPlatform(*this, world)){
			state = WALKING;
			state_i = -1;
		}
		state_i++; return;
	}

	// DYINGFORWARD: forward death animation → DEAD
	if(state == DYINGFORWARD){
		tractteamid = 0;
		if(state_i == 0){
			const EnemyDef* gd = GASLoader::Get().GetEnemyDef("civilian");
			static const EnemyDef _ced;
			const std::string* hurts[] = {
				gd ? &gd->soundHurt1 : &_ced.soundHurt1,
				gd ? &gd->soundHurt2 : &_ced.soundHurt2,
				gd ? &gd->soundHurt3 : &_ced.soundHurt3
			};
			EmitSound(world, world.resources.soundbank[*hurts[rand() % 3]], 128);
		}
		collidable = false;
		if(state_i >= 14){ state = DEAD; state_i = -1; }
		else { FollowGround(*this, world, xv); res_bank = 126; res_index = state_i; }
		is_dying = true; is_dead = false; is_walking = false; is_running = false;
		state_i++; return;
	}

	// DYINGBACKWARD: backward death animation → DEAD
	if(state == DYINGBACKWARD){
		tractteamid = 0;
		if(state_i == 0){
			const EnemyDef* gd = GASLoader::Get().GetEnemyDef("civilian");
			static const EnemyDef _ced;
			const std::string* hurts[] = {
				gd ? &gd->soundHurt1 : &_ced.soundHurt1,
				gd ? &gd->soundHurt2 : &_ced.soundHurt2,
				gd ? &gd->soundHurt3 : &_ced.soundHurt3
			};
			EmitSound(world, world.resources.soundbank[*hurts[rand() % 3]], 128);
		}
		collidable = false;
		if(state_i >= 14){ state = DEAD; state_i = -1; }
		else { FollowGround(*this, world, xv); res_bank = 125; res_index = state_i; }
		is_dying = true; is_dead = false; is_walking = false; is_running = false;
		state_i++; return;
	}

	// DYINGEXPLODE: instant → DEAD
	if(state == DYINGEXPLODE){
		tractteamid = 0;
		draw = false;
		state = DEAD; state_i = -1;
		is_dying = true; is_dead = false; is_walking = false; is_running = false;
		state_i++; return;
	}

	// DEAD: respawn after deadRespawnTicks
	if(state == DEAD){
		collidable = false;
		const EnemyDef* cd = GASLoader::Get().GetEnemyDef("civilian");
		int drt = cd ? cd->deadRespawnTicks : 100;
		if(state_i >= drt){
			draw = true; collidable = true;
			state = WALKING;
			state_warp = cd ? cd->warpTeleportTick : GASLoader::Get().player.warpTeleportTick;
			state_i = -1;
		}
		is_dead = true; is_dying = false; is_walking = false; is_running = false;
		state_i++; return;
	}

	// STANDING: idle animation
	if(state == STANDING){
		if(CheckTractVictim(world)){ state_i++; return; }
		res_bank = 121; res_index = state_i % 10;
	}

	// WALKING: movement and animation
	if(state == WALKING){
		if(CheckTractVictim(world)){ state_i++; return; }
		res_bank = 122; res_index = state_i % 20;
		{
			auto it = world.resources.actordefs.find("civilian");
			if(it != world.resources.actordefs.end()){
				auto* seq = it->second.GetSequence("WALKING");
				std::string snd; int vol;
				if(seq && seq->GetFrameSoundByIndex(state_i % 20, snd, vol))
					EmitSound(world, world.resources.soundbank[snd], vol);
			}
		}
		if(DistanceToEnd(*this, world) <= world.minwalldistance) mirrored = !mirrored;
		xv = mirrored ? -speed : speed;
		FollowGround(*this, world, xv);
	}

	// RUNNING: fast movement and animation
	if(state == RUNNING){
		if(CheckTractVictim(world)){ state_i++; return; }
		const EnemyDef* cd = GASLoader::Get().GetEnemyDef("civilian");
		int bonus = cd ? cd->runSpeedBonus : 5;
		xv = (mirrored ? -1 : 1) * (bonus + speed);
		res_bank = 123; res_index = state_i % 15;
		{
			auto it = world.resources.actordefs.find("civilian");
			if(it != world.resources.actordefs.end()){
				auto* seq = it->second.GetSequence("RUNNING");
				std::string snd; int vol;
				if(seq && seq->GetFrameSoundByIndex(state_i % 15, snd, vol))
					EmitSound(world, world.resources.soundbank[snd], vol);
			}
		}
		if(DistanceToEnd(*this, world) <= world.minwalldistance) mirrored = !mirrored;
		FollowGround(*this, world, xv);
	}

	btctx_.userData = &world;
	btctx_.dt = 1.0f / GASLoader::Get().gameengine.ticksPerSecond;
	bt_->tick(btctx_);

	is_walking = (state == WALKING);
	is_running = (state == RUNNING);
	is_dying   = (state == DYINGFORWARD || state == DYINGBACKWARD || state == DYINGEXPLODE);
	is_dead    = (state == DEAD);
	state_i++;
}

void Civilian::HandleHit(World & world, Uint8 x, Uint8 y, Object & projectile){
	Hittable::HandleHit(*this, world, x, y, projectile);
	if(projectile.healthdamage == 0){
		return;
	}
	if(state == DYINGFORWARD || state == DYINGBACKWARD || state == DEAD || state == DYINGEXPLODE){
		return;
	}
	float xpcnt = -((x - 50) / 50.0) * (mirrored ? -1 : 1);
	if(x < 50){
		state = DYINGFORWARD;
	}else{
		state = DYINGBACKWARD;
	}
	if((xpcnt < 0 && xv < 0) || (xpcnt > 0 && xv > 0)){
		xv = abs(xv) * xpcnt;
	}else{
		xv = speed * xpcnt;
	}
	if(projectile.type == ObjectTypes::ROCKETPROJECTILE || projectile.type == ObjectTypes::PLASMAPROJECTILE){
		state = DYINGEXPLODE;
		world.Explode(*this, suitcolor, xpcnt);
	}
	Object * owner = world.GetObjectFromId(projectile.ownerid);
	if(owner && owner->type == ObjectTypes::PLAYER){
		Player * player = static_cast<Player *>(owner);
		Peer * peer = player->GetPeer(world);
		if(peer){
			peer->stats.civilianskilled++;
		}
	}
	state_i = 0;
}

bool Civilian::CheckTractVictim(World & world){
	if(!tractteamid){
		return false;
	}
	std::vector<Uint8> types;
	types.push_back(ObjectTypes::PLAYER);
	int x1, y1, x2, y2;
	GetAABB(world.resources, &x1, &y1, &x2, &y2);
	std::vector<Object *> objects = world.TestAABB(x1, y1, x2, y2, types);
	for(std::vector<Object *>::iterator it = objects.begin(); it != objects.end(); it++){
		Player * player = static_cast<Player *>(*it);
		Team * team = player->GetTeam(world);
		if(team && team->id != tractteamid){
			world.Explode(*this, suitcolor, 1);
			state = DYINGEXPLODE;
			{ const EnemyDef* def = GASLoader::Get().GetEnemyDef("civilian"); EmitSound(world, world.resources.soundbank[(def && !def->soundDeath.empty()) ? def->soundDeath : "seekexp1.wav"], 128); }
			Object tractprojectile(ObjectTypes::PLASMAPROJECTILE);
		{
			const EnemyDef* def = GASLoader::Get().GetEnemyDef("civilian");
			tractprojectile.healthdamage = def ? def->tractHealthDamage : 80;
			tractprojectile.shielddamage = def ? def->tractShieldDamage : 80;
		}
			tractprojectile.ownerid = id;
			player->HandleHit(world, 50, 50, tractprojectile);
			Sint8 xvs[] = {-14, 14, -10, 10, -10, 10};
			Sint8 yvs[] = {-25, -25, -10, -10, -5, -5};
			for(int i = 0; i < 6; i++){
				PlasmaProjectile * plasmaprojectile = (PlasmaProjectile *)world.CreateObject(ObjectTypes::PLASMAPROJECTILE);
				if(plasmaprojectile){
					plasmaprojectile->large = false;
					plasmaprojectile->x = x;
					plasmaprojectile->y = y - 40;
					plasmaprojectile->ownerid = id;
					plasmaprojectile->xv = xvs[i];
					plasmaprojectile->yv = yvs[i];
				}
			}
			return true;
		}
	}
	return false;
}

bool Civilian::AddTract(Uint16 teamid){
	if(!tractteamid){
		tractteamid = teamid;
		return true;
	}
	return false;
}