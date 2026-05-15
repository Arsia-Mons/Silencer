#include "robot.h"
#include "rocketprojectile.h"
#include "plasmaprojectile.h"
#include "player.h"
#include "fixedcannon.h"
#include "plume.h"
#include "gasloader.h"

Robot::Robot() : Object(ObjectTypes::ROBOT){
	requiresauthority = true;
	state = NEW;
	state_i = 0;
	res_bank = 47;
	res_index = 0;
	const EnemyDef* r = GASLoader::Get().GetEnemyDef("robot");
	maxhealth = r ? r->health : 200;
	health    = maxhealth;
	maxshield = r ? r->shield : 400;
	shield    = maxshield;
	renderpass = 2;
	ishittable = true;
	isbipedal = true;
	isphysical = true;
	{ const EnemyDef* _rd = GASLoader::Get().GetEnemyDef("robot");
	  snapshotinterval = _rd ? _rd->snapshotInterval : 48; }
	respawnseconds = r ? r->respawnSeconds : 45;
	speed = r ? r->speed : 4;
	virusplanter = 0;
	damaging = 0;
	soundchannel = -1;
	patrol = false;
	shootcooldown = 0;
}

void Robot::InitBT() {
	bt_ = BehaviorTreeLibrary::instance().get("robot");
	if (!bt_) return;

	// WakeUp: only fires from ASLEEP — look both sides, awaken if target found.
	btctx_.actions["WakeUp"] = [this](BTContext& ctx) -> BTResult {
		if (state != ASLEEP) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		bool found = Look(world, 1) || Look(world, 2);
		if (!found) return BTResult::Failure;
		state = AWAKENING;
		state_i = -1;
		return BTResult::Success;
	};

	// LookForward: only from WALKING — long-range forward shoot ray.
	btctx_.actions["LookForward"] = [this](BTContext& ctx) -> BTResult {
		if (state != WALKING) return BTResult::Failure;
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
		if (bt_walk_ticks_ >= (GASLoader::Get().GetEnemyDef("robot") ? GASLoader::Get().GetEnemyDef("robot")->searchTicks : 600)) return BTResult::Failure; // ReturnToSpawn handles orientation
		World& world = *static_cast<World*>(ctx.userData);
		if (Look(world, 2)) { mirrored = true; }
		else if (Look(world, 1)) { mirrored = false; }
		return BTResult::Failure;
	};

	// MeleeCheck: only from WALKING, throttled to every 40 ticks.
	btctx_.actions["MeleeCheck"] = [this](BTContext& ctx) -> BTResult {
		if (state != WALKING) return BTResult::Failure;
		if (state_i % (GASLoader::Get().GetEnemyDef("robot") ? GASLoader::Get().GetEnemyDef("robot")->meleeCheckInterval : 40) != 0) return BTResult::Failure;
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
			{ const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot"); EmitSound(world, world.resources.soundbank[(rd && !rd->soundFire.empty()) ? rd->soundFire : "!laserew.wav"], 64); }
			return BTResult::Success;
		}
		return BTResult::Failure;
	};

	// Patrol: only from WALKING — move, follow ground, flip at wall.
	btctx_.actions["Patrol"] = [this](BTContext& ctx) -> BTResult {
		if (state != WALKING) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		xv = mirrored ? -(Sint8)speed : (Sint8)speed;
		FollowGround(*this, world, xv);
		if (DistanceToEnd(*this, world) <= world.minwalldistance) mirrored = !mirrored;
		return BTResult::Success;
	};

	// ReturnToSpawn: after search phase (!patrol, searchTicks from GAS), walk back to spawn and sleep.
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
		// Orient toward spawn and let Patrol move us there
		mirrored = (signed(originalx) < signed(x));
		if (abs(signed(x) - signed(originalx)) <= (GASLoader::Get().GetEnemyDef("robot") ? GASLoader::Get().GetEnemyDef("robot")->returnProximity : 20)) {
			state = SLEEPING;
			state_i = -1;
			return BTResult::Success;
		}
		return BTResult::Failure; // not at spawn yet — Patrol will move us
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
		int bank   = ctx.props->value("bank",   0);
		int frames = ctx.props->value("frames", 1);
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
		auto it = world.resources.soundbank.find(snd);
		if(it == world.resources.soundbank.end()) return BTResult::Failure;
		EmitSound(world, it->second, vol);
		return BTResult::Success;
	};
	btctx_.actions["SetFacing"] = [this](BTContext& ctx) -> BTResult {
		std::string dir = ctx.props ? ctx.props->value("dir", std::string{"flip"}) : "flip";
		if(dir == "left")       mirrored = true;
		else if(dir == "right") mirrored = false;
		else                    mirrored = !mirrored;
		return BTResult::Success;
	};

	btctx_.actions["SetSpeed"] = [this](BTContext& ctx) -> BTResult {
		if (!ctx.props) return BTResult::Failure;
		int spd = ctx.props->value("speed", -1);
		if (spd < 0) return BTResult::Failure;
		speed = (Uint8)spd;
		return BTResult::Success;
	};

	btctx_.actions["ApplyVelocity"] = [this](BTContext& ctx) -> BTResult {
		if (!ctx.props) return BTResult::Failure;
		if (ctx.props->contains("xv")) xv = (Sint8)ctx.props->value("xv", 0);
		if (ctx.props->contains("yv")) yv = (Sint8)ctx.props->value("yv", 0);
		return BTResult::Success;
	};

	btctx_.actions["CheckGround"] = [this](BTContext& ctx) -> BTResult {
		std::string key = ctx.props ? ctx.props->value("key", std::string{"on_ground"}) : "on_ground";
		bool grounded = (yv == 0);
		ctx.bbSet(key, grounded);
		return grounded ? BTResult::Success : BTResult::Failure;
	};
}

