#include "civilian.h"
#include "projectile.h"
#include "bodypart.h"
#include "player.h"
#include "plasmaprojectile.h"
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
	if(bt_) return;
	bt_ = BehaviorTreeLibrary::instance().get("civilian");
	if(!bt_) return;

	// New: find starting platform, then transition to WALKING.
	btctx_.actions["New"] = [this](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		if(state != NEW) return BTResult::Failure;
		if(FindCurrentPlatform(*this, world)){ state = WALKING; state_i = -1; }
		return BTResult::Running;
	};

	// DyingForward: 15-tick forward death animation → DEAD.
	btctx_.actions["DyingForward"] = [this](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		if(state != DYINGFORWARD) return BTResult::Failure;
		tractteamid = 0;
		collidable = false;
		if(ctx.elapsedTicks() == 0){
			const EnemyDef* gd = GASLoader::Get().GetEnemyDef("civilian");
			static const EnemyDef _ced;
			const std::string* hurts[] = {
				gd ? &gd->soundHurt1 : &_ced.soundHurt1,
				gd ? &gd->soundHurt2 : &_ced.soundHurt2,
				gd ? &gd->soundHurt3 : &_ced.soundHurt3
			};
			EmitSound(world, world.resources.soundbank[*hurts[rand() % 3]], 128);
		}
		if(ctx.elapsedTicks() >= 14){ state = DEAD; state_i = -1; }
		else { FollowGround(*this, world, xv); res_bank = 126; res_index = ctx.elapsedTicks(); }
		return BTResult::Running;
	};

	// DyingBackward: 15-tick backward death animation → DEAD.
	btctx_.actions["DyingBackward"] = [this](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		if(state != DYINGBACKWARD) return BTResult::Failure;
		tractteamid = 0;
		collidable = false;
		if(ctx.elapsedTicks() == 0){
			const EnemyDef* gd = GASLoader::Get().GetEnemyDef("civilian");
			static const EnemyDef _ced;
			const std::string* hurts[] = {
				gd ? &gd->soundHurt1 : &_ced.soundHurt1,
				gd ? &gd->soundHurt2 : &_ced.soundHurt2,
				gd ? &gd->soundHurt3 : &_ced.soundHurt3
			};
			EmitSound(world, world.resources.soundbank[*hurts[rand() % 3]], 128);
		}
		if(ctx.elapsedTicks() >= 14){ state = DEAD; state_i = -1; }
		else { FollowGround(*this, world, xv); res_bank = 125; res_index = ctx.elapsedTicks(); }
		return BTResult::Running;
	};

	// DyingExplode: instant draw hide → DEAD.
	btctx_.actions["DyingExplode"] = [this](BTContext&) -> BTResult {
		if(state != DYINGEXPLODE) return BTResult::Failure;
		tractteamid = 0;
		draw = false;
		state = DEAD; state_i = -1;
		return BTResult::Running;
	};

	// Dead: wait deadRespawnTicks, then respawn as WALKING.
	btctx_.actions["Dead"] = [this](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		if(state != DEAD) return BTResult::Failure;
		collidable = false;
		const EnemyDef* cd = GASLoader::Get().GetEnemyDef("civilian");
		int drt = cd ? cd->deadRespawnTicks : 100;
		if(ctx.elapsedTicks() >= drt){
			draw = true; collidable = true;
			state = WALKING;
			state_warp = cd ? cd->warpTeleportTick : GASLoader::Get().player.warpTeleportTick;
			state_i = -1;
		}
		return BTResult::Running;
	};

	// TractCheck: if tracted and hostile player overlaps, explode. Failure = no victim.
	btctx_.actions["TractCheck"] = [this](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		return CheckTractVictim(world) ? BTResult::Success : BTResult::Failure;
	};

	// RunMove: enter RUNNING if walking near a projectile threat; do RUNNING movement;
	// return to WALKING after runDurationTicks. Returns Running while running, Failure otherwise.
	btctx_.actions["RunMove"] = [this](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		const EnemyDef* cd = GASLoader::Get().GetEnemyDef("civilian");
		// Check for threats every tick — updates flee direction and triggers RUNNING from WALKING.
		// Matches original: ThreatLook re-ran every tick after the movement block.
		if(state == WALKING || state == RUNNING){
			std::vector<Uint8> types = {
				ObjectTypes::BLASTERPROJECTILE, ObjectTypes::LASERPROJECTILE,
				ObjectTypes::ROCKETPROJECTILE,  ObjectTypes::FLAMERPROJECTILE,
				ObjectTypes::PLASMAPROJECTILE,  ObjectTypes::WALLPROJECTILE,
				ObjectTypes::FLAREPROJECTILE
			};
			int dx = cd ? cd->threatDetectX : 200;
			int dy = cd ? cd->threatDetectY : 100;
			std::vector<Object*> threats = world.TestAABB(x - dx, y - dy, x + dx, y + dy, types);
			// debug: scan wider area with no type filter to count any objects
			std::vector<Uint8> alltypes;
			auto allnearby = world.TestAABB(x - 2000, y - 2000, x + 2000, y + 2000, alltypes);
			fprintf(stderr, "[civ#%d] x=%d y=%d threats=%d allobjs_2000=%d\n", id, x, y, (int)threats.size(), (int)allnearby.size());
			if(!threats.empty()){
				mirrored = (threats[0]->x > x);
				if(state != RUNNING){ fprintf(stderr, "[civ#%d] WALKING→RUNNING\n", id); state = RUNNING; state_i = -1; }
			}
		}
		if(state != RUNNING) return BTResult::Failure;
		// Exit RUNNING after runDurationTicks
		int runDuration = cd ? cd->runDurationTicks : 150;
		if(state_i >= runDuration){ state = WALKING; state_i = -1; return BTResult::Failure; }
		// RUNNING movement
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
		return BTResult::Running;
	};

	// StandAnim: STANDING idle animation. Returns Running to keep Selector here.
	btctx_.actions["StandAnim"] = [this](BTContext&) -> BTResult {
		if(state != STANDING) return BTResult::Failure;
		res_bank = 121; res_index = state_i % 10;
		return BTResult::Running;
	};

	// WalkMove: WALKING movement and animation. Returns Running to keep Selector here.
	btctx_.actions["WalkMove"] = [this](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		if(state != WALKING) return BTResult::Failure;
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
		return BTResult::Running;
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
	if(!bt_){ state_i++; return; }
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