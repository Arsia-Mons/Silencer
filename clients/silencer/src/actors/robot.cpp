#include "robot.h"
#include "rocketprojectile.h"
#include "plasmaprojectile.h"
#include "player.h"
#include "fixedcannon.h"
#include "plume.h"
#include "gasloader.h"

Robot::Robot() : Object(ObjectTypes::ROBOT) {
	requiresauthority = true;
	state   = NEW;
	state_i = 0;
	res_bank  = 47;
	res_index = 0;
	const EnemyDef* r = GASLoader::Get().GetEnemyDef("robot");
	maxhealth        = r ? r->health          : 200;
	health           = maxhealth;
	maxshield        = r ? r->shield          : 400;
	shield           = maxshield;
	snapshotinterval = r ? r->snapshotInterval : 48;
	respawnseconds   = r ? r->respawnSeconds   : 45;
	renderpass    = 2;
	ishittable    = true;
	isbipedal     = true;
	isphysical    = true;
	virusplanter  = 0;
	damaging      = 0;
	soundchannel  = -1;
	patrol        = false;
	shootcooldown = 0;
}

void Robot::InitBT() {
	bt_ = BehaviorTreeLibrary::instance().get("robot");
	if (!bt_) return;

	// New: handles NEW state — fall to platform, then transition to ASLEEP or WALKING.
	btctx_.actions["New"] = [this](BTContext& ctx) -> BTResult {
		if (state != NEW) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		draw = true;
		currentplatformid = 0;
		if (FindCurrentPlatform(*this, world)) {
			state   = patrol ? WALKING : ASLEEP;
			state_i = -1;
			return BTResult::Success;
		}
		yv += world.gravity;
		if (yv > world.maxyvelocity) yv = world.maxyvelocity;
		return BTResult::Running;
	};

	// Dying: death animation, pickup drop, plasma explosion — transitions to DEAD.
	btctx_.actions["Dying"] = [this](BTContext& ctx) -> BTResult {
		if (state != DYING) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
		res_bank  = 48;
		res_index = std::min(state_i / 2, 15);
		if (state_i == 0) {
			PickUp* pickup = (PickUp*)world.CreateObject(ObjectTypes::PICKUP);
			if (pickup) {
				pickup->type     = PickUp::FILES;
				pickup->quantity = rd ? rd->deathDropFiles : 250;
				pickup->x        = x; pickup->y = y - 1;
				pickup->xv       = (world.Random() % (2 * (rd ? rd->deathDropXVRange : 4) + 1)) - (rd ? rd->deathDropXVRange : 4);
				pickup->yv       = -(rd ? rd->deathDropYV : 15);
			}
		}
		if (state_i % 2 == 0 && state_i >= 5) {
			Plume* plume = (Plume*)world.CreateObject(ObjectTypes::PLUME);
			if (plume) {
				plume->type = 4;
				plume->xv   = (rand() % 17) - 8 + (xv * 8);
				plume->yv   = (rand() % 17) - 8 + (yv * 8);
				plume->SetPosition(x + (rand() % 39) - 19, y - 5);
				plume->state_i = 0;
			}
		}
		if (state_i == 4 * 2) {
			StopAmbience();
			EmitSound(world, world.resources.soundbank[(rd && !rd->soundDeath.empty()) ? rd->soundDeath : "seekexp1.wav"], 128);
		}
		collidable = false;
		if (state_i >= (rd ? rd->deathExplosionDelayTicks : 96)) {
			EmitSound(world, world.resources.soundbank[(rd && !rd->soundDeath.empty()) ? rd->soundDeath : "seekexp1.wav"], 128);
			Sint8 xvs[] = {-14, 14, -10, 10, -10, 10};
			Sint8 yvs[] = {-25, -25, -10, -10, -5, -5};
			Sint8 ys[]  = {0, 0, 0, 0, 0, 0, 0, 0};
			for (int i = 0; i < 6; i++) {
				PlasmaProjectile* pp = (PlasmaProjectile*)world.CreateObject(ObjectTypes::PLASMAPROJECTILE);
				if (pp) { pp->large = false; pp->x = x; pp->y = y - 1 + ys[i]; pp->ownerid = id; pp->xv = xvs[i]; pp->yv = yvs[i]; }
			}
			state   = DEAD;
			state_i = -1;
			return BTResult::Success;
		}
		return BTResult::Running;
	};

	// Dead: respawn countdown — transitions to NEW when timer expires.
	btctx_.actions["Dead"] = [this](BTContext& ctx) -> BTResult {
		if (state != DEAD) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		StopAmbience();
		collidable   = false;
		virusplanter = 0;
		res_bank = 48; res_index = 15;
		if (state_i > 1) draw = false;
		if (state_i >= respawnseconds) {
			state      = NEW; x = originalx; y = originaly;
			state_i    = -1;
			state_warp = GASLoader::Get().player.warpTeleportTick;
			health = maxhealth; shield = maxshield;
			return BTResult::Success;
		}
		if (world.tickcount % GASLoader::Get().gameengine.ticksPerSecond != 0) state_i--;
		return BTResult::Running;
	};

	// WakeUp: owns ASLEEP state — sets animation/sound, looks both sides.
	// Returns Running while dormant; Success when target found (transitions to AWAKENING).
	btctx_.actions["WakeUp"] = [this](BTContext& ctx) -> BTResult {
		if (state != ASLEEP) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
		if (soundchannel == -1)
			soundchannel = EmitSound(world, world.resources.soundbank[(rd && !rd->soundAmbient.empty()) ? rd->soundAmbient : "wndloope.wav"], 32, true);
		res_bank = 47; res_index = 0;
		if (!Look(world, 1) && !Look(world, 2)) return BTResult::Running;
		state   = AWAKENING;
		state_i = -1;
		return BTResult::Success;
	};

	// WalkAnim: sets walking animation and footstep sounds each WALKING tick.
	// Returns Failure so the Selector continues to behaviour leaves.
	btctx_.actions["WalkAnim"] = [this](BTContext& ctx) -> BTResult {
		if (state != WALKING) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
		res_bank  = 45;
		res_index = state_i % 20;
		if (state_i % 20 == 1) {
			StopAmbience();
			EmitSound(world, world.resources.soundbank[(rd && !rd->soundMoveRight.empty()) ? rd->soundMoveRight : "robot3r.wav"], 48);
		}
		if (state_i % 20 == 10) {
			StopAmbience();
			EmitSound(world, world.resources.soundbank[(rd && !rd->soundMoveLeft.empty()) ? rd->soundMoveLeft : "robot3l.wav"], 48);
		}
		return BTResult::Failure;
	};

	// LookForward: only from WALKING — long-range forward shoot ray.
	// Skips the shot if shootcooldown is active (let other nodes run instead).
	btctx_.actions["LookForward"] = [this](BTContext& ctx) -> BTResult {
		if (state != WALKING) return BTResult::Failure;
		const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
		if (shootcooldown && shootcooldown < (rd ? rd->shootCooldownCap : 50)) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		if (!Look(world, 0)) return BTResult::Failure;
		state = SHOOTING;
		state_i = -1;
		return BTResult::Success;
	};

	// LookSides: only from WALKING during search phase (bt_walk_ticks_ < searchTicks from GAS) — orient toward target.
	// Always returns Failure so the Selector continues to Patrol (orient + move each tick).
	btctx_.actions["LookSides"] = [this](BTContext& ctx) -> BTResult {
		if (state != WALKING) return BTResult::Failure;
		const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
		if (bt_walk_ticks_ >= (rd ? rd->searchTicks : 600)) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		if (Look(world, 2))      mirrored = true;
		else if (Look(world, 1)) mirrored = false;
		return BTResult::Failure;
	};

	// MeleeCheck: only from WALKING, throttled to every 40 ticks.
	btctx_.actions["MeleeCheck"] = [this](BTContext& ctx) -> BTResult {
		if (state != WALKING) return BTResult::Failure;
		const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
		if (state_i % (rd ? rd->meleeCheckInterval : 40) != 0) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		int x1, y1, x2, y2;
		GetAABB(world.resources, &x1, &y1, &x2, &y2);
		std::vector<Uint8> types;
		types.push_back(ObjectTypes::PLAYER);
		types.push_back(ObjectTypes::FIXEDCANNON);
		types.push_back(ObjectTypes::GUARD);
		std::vector<Object*> objects = world.TestAABB(x1, y1, x2, y2, types);
		bool meleed = false;
		for (auto* obj : objects) {
			switch (obj->type) {
				case ObjectTypes::PLAYER: {
					Player* player = static_cast<Player*>(obj);
					Team* team = player->GetTeam(world);
					if (!player->IsDisguised() && !player->IsInvisible(world)
					    && (team && team->id != virusplanter) && !player->HasSecurityPass() && player->IsAlive()) {
						Melee(*obj, world); meleed = true;
					}
				} break;
				case ObjectTypes::FIXEDCANNON: {
					FixedCannon* fc = static_cast<FixedCannon*>(obj);
					if (fc->teamid != virusplanter) { Melee(*obj, world); meleed = true; }
				} break;
				case ObjectTypes::GUARD: {
					if (world.IsSecurity(*obj) && !world.IsSecurity(*this)) {
						Melee(*obj, world); meleed = true;
					}
				} break;
			}
		}
		if (meleed) {
			StopAmbience();
			const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
			EmitSound(world, world.resources.soundbank[(rd && !rd->soundFire.empty()) ? rd->soundFire : "!laserew.wav"], 64);
			return BTResult::Success;
		}
		return BTResult::Failure;
	};

	// Patrol: only from WALKING — move, follow ground, flip at wall.
	btctx_.actions["Patrol"] = [this](BTContext& ctx) -> BTResult {
		if (state != WALKING) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
		xv = mirrored ? -(rd ? rd->speed : 4) : (rd ? rd->speed : 4);
		FollowGround(*this, world, xv);
		int d = DistanceToEnd(*this, world);
		if (d >= 0 && d <= world.minwalldistance) mirrored = !mirrored;
		return BTResult::Success;
	};

	// ReturnToSpawn: after search phase (!patrol, searchTicks from GAS), walk back to spawn and sleep.
	// Owns its own movement (returns Running) so Patrol never clobbers mirrored during return.
	// If a target is spotted en-route, reset the search timer and resume hunting.
	btctx_.actions["ReturnToSpawn"] = [this](BTContext& ctx) -> BTResult {
		if (state != WALKING) return BTResult::Failure;
		if (patrol) return BTResult::Failure;
		const EnemyDef* _rd = GASLoader::Get().GetEnemyDef("robot");
		if (bt_walk_ticks_ < (_rd ? _rd->searchTicks : 600)) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		if (Look(world, 1) || Look(world, 2)) {
			bt_walk_ticks_ = 0; // target spotted — reset and keep hunting
			return BTResult::Failure;
		}
		// Orient toward spawn, drive movement directly — Patrol must not run during return.
		mirrored = (signed(originalx) < signed(x));
		if (abs(signed(x) - signed(originalx)) <= (_rd ? _rd->returnProximity : 20)) {
			state   = SLEEPING;
			state_i = -1;
			return BTResult::Success;
		}
		const Sint8 spd = _rd ? _rd->speed : 4;
		xv = mirrored ? -spd : spd;
		FollowGround(*this, world, xv);
		return BTResult::Running; // Running keeps Patrol from running this tick
	};

	// Shoot: drives 36-tick shoot animation (res_bank=46), fires rocket at frame 11.
	// Only runs from SHOOTING state (LookForward sets it). Returns Success when done.
	btctx_.actions["Shoot"] = [this](BTContext& ctx) -> BTResult {
		if (state != SHOOTING) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		int tick = ctx.elapsedTicks();
		const int total = 36;
		res_bank  = 46;
		res_index = tick < 18 ? tick : (total - tick - 1);
		if (tick == 11) {
			RocketProjectile* rp = (RocketProjectile*)world.CreateObject(ObjectTypes::ROCKETPROJECTILE);
			if (rp) {
				rp->FromSecurity();
				rp->ownerid = id;
				const EnemyDef* grd = GASLoader::Get().GetEnemyDef("robot");
				const int rox = grd ? grd->rocketOffsetX   : 70;
				const int roy = grd ? grd->rocketOffsetY   : 60;
				const int rxv = grd ? grd->rocketLaunchXv  : 25;
				rp->y = y - roy;
				if (mirrored) { rp->x = x - rox; rp->xv = -rxv; }
				else          { rp->x = x + rox; rp->xv =  rxv; }
				shootcooldown = 1;
			}
		}
		if (tick >= total - 1) { state = WALKING; state_i = 0; return BTResult::Success; }
		return BTResult::Running;
	};

	// WakeAnim: plays 15-tick wake-up animation (res_bank=47, frames 0→14) then transitions to WALKING.
	// Fires wake sound on tick 0. Handles both BT-triggered and HandleHit-triggered AWAKENING.
	btctx_.actions["WakeAnim"] = [this](BTContext& ctx) -> BTResult {
		if (state != AWAKENING) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		if (ctx.elapsedTicks() == 0) {
			StopAmbience();
			const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
			EmitSound(world, world.resources.soundbank[(rd && !rd->soundMelee.empty()) ? rd->soundMelee : "robotarm.wav"], 128);
		}
		res_bank  = 47;
		res_index = std::min(ctx.elapsedTicks(), 14);
		if (ctx.elapsedTicks() >= 15) { state = WALKING; state_i = 0; return BTResult::Success; }
		return BTResult::Running;
	};

	// SleepAnim: plays 15-tick sleep animation (res_bank=47, frames 14→0) then transitions to ASLEEP.
	// Runs after ReturnToSpawn sets state=SLEEPING.
	btctx_.actions["SleepAnim"] = [this](BTContext& ctx) -> BTResult {
		if (state != SLEEPING) return BTResult::Failure;
		res_bank  = 47;
		res_index = 14 - std::min(ctx.elapsedTicks(), 14);
		if (ctx.elapsedTicks() >= 15) { state = ASLEEP; state_i = 0; return BTResult::Success; }
		return BTResult::Running;
	};
}