void Robot::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state, old);
	data.Serialize(write, state_i, old);
	data.Serialize(write, damaging, old);
	data.Serialize(write, virusplanter, old);
	data.Serialize(write, patrol, old);
	data.Serialize(write, shootcooldown, old);
}

void Robot::Tick(World & world){
	Hittable::Tick(*this, world);
	Bipedal::Tick(*this, world);
	if(shootcooldown){
		shootcooldown++;
	}
	{ const EnemyDef* _ramb = GASLoader::Get().GetEnemyDef("robot");
	  if(state != DEAD && rand() % (_ramb ? _ramb->ambientSoundIntervalTicks : 360) == 0){
		StopAmbience();
		EmitSound(world, world.resources.soundbank[(_ramb && !_ramb->soundActivate.empty()) ? _ramb->soundActivate : "airlokj.wav"], 64);
	  }
	}
	// BT patrol timer: counts up while WALKING, resets in any other state
	if (bt_) {
		if (state == WALKING) bt_walk_ticks_++;
		else bt_walk_ticks_ = 0;
	}
	switch(state){
		case NEW:{
			draw = true;
			currentplatformid = 0;
			if(FindCurrentPlatform(*this, world)){
				if(patrol){
					state = WALKING;
				}else{
					state = ASLEEP;
				}
			}
		}break;
		case SLEEPING:{
			if(state_i >= 15){
				state = ASLEEP;
				state_i = -1;
				break;
			}
			res_bank = 47;
			res_index = 14 - state_i;
		}break;
		case ASLEEP:{
			if(soundchannel == -1){
				{ const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot"); soundchannel = EmitSound(world, world.resources.soundbank[(rd && !rd->soundAmbient.empty()) ? rd->soundAmbient : "wndloope.wav"], 32, true); }
			}
			res_bank = 47;
			res_index = 0;
			if (bt_) {
				// BT handles wakeup
			} else {
				if(Look(world, 1) || Look(world, 2)){
					state = AWAKENING;
					state_i = -1;
				}
			}
		}break;
		case AWAKENING:{
			if(state_i == 0){
				StopAmbience();
				{ const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot"); EmitSound(world, world.resources.soundbank[(rd && !rd->soundMelee.empty()) ? rd->soundMelee : "robotarm.wav"], 128); }
			}
			if(state_i >= 15){
				state = WALKING;
				state_i = -1;
				break;
			}
			res_bank = 47;
			res_index = state_i;
		}break;
		case WALKING:{
			if(state_i > 240){
				state_i = 0;
			}
			if(state_i % 20 == 1){
				StopAmbience();
				{ const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot"); EmitSound(world, world.resources.soundbank[(rd && !rd->soundMoveRight.empty()) ? rd->soundMoveRight : "robot3r.wav"], 48); }
			}
			if(state_i % 20 == 10){
				StopAmbience();
				{ const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot"); EmitSound(world, world.resources.soundbank[(rd && !rd->soundMoveLeft.empty()) ? rd->soundMoveLeft : "robot3l.wav"], 48); }
			}
			res_bank = 45;
			res_index = state_i % 20;
			if (!bt_) {
				const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
				if(state_i >= (rd ? rd->sleepTicks : 100) && !patrol){
					state = SLEEPING;
					state_i = -1;
					break;
				}
				if(Look(world, 0)){
					state = SHOOTING;
					state_i = -1;
					break;
				}
				if(Look(world, 2)){ mirrored = true; }
				if(Look(world, 1)){ mirrored = false; }
				if(state_i % (GASLoader::Get().GetEnemyDef("robot") ? GASLoader::Get().GetEnemyDef("robot")->meleeCheckInterval : 40) == 0){
					int x1, y1, x2, y2;
					GetAABB(world.resources, &x1, &y1, &x2, &y2);
					std::vector<Uint8> types;
					types.push_back(ObjectTypes::PLAYER);
					types.push_back(ObjectTypes::FIXEDCANNON);
					types.push_back(ObjectTypes::GUARD);
					std::vector<Object *> objects = world.TestAABB(x1, y1, x2, y2, types);
					bool meleed = false;
					for(std::vector<Object *>::iterator it = objects.begin(); it != objects.end(); it++){
						switch((*it)->type){
							case ObjectTypes::PLAYER:{
								Player * player = static_cast<Player *>(*it);
								Team * team = player->GetTeam(world);
								if(!player->IsDisguised() && !player->IsInvisible(world) && (team && team->id != virusplanter) && !player->HasSecurityPass()){
									Melee(*(*it), world);
									meleed = true;
								}
							}break;
							case ObjectTypes::FIXEDCANNON:{
								FixedCannon * fixedcannon = static_cast<FixedCannon *>(*it);
								if(fixedcannon->teamid != virusplanter){
									Melee(*(*it), world);
									meleed = true;
								}
							}break;
							case ObjectTypes::GUARD:{
								if(world.IsSecurity(*(*it)) && !world.IsSecurity(*this)){
									Melee(*(*it), world);
									meleed = true;
								}
							}break;
						}
					}
					if(meleed){
						StopAmbience();
						{ const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot"); EmitSound(world, world.resources.soundbank[(rd && !rd->soundFire.empty()) ? rd->soundFire : "!laserew.wav"], 64); }
					}
				}
				xv = mirrored ? -(Sint8)speed : (Sint8)speed;
				FollowGround(*this, world, xv);
				if(DistanceToEnd(*this, world) <= world.minwalldistance){
					mirrored = mirrored ? false : true;
				}
			}
		}break;
		case SHOOTING:{
			if(state_i >= 18 * 2){
				state = WALKING;
				state_i = -1;
				break;
			}
			res_bank = 46;
			if(state_i < 18){
				res_index = state_i;
			}else{
				res_index = (18 * 2) - state_i - 1;
			}
			if(state_i == 0){
				if(Look(world, 0)){
					if(shootcooldown < ([](){ const EnemyDef* _g = GASLoader::Get().GetEnemyDef("robot"); return _g ? _g->shootCooldownCap : 50; }()) && shootcooldown){
						state_i--;
					}
				}
			}
			if(state_i == 11){
				RocketProjectile * rocketprojectile = (RocketProjectile *)world.CreateObject(ObjectTypes::ROCKETPROJECTILE);
				if(rocketprojectile){
					rocketprojectile->FromSecurity();
					rocketprojectile->ownerid = id;
					{ const EnemyDef* _grd = GASLoader::Get().GetEnemyDef("robot");
					  const int _rox = _grd ? _grd->rocketOffsetX : 70;
					  const int _roy = _grd ? _grd->rocketOffsetY : 60;
					  rocketprojectile->y = y - _roy;
					  if(mirrored){
						rocketprojectile->x = x - _rox;
						rocketprojectile->xv = -(_grd ? _grd->rocketLaunchXv : 25);
					  }else{
						rocketprojectile->x = x + _rox;
						rocketprojectile->xv = (_grd ? _grd->rocketLaunchXv : 25);
					  }
					}
				}
				shootcooldown = 1;
			}
		}break;
		case DYING:{
			if(state_i == 0){
				PickUp * pickup = (PickUp *)world.CreateObject(ObjectTypes::PICKUP);
				if(pickup){
					pickup->type = PickUp::FILES;
					{ const EnemyDef* _gd = GASLoader::Get().GetEnemyDef("robot"); pickup->quantity = _gd ? _gd->deathDropFiles : 250; }
					pickup->x = x;
					pickup->y = y - 1;
					{ const EnemyDef* _gd2 = GASLoader::Get().GetEnemyDef("robot");
					  pickup->xv = (world.Random() % (2 * (_gd2 ? _gd2->deathDropXVRange : 4) + 1)) - (_gd2 ? _gd2->deathDropXVRange : 4);
					  pickup->yv = -(_gd2 ? _gd2->deathDropYV : 15); }
				}
			}
			if(state_i % 2 == 0 && state_i >= 5){
				Plume * plume = (Plume *)world.CreateObject(ObjectTypes::PLUME);
				if(plume){
					plume->type = 4;
					plume->xv = (rand() % 17) - 8 + (xv * 8);
					plume->yv = (rand() % 17) - 8 + (yv * 8);
					plume->SetPosition(x + (rand() % 39) - 19, y - 5);
					plume->state_i = 0;
				}
			}
			if(state_i == 4 * 2){
				StopAmbience();
				{ const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot"); EmitSound(world, world.resources.soundbank[(rd && !rd->soundDeath.empty()) ? rd->soundDeath : "seekexp1.wav"], 128); }
			}
			collidable = false;
			{ const EnemyDef* _rdd = GASLoader::Get().GetEnemyDef("robot");
			  if(state_i >= (_rdd ? _rdd->deathExplosionDelayTicks : 96)){
				{ const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot"); EmitSound(world, world.resources.soundbank[(rd && !rd->soundDeath.empty()) ? rd->soundDeath : "seekexp1.wav"], 128); }
				Sint8 xvs[] = {-14, 14, -10, 10, -10, 10};
				Sint8 yvs[] = {-25, -25, -10, -10, -5, -5};
				Sint8 ys[] = {0, 0, 0, 0, 0, 0, 0, 0};
				for(int i = 0; i < 6; i++){
					PlasmaProjectile * plasmaprojectile = (PlasmaProjectile *)world.CreateObject(ObjectTypes::PLASMAPROJECTILE);
					if(plasmaprojectile){
						plasmaprojectile->large = false;
						plasmaprojectile->x = x;
						plasmaprojectile->y = y - 1 + ys[i];
						plasmaprojectile->ownerid = id;
						plasmaprojectile->xv = xvs[i];
						plasmaprojectile->yv = yvs[i];
					}
				}
				state = DEAD;
				state_i = -1;
				break;
			  }
			}
			res_bank = 48;
			res_index = state_i / 2;
			if(res_index > 15){
				res_index = 15;
			}
		}break;
		case DEAD:{
			StopAmbience();
			collidable = false;
			virusplanter = 0;
			res_bank = 48;
			res_index = 15;
			if(state_i > 1){
				draw = false;
			}
			if(state_i >= respawnseconds){
				state = NEW;
				x = originalx;
				y = originaly;
				state_i = -1;
				state_warp = GASLoader::Get().player.warpTeleportTick;
				health = maxhealth;
				shield = maxshield;
				break;
			}
			if(world.tickcount % GASLoader::Get().gameengine.ticksPerSecond != 0){
				state_i--;
			}
		}break;
	}

	// BT drives decisions in ASLEEP and WALKING only.
	if (!bt_) InitBT();
	if (bt_ && (state == ASLEEP || state == WALKING)) {
		btctx_.userData = &world;
		btctx_.bbSet("patrol", (bool)patrol);
		btctx_.bbSet("health_pct", maxhealth > 0 ? (float)health / (float)maxhealth : 0.0f);
		btctx_.bbSet("on_ladder", false);
		{
			const char* sn = "unknown";
			switch(state){
				case NEW:       sn = "new"; break;
				case SLEEPING:  sn = "sleeping"; break;
				case ASLEEP:    sn = "asleep"; break;
				case AWAKENING: sn = "awakening"; break;
				case WALKING:   sn = "walking"; break;
				case SHOOTING:  sn = "shooting"; break;
				case DYING:     sn = "dying"; break;
				case DEAD:      sn = "dead"; break;
			}
			btctx_.bbSet("state_name", std::string{sn});
		}
		btctx_.bbSet("dist_to_target", -1);
		bt_->tick(btctx_);
	}

	if(damaging){
		damaging++;
		{ const EnemyDef* _rd = GASLoader::Get().GetEnemyDef("robot");
		  if(damaging > (_rd ? _rd->meleeHitDuration : 24)){ damaging = 0; } }
	}
	state_i++;
}

void Robot::HandleHit(World & world, Uint8 x, Uint8 y, Object & projectile){
	Hittable::HandleHit(*this, world, x, y, projectile);
	if(health == 0 && state != DYING && state != DEAD){
		state = DYING;
		xv = 0;
		state_i = 0;
		Object * owner = world.GetObjectFromId(projectile.ownerid);
		if(owner && owner->type == ObjectTypes::PLAYER){
			Player * player = static_cast<Player *>(owner);
			Peer * peer = player->GetPeer(world);
			if(peer){
				peer->stats.robotskilled++;
			}
		}
	} else if (health > 0 && (state == ASLEEP || state == SLEEPING)) {
		StopAmbience();
		{ const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot"); EmitSound(world, world.resources.soundbank[(rd && !rd->soundMelee.empty()) ? rd->soundMelee : "robotarm.wav"], 128); }
		state = AWAKENING;
		state_i = -1;
	}
}

bool Robot::ImplantVirus(Uint16 teamid){
	if(!virusplanter || virusplanter != teamid){
		virusplanter = teamid;
		return true;
	}
	return false;
}

bool Robot::Look(World & world, Uint8 direction){
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

void Robot::StopAmbience(void){
	if(soundchannel != -1){
		const EnemyDef* _rd = GASLoader::Get().GetEnemyDef("robot");
		Audio::GetInstance().Stop(soundchannel, _rd ? _rd->audioFadeAmbientMs : 800);
	}
	soundchannel = -1;
}

void Robot::Melee(Object & object, World & world){
	damaging = 1;
	const EnemyDef* rd = GASLoader::Get().GetEnemyDef("robot");
	Object damageprojectile(ObjectTypes::FLAREPROJECTILE);
	damageprojectile.healthdamage = rd ? rd->meleeDamageHealth : 60;
	damageprojectile.shielddamage = rd ? rd->meleeDamageShield : 60;
	damageprojectile.ownerid = id;
	object.HandleHit(world, 50, 50, damageprojectile);
}