void Robot::Serialize(bool write, Serializer& data, Serializer* old) {
	Object::Serialize(write, data, old);
	data.Serialize(write, state, old);
	data.Serialize(write, state_i, old);
	data.Serialize(write, damaging, old);
	data.Serialize(write, virusplanter, old);
	data.Serialize(write, patrol, old);
	data.Serialize(write, shootcooldown, old);
	data.Serialize(write, originalx, old);
	data.Serialize(write, originaly, old);
}

void Robot::Tick(World& world) {
	Hittable::Tick(*this, world);
	Bipedal::Tick(*this, world);

	if (shootcooldown) shootcooldown++;

	const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");

	if (state != DEAD && rand() % (rd ? rd->ambientSoundIntervalTicks : 360) == 0) {
		StopAmbience();
		EmitSound(world, world.resources.soundbank[(rd && !rd->soundActivate.empty()) ? rd->soundActivate : "airlokj.wav"], 64);
	}

	InitBT();
	if (!bt_) { state_i++; return; }

	if (state == WALKING) bt_walk_ticks_++;
	else                   bt_walk_ticks_ = 0;

	btctx_.userData = &world;
	btctx_.dt       = 1.0f / GASLoader::Get().gameengine.ticksPerSecond;
	btctx_.bbSet("patrol", (bool)patrol);
	bt_->tick(btctx_);

	is_walking  = (state == WALKING);
	is_shooting = (state == SHOOTING);
	is_asleep   = (state == ASLEEP || state == SLEEPING);
	is_dying    = (state == DYING);
	is_dead     = (state == DEAD);

	if (damaging) {
		damaging++;
		if (damaging > (rd ? rd->meleeHitDuration : 24)) damaging = 0;
	}
	state_i++;
}

void Robot::HandleHit(World& world, Uint8 x, Uint8 y, Object& projectile) {
	Hittable::HandleHit(*this, world, x, y, projectile);
	if (health == 0 && state != DYING && state != DEAD) {
		state   = DYING;
		xv      = 0;
		state_i = 0;
		Object* owner = world.GetObjectFromId(projectile.ownerid);
		if (owner && owner->type == ObjectTypes::PLAYER) {
			Player* player = static_cast<Player*>(owner);
			Peer*   peer   = player->GetPeer(world);
			if (peer) peer->stats.robotskilled++;
		}
	} else if (health > 0 && (state == ASLEEP || state == SLEEPING)) {
		StopAmbience();
		const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
		EmitSound(world, world.resources.soundbank[(rd && !rd->soundMelee.empty()) ? rd->soundMelee : "robotarm.wav"], 128);
		state   = AWAKENING;
		state_i = -1;
	}
}

bool Robot::ImplantVirus(Uint16 teamid) {
	if(!virusplanter || virusplanter != teamid){
		virusplanter = teamid;
		return true;
	}
	return false;
}

bool Robot::Look(World& world, Uint8 direction) {
	// 0: forward target
	// 1: forward
	// 2: backward
	std::vector<Uint8> types;
	types.push_back(ObjectTypes::PLAYER);
	types.push_back(ObjectTypes::FIXEDCANNON);
	if(virusplanter){
		types.push_back(ObjectTypes::GUARD);
	}
	const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
	int y1 = rd ? rd->lookDefaultY : -60;
	int y2 = y1;
	int minx = rd ? rd->lookDefaultMinX : 70;
	int maxx = rd ? rd->lookDefaultMaxX : 500;
	minx *= (mirrored ? -1 : 1);
	maxx *= (mirrored ? -1 : 1);
	switch(direction){
		case 0:
		break;
		case 1:
			minx = rd ? rd->lookDirMinX : 70;
			maxx = rd ? rd->lookDirMaxX : 200;
			y1 = rd ? rd->lookDirY1 : -10;
			y2 = rd ? rd->lookDirY2 : -100;
		break;
		case 2:
			minx = -(rd ? rd->lookDirMinX : 70);
			maxx = -(rd ? rd->lookDirMaxX : 200);
			y1 = rd ? rd->lookDirY1 : -10;
			y2 = rd ? rd->lookDirY2 : -100;
		break;
	}
	if(signed(x) + minx < 0){
		minx = -x;
	}
	if(signed(x) + maxx < 0){
		maxx = -x;
	}
	if(signed(y) + y1 < 0){
		y1 = -y;
	}
	if(signed(y) + y2 < 0){
		y2 = -y;
	}
	std::vector<Object *> objects = world.TestAABB(x + minx, y + y1, x + maxx, y + y2, types);
	for(std::vector<Object *>::iterator it = objects.begin(); it != objects.end(); it++){
		bool target = false;
		switch((*it)->type){
			case ObjectTypes::PLAYER:{
				Player * player = static_cast<Player *>(*it);
				Team * team = player->GetTeam(world);
				if(!player->IsDisguised() && !player->IsInvisible(world) && !player->HasSecurityPass() && player->IsAlive() && (team && team->id != virusplanter)){
					target = true;
				}
			}break;
			case ObjectTypes::GUARD:{
				target = true;
			}break;
			case ObjectTypes::FIXEDCANNON:{
				if(virusplanter){
					FixedCannon * fixedcannon = static_cast<FixedCannon *>(*it);
					if(fixedcannon->teamid == virusplanter){
						target = false;
					}else{
						target = true;
					}
				}else{
					target = true;
				}
			}
		}
		if(target){
			int xv2 = maxx - minx;
			int yv2 = y2 - y1;
			Object * object = world.TestIncr(x + minx, y + y1 - 1, x + minx, y + y1, &xv2, &yv2, types);
			if(object){
				if(!world.map.TestIncr(x + minx, y + y1 - 1, x + minx, y + y1, &xv2, &yv2, Platform::STAIRSUP | Platform::STAIRSDOWN | Platform::RECTANGLE, 0, true)){
					if(state == ASLEEP){
						if(object->x < x){
							mirrored = true;
						}else{
							mirrored = false;
						}
					}
					return true;
				}
			}
		}
	}
	return false;
}

void Robot::StopAmbience() {
	if(soundchannel != -1){
		const EnemyDef* _rd = GASLoader::Get().GetEnemyDef("robot");
		Audio::GetInstance().Stop(soundchannel, _rd ? _rd->audioFadeAmbientMs : 800);
	}
	soundchannel = -1;
}

void Robot::Melee(Object& object, World& world) {
	damaging = 1;
	const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
	Object damageprojectile(ObjectTypes::FLAREPROJECTILE);
	damageprojectile.healthdamage = rd ? rd->meleeDamageHealth : 60;
	damageprojectile.shielddamage = rd ? rd->meleeDamageShield : 60;
	damageprojectile.ownerid = id;
	object.HandleHit(world, 50, 50, damageprojectile);
}