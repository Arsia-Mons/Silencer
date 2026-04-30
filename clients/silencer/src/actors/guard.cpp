#include "guard.h"
#include "projectile.h"
#include "bodypart.h"
#include "player.h"
#include "robot.h"
#include "laserprojectile.h"
#include "rocketprojectile.h"
#include "pickup.h"
#include "gasloader.h"
#include <math.h>

static const char* ActorDefName(Uint8 weapon) {
	switch (weapon) {
		case 1:  return "guard-laser";
		case 2:  return "guard-rocket";
		default: return "guard-blaster";
	}
}

Guard::Guard() : Object(ObjectTypes::GUARD){
	requiresauthority = true;
	state = NEW;
	state_i = 0;
	res_bank = 59;
	res_index = 0;
	const EnemyDef* g = GASLoader::Get().GetEnemyDef("guard-blaster");
	speed        = g ? g->speed       : 5;
	maxhealth    = g ? g->health      : 25;
	health       = maxhealth;
	maxshield    = g ? g->shield      : 15;
	shield       = maxshield;
	chasing = 0;
	weapon = 0;
	renderpass = 2;
	ishittable = true;
	isbipedal = true;
	isphysical = true;
	{ const EnemyDef* _gd = GASLoader::Get().GetEnemyDef("guard-blaster");
	  snapshotinterval = _gd ? _gd->snapshotInterval : 48; }
	respawnseconds   = g ? g->respawnSeconds : 30;
	patrol = false;
	lastspoke = 0;
	lastshot = 0;
	cooldowntime = g ? g->shotCooldown : 48;
	bt_ = nullptr;
}

void Guard::InitBT(){
	bt_ = BehaviorTreeLibrary::instance().get("guard");
	if(!bt_) return;

	// Shared helper: update chasing id + play alert sound on first detection.
	auto updateChasing = [this](Object* f, World& world){
		if(!chasing){
			chasing = f->id;
			{ const EnemyDef* _ag = GASLoader::Get().GetEnemyDef("guard-blaster");
			  static const EnemyDef _def;
			  if(world.tickcount - lastspoke > (Uint32)(_ag ? _ag->speakCooldownTicks : 240)){
				lastspoke = world.tickcount;
				const std::string* alerts[] = {
					_ag ? &_ag->soundAlert1 : &_def.soundAlert1,
					_ag ? &_ag->soundAlert2 : &_def.soundAlert2,
					_ag ? &_ag->soundAlert3 : &_def.soundAlert3,
					_ag ? &_ag->soundAlert4 : &_def.soundAlert4,
					_ag ? &_ag->soundAlert5 : &_def.soundAlert5
				};
				EmitSound(world, world.resources.soundbank[*alerts[rand() % 5]], 128);
			  }
			}
		} else {
			chasing = f->id;
		}
	};

	// Look(0): standing eye-level forward. Shoots standing or uncrouches.
	btctx_.actions["Look0"] = [this, updateChasing](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		Object* f = Look(world, 0);
		if(!f) return BTResult::Failure;
		ctx.bbSet("target_seen", true);
		updateChasing(f, world);
		if(state == WALKING || state == STANDING || state == LOOKING){
			if(CooledDown(world)){
				state = SHOOTSTANDING; state_i = 0;
			} else if(state == WALKING || state == LOOKING){
				state = STANDING; state_i = 0;
			}
		} else if(state == CROUCHED){
			state = UNCROUCHING; state_i = 0;
		}
		return BTResult::Success;
	};

	// Look(1): low forward ray. Crouch-shoots crouched targets; stand-shoots tall targets.
	btctx_.actions["Look1"] = [this, updateChasing](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		Object* f = Look(world, 1);
		if(!f) return BTResult::Failure;
		ctx.bbSet("target_seen", true);
		updateChasing(f, world);
		if(state == CROUCHED){
			if(CooledDown(world) && (state_hit == 0 || [&]{ const EnemyDef* _g = GASLoader::Get().GetEnemyDef("guard-blaster"); return state_hit % (_g?_g->meleeCycleTicks:32) >= (_g?_g->meleeDelayTicks:10); }())){
				state = SHOOTCROUCHED; state_i = 0;
			}
		} else if(state == WALKING || state == STANDING || state == LOOKING){
			int tsx1, tsy1, tsx2, tsy2;
			f->GetAABB(world.resources, &tsx1, &tsy1, &tsx2, &tsy2);
			// Use hurtbox height to distinguish standing (≥50px) from crouched (<50px).
			// Absolute y comparison fails on sloped terrain.
			if([&]{ const EnemyDef* _g = GASLoader::Get().GetEnemyDef("guard-blaster"); return (tsy2 - tsy1) >= (_g?_g->targetStandingHeight:50); }()){
				// Target is standing height — shoot from standing.
				if(CooledDown(world)){
					state = SHOOTSTANDING; state_i = 0;
				} else if(state == WALKING || state == LOOKING){
					state = STANDING; state_i = 0;
				}
			} else {
				// Short/crouched target — crouch to shoot.
				state = CROUCHING; state_i = 0;
			}
		}
		return BTResult::Success;
	};

	// Look(2): upward ray.
	btctx_.actions["Look2"] = [this, updateChasing](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		Object* f = Look(world, 2);
		if(!f) return BTResult::Failure;
		ctx.bbSet("target_seen", true);
		updateChasing(f, world);
		if(state == WALKING || state == STANDING || state == LOOKING){
			if(CooledDown(world)){ state = SHOOTUP; state_i = 0; }
		}
		return BTResult::Success;
	};

	// Look(3): downward ray.
	btctx_.actions["Look3"] = [this, updateChasing](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		Object* f = Look(world, 3);
		if(!f) return BTResult::Failure;
		ctx.bbSet("target_seen", true);
		updateChasing(f, world);
		if(state == WALKING || state == STANDING || state == LOOKING){
			if(CooledDown(world)){ state = SHOOTDOWN; state_i = 0; }
		}
		return BTResult::Success;
	};

	// Look(4): up-angle ray.
	btctx_.actions["Look4"] = [this, updateChasing](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		Object* f = Look(world, 4);
		if(!f) return BTResult::Failure;
		ctx.bbSet("target_seen", true);
		updateChasing(f, world);
		if(state == WALKING || state == STANDING || state == LOOKING){
			if(CooledDown(world)){ state = SHOOTUPANGLE; state_i = 0; }
		}
		return BTResult::Success;
	};

	// Look(5): down-angle ray. Too-close targets: mark seen + Failure so Chase runs.
	btctx_.actions["Look5"] = [this, updateChasing](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		Object* f = Look(world, 5);
		if(!f) return BTResult::Failure;
		if(f->type == ObjectTypes::PLAYER){
			Player* p = static_cast<Player*>(f);
			if(p && abs(p->x - x) < GASLoader::Get().GetEnemyDef("guard-blaster")->chaseRangeClose){
				// Too close for this angle — mark seen so leaf_uncrouch won't fire, then chase.
				ctx.bbSet("target_seen", true);
				updateChasing(f, world);
				return BTResult::Failure;
			}
		}
		ctx.bbSet("target_seen", true);
		updateChasing(f, world);
		if(state == WALKING || state == STANDING || state == LOOKING){
			if(CooledDown(world)){ state = SHOOTDOWNANGLE; state_i = 0; }
		}
		return BTResult::Success;
	};

	// UncrouchIdle: uncrouch when guard has lost sight of target while crouched.
	btctx_.actions["UncrouchIdle"] = [this](BTContext& ctx) -> BTResult {
		if(ctx.bb<bool>("target_seen")) return BTResult::Failure;
		if(state == CROUCHED){ state = UNCROUCHING; state_i = 0; return BTResult::Running; }
		if(state == UNCROUCHING) return BTResult::Running;
		return BTResult::Failure;
	};

	// Chase: walk toward the chasing target. Only when patrol=true.
	btctx_.actions["Chase"] = [this](BTContext& ctx) -> BTResult {
		World& world = *static_cast<World*>(ctx.userData);
		if(!chasing) return BTResult::Failure;
		if(!patrol) return BTResult::Failure; // stay at post
		Object* obj = world.GetObjectFromId(chasing);
		if(!obj){ chasing = 0; return BTResult::Failure; }
		if(obj->type == ObjectTypes::PLAYER){
			Player* p = static_cast<Player*>(obj);
			if(p->InBase(world) || p->IsInvisible(world)){ chasing = 0; return BTResult::Failure; }
		}
		if(state == STANDING || state == WALKING){
			if(abs(obj->x - x) <= GASLoader::Get().GetEnemyDef("guard-blaster")->chaseRangeMax && abs(obj->x - x) > GASLoader::Get().GetEnemyDef("guard-blaster")->chaseRangeStop){
				mirrored = (obj->x < x);
			} else if(abs(obj->x - x) > GASLoader::Get().GetEnemyDef("guard-blaster")->chaseRangeMax){
				state = WALKING;
				mirrored = (obj->x < x);
			} else {
				state = WALKING;
			}
			Platform* ladder = world.map.TestAABB(x - abs(xv), y, x + abs(xv), y, Platform::LADDER);
			if(ladder){
				Uint32 center = ((ladder->x2 - ladder->x1) / 2) + ladder->x1;
				if(abs(signed(center) - x) <= abs(ceil(float(xv)))){
					if(ladder->y2 == obj->y && y != obj->y && ladder->y2 > y){
						{ const EnemyDef* _gls = GASLoader::Get().GetEnemyDef("guard-blaster"); x = center; yv = _gls ? _gls->ladderClimbSpeed : 5; state = LADDER; state_i = 0; }
					}
					if(ladder->y1 == obj->y && y != obj->y && ladder->y1 < y){
						{ const EnemyDef* _gls = GASLoader::Get().GetEnemyDef("guard-blaster"); x = center; yv = -(_gls ? _gls->ladderClimbSpeed : 5); state = LADDER; state_i = 0; }
					}
				}
			}
		}
		return BTResult::Running;
	};

	btctx_.actions["Patrol"] = [this](BTContext&) -> BTResult {
		if(state == STANDING || state == LOOKING){ state = WALKING; state_i = 0; }
		return BTResult::Success;
	};

	// SearchAndReturn: non-patrol guard that was alerted (chasing set or bt_walk_ticks_ > 0).
	// Searches for 600 ticks (10s) oriented toward last known target, then walks back to post.
	btctx_.actions["SearchAndReturn"] = [this](BTContext& ctx) -> BTResult {
		if (patrol) return BTResult::Failure;
		if (bt_walk_ticks_ == 0 && !chasing) return BTResult::Failure;
		World& world = *static_cast<World*>(ctx.userData);
		if (state == STANDING || state == LOOKING) { state = WALKING; state_i = 0; }
		{ const EnemyDef* _sg = GASLoader::Get().GetEnemyDef("guard-blaster");
		  const int _stout = _sg && _sg->searchTimeoutTicks > 0 ? _sg->searchTimeoutTicks : 600;
		  if (bt_walk_ticks_ < _stout && chasing) {
			// Search phase: orient toward target when not on a ladder and not too close.
			if (state != LADDER) {
				Object* obj = world.GetObjectFromId(chasing);
				if (obj && obj->IsAlive()) {
					// If target slipped into base or is now untargetable (disguised, invisible),
					// forget them — guard searches blindly then returns to post.
					if (obj->type == ObjectTypes::PLAYER) {
						Player* p = static_cast<Player*>(obj);
						if (p->InBase(world) || p->IsInvisible(world) || !ShouldTarget(*obj, world)) {
							chasing = 0;
							return BTResult::Running;
						}
					}
					int dist = abs(signed(obj->x) - signed(x));
					if (dist > ([]{ const EnemyDef* _g = GASLoader::Get().GetEnemyDef("guard-blaster"); return _g ? _g->chaseRangeStop : 80; }())) {
						mirrored = (obj->x < x); // orient toward target
					}
					// <=80px: keep current direction to avoid oscillation
					// Climb ladder toward target if on a meaningfully different level
					int ydiff = signed(obj->y) - signed(y);
					{ const EnemyDef* _gg = GASLoader::Get().GetEnemyDef("guard-blaster");
					if(abs(ydiff) > (_gg?_gg->ladderYThreshold:48) && bt_ladder_cooldown_ == 0){
						Platform* ladder = world.map.TestAABB(x - 8, y, x + 8, y, Platform::LADDER);
						if(ladder && state == WALKING){
							Uint32 center = ((ladder->x2 - ladder->x1) / 2) + ladder->x1;
							if(abs(signed(center) - signed(x)) <= (_gg?_gg->ladderXTolerance:8)){
								if(ydiff < 0 && signed(ladder->y1) < signed(y)){
									// player above, ladder goes up
									x = center; yv = -(_gg ? _gg->ladderClimbSpeed : 5); state = LADDER; state_i = 0;
									{ const EnemyDef* gd = GASLoader::Get().GetEnemyDef("guard-blaster"); bt_ladder_cooldown_ = gd ? gd->ladderCooldown : 120; }
								} else if(ydiff > 0 && signed(ladder->y2) > signed(y)){
									// player below, ladder goes down
									x = center; yv = (_gg ? _gg->ladderClimbSpeed : 5); state = LADDER; state_i = 0;
									{ const EnemyDef* gd = GASLoader::Get().GetEnemyDef("guard-blaster"); bt_ladder_cooldown_ = gd ? gd->ladderCooldown : 120; }
								}
							}
						}
					}
					} // _gg scope
				} else {
					chasing = 0; // target gone or dead
				}
			}
			return BTResult::Running;
		}
		} // _sg scope
		// Return-to-post phase: face toward spawn, climb ladders back if needed
		if (state == STANDING || state == LOOKING) { state = WALKING; state_i = 0; }
		{ const EnemyDef* _ggr = GASLoader::Get().GetEnemyDef("guard-blaster");
		if (abs(signed(x) - signed(originalx)) <= (_ggr?_ggr->patrolReturnProximity:20)) {
			chasing = 0;
			bt_walk_ticks_ = 0;
			state = STANDING;
			state_i = -1;
			mirrored = originalmirrored;
			return BTResult::Success;
		}
		if (state != LADDER) {
			mirrored = (signed(originalx) < signed(x));
			// Climb ladders back to original level
			int ydiff = signed(originaly) - signed(y);
			if(abs(ydiff) > (_ggr?_ggr->ladderYThreshold:48) && bt_ladder_cooldown_ == 0){
				Platform* ladder = world.map.TestAABB(x - 8, y, x + 8, y, Platform::LADDER);
				if(ladder && state == WALKING){
					Uint32 center = ((ladder->x2 - ladder->x1) / 2) + ladder->x1;
					if(abs(signed(center) - signed(x)) <= (_ggr?_ggr->ladderXTolerance:8)){
						if(ydiff < 0 && signed(ladder->y1) < signed(y)){
							x = center; yv = -(_ggr ? _ggr->ladderClimbSpeed : 5); state = LADDER; state_i = 0;
							{ const EnemyDef* gd = GASLoader::Get().GetEnemyDef("guard-blaster"); bt_ladder_cooldown_ = gd ? gd->ladderCooldown : 120; }
						} else if(ydiff > 0 && signed(ladder->y2) > signed(y)){
							x = center; yv = (_ggr ? _ggr->ladderClimbSpeed : 5); state = LADDER; state_i = 0;
							{ const EnemyDef* gd = GASLoader::Get().GetEnemyDef("guard-blaster"); bt_ladder_cooldown_ = gd ? gd->ladderCooldown : 120; }
						}
					}
				}
			}
		}
		} // _ggr scope
		return BTResult::Running;
	};

	btctx_.actions["Stand"] = [this](BTContext&) -> BTResult {
		return BTResult::Success;
	};
}

void Guard::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state, old);
	data.Serialize(write, state_i, old);
	data.Serialize(write, chasing, old);
	data.Serialize(write, weapon, old);
	data.Serialize(write, patrol, old);
}

void Guard::Tick(World & world){
	// 62:0-19 climb ladder
	// 63:0-3 hit
	// 154:0-9 shoot up
	// 155:0-8 shoot down
	// 156:0-8 shoot up/right
	// 157:0-8 shoot down/right
	// 158:0-9 crouch
	// 159:0-8 crouch shoot
	// 196:0-8 ladder shoot up
	// 197:0-8 ladder shoot down
	Hittable::Tick(*this, world);
	Bipedal::Tick(*this, world);

	if(!bt_) InitBT();

	// BT alert timer: counts up while WALKING and chasing, resets when calm
	if (bt_) {
		if (state == WALKING) bt_walk_ticks_++;
		else if (!chasing) bt_walk_ticks_ = 0;
		if (bt_ladder_cooldown_ > 0) bt_ladder_cooldown_--;
	}

	// Original priority interrupt for combat — runs every tick, exact semantics preserved
	Object* found = nullptr;
	if(state != DYING && state != DEAD && state != DYINGEXPLODE){
		if(bt_){
			btctx_.userData = &world;
			btctx_.bbSet("patrol", (bool)patrol);
			btctx_.bbSet("target_seen", false);
			bt_->tick(btctx_);
		} else {
		do{
			if((found = Look(world, 0))){
			if(world.debugoverlay) fprintf(stderr, "[guard#%u] Look(0) HIT  state=%d state_i=%d\n", id, state, state_i);
			if(state == WALKING || state == STANDING || state == LOOKING){
				if(CooledDown(world)){
					if(world.debugoverlay) fprintf(stderr, "[guard#%u] -> SHOOTSTANDING\n", id);
					state = SHOOTSTANDING;
					state_i = 0;
				}else{
					// Can see player but on cooldown — stop moving so LOS is maintained
					if(state == WALKING || state == LOOKING){
						state = STANDING;
						state_i = 0;
					}
					if(world.debugoverlay) fprintf(stderr, "[guard#%u] Look(0) HIT cooldown state=%d\n", id, state);
				}
			}else
			if(state == CROUCHED){
				if(world.debugoverlay) fprintf(stderr, "[guard#%u] -> UNCROUCHING\n", id);
				state = UNCROUCHING;
				state_i = 0;
			}
			break;
		}
			if(world.debugoverlay) fprintf(stderr, "[guard#%u] Look(0) MISS state=%d state_i=%d\n", id, state, state_i);
			if((found = Look(world, 1))){
				if(world.debugoverlay) fprintf(stderr, "[guard#%u] Look(1) HIT  state=%d state_i=%d\n", id, state, state_i);
				if(state == CROUCHED){
					if(CooledDown(world) && (state_hit == 0 || [&]{ const EnemyDef* _g = GASLoader::Get().GetEnemyDef("guard-blaster"); return state_hit % (_g?_g->meleeCycleTicks:32) >= (_g?_g->meleeDelayTicks:10); }())){
						state = SHOOTCROUCHED;
						state_i = 0;
					}
				}else
				if(state == WALKING || state == STANDING || state == LOOKING){
					// Use hurtbox height to distinguish standing (≥50px) from crouched (<50px).
					int tsx1, tsy1, tsx2, tsy2;
					found->GetAABB(world.resources, &tsx1, &tsy1, &tsx2, &tsy2);
					if([&]{ const EnemyDef* _g = GASLoader::Get().GetEnemyDef("guard-blaster"); return (tsy2 - tsy1) >= (_g?_g->targetStandingHeight:50); }()){
						// Standing-height target: shoot from standing position
						if(CooledDown(world)){
							state = SHOOTSTANDING;
							state_i = 0;
						} else {
							if(state == WALKING || state == LOOKING){
								state = STANDING;
								state_i = 0;
							}
						}
					} else {
						// Short/crouched target: crouch to shoot
						state = CROUCHING;
						state_i = 0;
					}
				}
				break;
			}
			if((found = Look(world, 2))){
				if(state == WALKING || state == STANDING || state == LOOKING){
					if(CooledDown(world)){
						state = SHOOTUP;
						state_i = 0;
					}
				}
				break;
			}
			if((found = Look(world, 3))){
				if(state == WALKING || state == STANDING || state == LOOKING){
					if(CooledDown(world)){
						state = SHOOTDOWN;
						state_i = 0;
					}
				}
				break;
			}
			if((found = Look(world, 4))){
				if(state == WALKING || state == STANDING || state == LOOKING){
					if(CooledDown(world)){
						state = SHOOTUPANGLE;
						state_i = 0;
					}
				}
				break;
			}
			if((found = Look(world, 5))){
				Player* player = static_cast<Player*>(found);
				if(player){
					const EnemyDef* _gcp = GASLoader::Get().GetEnemyDef("guard-blaster");
					if(abs(player->x - x) < (_gcp ? _gcp->chaseProximityX : 60)){
						break;
					}
				}
				if(state == WALKING || state == STANDING || state == LOOKING){
					if(CooledDown(world)){
						state = SHOOTDOWNANGLE;
						state_i = 0;
					}
				}
				break;
			}
		}while(0);
		if(found){
			if(!chasing){
				chasing = found->id;
				{ const EnemyDef* _ag = GASLoader::Get().GetEnemyDef("guard-blaster");
				  static const EnemyDef _def;
				  if(world.tickcount - lastspoke > (Uint32)(_ag ? _ag->speakCooldownTicks : 240)){
					lastspoke = world.tickcount;
					const std::string* alerts[] = {
						_ag ? &_ag->soundAlert1 : &_def.soundAlert1,
						_ag ? &_ag->soundAlert2 : &_def.soundAlert2,
						_ag ? &_ag->soundAlert3 : &_def.soundAlert3,
						_ag ? &_ag->soundAlert4 : &_def.soundAlert4,
						_ag ? &_ag->soundAlert5 : &_def.soundAlert5
					};
					EmitSound(world, world.resources.soundbank[*alerts[rand() % 5]], 128);
				  }
				}
			}
		}else{
			if(state == CROUCHED){
				state = UNCROUCHING;
				state_i = 0;
			}
		}
		} // end else (!bt_)
	}

	switch(state){
		case NEW:{
			draw = true;
			currentplatformid = 0;
			if(FindCurrentPlatform(*this, world)){
				state = STANDING;
				state_i = -1;
				break;
			}
		}break;
		case STANDING:{
			yv = 0;
			res_bank = 59;
			res_index = 0;
			{ const EnemyDef* _gsd = GASLoader::Get().GetEnemyDef("guard-blaster");
			  if(state_i >= (_gsd ? _gsd->standingDurationTicks : 48)){
				if(patrol && world.Random() % 3 == 0){
					state = WALKING;
				}else{
					state = LOOKING;
				}
				state_i = -1;
			}
			}
		}break;
		case CROUCHING:{
			xv = 0;
			res_bank = 158;
			res_index = state_i;
			if(state_i >= 9){
				state = CROUCHED;
				state_i = -1;
				break;
			}
		}break;
		case CROUCHED:{
			xv = 0;
			res_bank = 158;
			res_index = 9;
		}break;
		case SHOOTCROUCHED:{
			xv = 0;
			if(state_i == 6){
				Fire(world, 1);
			}
			if((state_i) == 9){
				state_i = 13;
			}
			if(state_i >= 16){
				state = CROUCHED;
				state_i = -1;
				break;
			}
			res_bank = 159;
			if(state_i > 8){
				res_index = 8 - ((state_i) - 8);
			}else{
				res_index = state_i;
			}
		}break;
		case UNCROUCHING:{
			xv = 0;
			res_bank = 158;
			res_index = 9 - state_i;  // reverse of CROUCHING: animate from crouched back to standing
			if(state_i >= 9){
				state = STANDING;
				state_i = -1;
				break;
			}
		}break;
		case LOOKING:{
			if(!bt_ && !found){
				chasing = 0;
			}
			if(state_i == 0 && Look(world, 10)){
				mirrored = !mirrored;
			}
			if(state_i >= 6 * 4){
				state = STANDING;
				state_i = -1;
				break;
			}
			res_bank = 69;
			res_index = state_i / 4;
		}break;
		case WALKING:{
			res_bank = 60;
			res_index = state_i % 19;
			xv = mirrored ? -speed : speed;
			FollowGround(*this, world, xv);
			if(DistanceToEnd(*this, world) <= world.minwalldistance){
				mirrored = !mirrored;
			}
			// play per-frame sounds defined in actordefs/guard.json
			{
				auto it = world.resources.actordefs.find(ActorDefName(weapon));
				if(it != world.resources.actordefs.end()){
					auto* seq = it->second.GetSequence("WALKING");
					std::string snd; int vol;
					if(seq && seq->GetFrameSoundByIndex(state_i % 19, snd, vol)){
						EmitSound(world, world.resources.soundbank[snd], vol);
					}
				}
			}
			{ const EnemyDef* _gwd = GASLoader::Get().GetEnemyDef("guard-blaster");
			  if(state_i >= (_gwd ? _gwd->walkingDurationTicks : 240)){
				state = LOOKING;
				state_i = -1;
				break;
			  }
			}
		}break;
		case SHOOTSTANDING:{
			if(state_i == 7){
				Fire(world, 0);
			}
			if((state_i) == 10){
				state_i = 13;
			}
			if(state_i >= 18){
				state = STANDING;
				state_i = -1;
				break;
			}
			res_bank = 61;
			if(state_i > 9){
				res_index = 9 - ((state_i) - 9);
			}else{
				res_index = state_i;
			}
		}break;
		case SHOOTUP:{
			if(state_i == 7){
				Fire(world, 2);
			}
			if((state_i) == 10){
				state_i = 13;
			}
			if(state_i >= 18){
				state = STANDING;
				state_i = -1;
				break;
			}
			res_bank = 154;
			if(state_i > 9){
				res_index = 9 - ((state_i) - 9);
			}else{
				res_index = state_i;
			}
		}break;
		case SHOOTDOWN:{
			if(state_i == 6){
				Fire(world, 3);
			}
			if((state_i) == 9){
				state_i = 13;
			}
			if(state_i >= 16){
				state = STANDING;
				state_i = -1;
				break;
			}
			res_bank = 155;
			if(state_i > 8){
				res_index = 8 - ((state_i) - 8);
			}else{
				res_index = state_i;
			}
		}break;
		case SHOOTUPANGLE:{
			if(state_i == 6){
				Fire(world, 4);
			}
			if((state_i) == 9){
				state_i = 13;
			}
			if(state_i >= 16){
				state = STANDING;
				state_i = -1;
				break;
			}
			res_bank = 156;
			if(state_i > 8){
				res_index = 8 - ((state_i) - 8);
			}else{
				res_index = state_i;
			}
		}break;
		case SHOOTDOWNANGLE:{
			if(state_i == 6){
				Fire(world, 5);
			}
			if((state_i) == 9){
				state_i = 13;
			}
			if(state_i >= 16){
				state = STANDING;
				state_i = -1;
				break;
			}
			res_bank = 157;
			if(state_i > 8){
				res_index = 8 - ((state_i) - 8);
			}else{
				res_index = state_i;
			}
		}break;
		case LADDER:{
			xv = 0;
			int ye = yv;
			int xe = xv;
			Platform * platform = world.map.TestIncr(x, y, x, y, &xe, &ye, Platform::RECTANGLE | Platform::STAIRSUP | Platform::STAIRSDOWN);
			Platform * ladder = world.map.TestAABB(x, y + yv, x, y + yv, Platform::LADDER);
			if(!ladder){
				if(platform){
					currentplatformid = platform->id;
					y = platform->XtoY(x);
					state = STANDING;
					state_i = -1;
					break;
				}else{
					yv = -yv;
				}
			}
			if(state_hit == 0 || [&]{ const EnemyDef* _g = GASLoader::Get().GetEnemyDef("guard-blaster"); return state_hit % (_g?_g->meleeCycleTicks:32) >= (_g?_g->meleeDelayTicks:10); }()){
				if(Look(world, 6) && CooledDown(world)){
					state = SHOOTLADDERUP;
					state_i = -1;
					break;
				}
				if(Look(world, 7) && CooledDown(world)){
					state = SHOOTLADDERDOWN;
					state_i = -1;
					break;
				}
			}
			if(state_i >= 20){
				state_i = 0;
			}
			y += yv;
			res_bank = 62;
			res_index = state_i;
		}break;
		case SHOOTLADDERUP:{
			yv = 0;
			if(state_i == 6){
				Fire(world, 6);
			}
			if((state_i) == 9){
				state_i = 13;
			}
			if(state_i >= 16){
				state = LADDER;
				{ const EnemyDef* _gls = GASLoader::Get().GetEnemyDef("guard-blaster"); yv = -(_gls ? _gls->ladderClimbSpeed : 5); }
				state_i = -1;
				break;
			}
			res_bank = 196;
			if(state_i > 8){
				res_index = 8 - ((state_i) - 8);
			}else{
				res_index = state_i;
			}
		}break;
		case SHOOTLADDERDOWN:{
			yv = 0;
			if(state_i == 6){
				Fire(world, 7);
			}
			if((state_i) == 9){
				state_i = 13;
			}
			if(state_i >= 16){
				state = LADDER;
				{ const EnemyDef* _gls = GASLoader::Get().GetEnemyDef("guard-blaster"); yv = (_gls ? _gls->ladderClimbSpeed : 5); }
				state_i = -1;
				break;
			}
			res_bank = 197;
			if(state_i > 8){
				res_index = 8 - ((state_i) - 8);
			}else{
				res_index = state_i;
			}
		}break;
		case DYING:{
			if(state_i == 0){
				const EnemyDef* gd = GASLoader::Get().GetEnemyDef(ActorDefName(weapon));
				switch(rand() % 3){
					case 0:
						EmitSound(world, world.resources.soundbank[(gd && !gd->soundHurt1.empty()) ? gd->soundHurt1 : "groan2.wav"], 128);
						break;
					case 1:
						EmitSound(world, world.resources.soundbank[(gd && !gd->soundHurt2.empty()) ? gd->soundHurt2 : "groan2a.wav"], 128);
						break;
					case 2:
						EmitSound(world, world.resources.soundbank[(gd && !gd->soundHurt3.empty()) ? gd->soundHurt3 : "grunt2a.wav"], 128);
						break;
				}
			}
			collidable = false;
			if(state_i >= 10){
				state = DEAD;
				state_i = -1;
				break;
			}
			res_bank = 64;
			res_index = state_i;
		}break;
		case HIT:{
			res_bank = 63;
			res_index = state_i;
			if(state_i >= 3){
				// Non-patrol guard that was alerted: go to WALKING to start SearchAndReturn
				if(bt_ && !patrol && chasing){
					state = WALKING;
				} else {
					state = STANDING;
				}
				state_i = -1;
				break;
			}
		}break;
		case DYINGEXPLODE:{
			draw = false;
			res_index = 0xFF;
			state = DEAD;
			state_i = -1;
			break;
		}break;
		case DEAD:{
			chasing = 0;
			collidable = false;
			if(state_i > 1){
				draw = false;
			}
			if(state_i >= respawnseconds){
				x = originalx;
				y = originaly;
				mirrored = originalmirrored;
				state = NEW;
				state_i = -1;
				{ const EnemyDef* _gd = GASLoader::Get().GetEnemyDef(ActorDefName(weapon));
				  state_warp = _gd ? _gd->warpTeleportTick : GASLoader::Get().player.warpTeleportTick; }
				health = maxhealth;
				shield = maxshield;
				break;
			}
			if(world.tickcount % GASLoader::Get().gameengine.ticksPerSecond != 0){
				state_i--;
			}
		}break;
	}
	if(!bt_ && chasing){
		Object * object = world.GetObjectFromId(chasing);
		if(object){
			if(object->type == ObjectTypes::PLAYER){
				Player * player = static_cast<Player *>(object);
				if(player->InBase(world) || player->IsInvisible(world)){
					chasing = 0;
				}
			}
			if(state == STANDING || state == WALKING){
				if(abs(object->x - x) <= GASLoader::Get().GetEnemyDef("guard-blaster")->chaseRangeMax && abs(object->x - x) > GASLoader::Get().GetEnemyDef("guard-blaster")->chaseRangeStop){
					if(object->x > x){
						mirrored = false;
					}else{
						mirrored = true;
					}
				}else
					if(abs(object->x - x) > GASLoader::Get().GetEnemyDef("guard-blaster")->chaseRangeMax){
						state = WALKING;
						if(object->x > x){
							mirrored = false;
						}else{
							mirrored = true;
						}
					}else{
						state = WALKING;
					}
				Platform * ladder = world.map.TestAABB(x - abs(xv), y, x + abs(xv), y, Platform::LADDER);
				if(ladder){
					Uint32 center = ((ladder->x2 - ladder->x1) / 2) + ladder->x1;
					if(abs(signed(center) - x) <= abs(ceil(float(xv)))){
						if(ladder->y2 == object->y && y != object->y && ladder->y2 > y){
							x = center;
							{ const EnemyDef* _gls = GASLoader::Get().GetEnemyDef("guard-blaster"); yv = _gls ? _gls->ladderClimbSpeed : 5; }
							state = LADDER;
							state_i = 0;
						}
						if(ladder->y1 == object->y && y != object->y && ladder->y1 < y){
							x = center;
							{ const EnemyDef* _gls = GASLoader::Get().GetEnemyDef("guard-blaster"); yv = -(_gls ? _gls->ladderClimbSpeed : 5); }
							state = LADDER;
							state_i = 0;
						}
					}
				}
			}
		}
	}
	state_i++;
}

void Guard::HandleHit(World & world, Uint8 x, Uint8 y, Object & projectile){
	Hittable::HandleHit(*this, world, x, y, projectile);
	float xpcnt = -((x - 50) / 50.0) * (mirrored ? -1 : 1);
	if(state == WALKING || state == STANDING || state == SHOOTSTANDING || state == SHOOTUP || state == SHOOTUPANGLE || state == SHOOTDOWN || state == SHOOTDOWNANGLE){
		state = HIT;
		state_i = 0;
	}
	// Non-patrol guard hit by player: alert so SearchAndReturn activates
	if(bt_ && !patrol && health > 0 && !chasing){
		Object* owner = world.GetObjectFromId(projectile.ownerid);
		if(owner && owner->type == ObjectTypes::PLAYER){
			chasing = owner->id;
		}
	}
	if(health == 0 && state != DYING && state != DYINGEXPLODE && state != DEAD){
		state = DYING;
		state_i = 0;
		if(weapon != 0){
			PickUp * pickup = (PickUp *)world.CreateObject(ObjectTypes::PICKUP);
			if(pickup){
				if(weapon == 2){
					pickup->type = PickUp::ROCKETAMMO;
					{ const EnemyDef* _grd = GASLoader::Get().GetEnemyDef("guard-rocket"); pickup->quantity = _grd ? _grd->ammoDropQuantity : 3; }
				}else
				if(weapon == 1){
					pickup->type = PickUp::LASERAMMO;
					{ const EnemyDef* _gld = GASLoader::Get().GetEnemyDef("guard-laser"); pickup->quantity = _gld ? _gld->ammoDropQuantity : 5; }
				}
				pickup->x = Guard::x;
				pickup->y = Guard::y - 1;
				pickup->xv = (world.Random() % 9) - 4;
				pickup->yv = -15;
			}
		}
		Object * owner = world.GetObjectFromId(projectile.ownerid);
		if(owner && owner->type == ObjectTypes::PLAYER){
			Player * player = static_cast<Player *>(owner);
			Peer * peer = player->GetPeer(world);
			if(peer){
				peer->stats.guardskilled++;
			}
		}
	}
	xv = projectile.moveamount * xpcnt;
	if(state != LADDER && state != SHOOTLADDERUP && state != SHOOTLADDERDOWN){
		FollowGround(*this, world, xv);
	}
	/*if(x < 50){
		xv = abs(xv) * (mirrored ? -1 : 1);
	}else{
		xv = -abs(speed) * (mirrored ? -1 : 1);
	}*/
	if(projectile.type == ObjectTypes::ROCKETPROJECTILE || projectile.type == ObjectTypes::PLASMAPROJECTILE){
		if(health == 0 && state != DYINGEXPLODE){
			state = DYINGEXPLODE;
			world.Explode(*this, 8, xpcnt);
		}
	}
}


Object * Guard::Look(World & world, Uint8 direction){
	// directions:
	// 0: standing and forward
	// 1: crouched and forward
	// 2: up
	// 3: down
	// 4: up angled
	// 5: down angled
	// 6: on ladder and down
	// 7: on ladder and up
	// 10: standing and backward
	std::vector<Uint8> types;
	types.push_back(ObjectTypes::PLAYER);
	types.push_back(ObjectTypes::ROBOT);
	types.push_back(ObjectTypes::FIXEDCANNON);
	Sint16 y1 = 0;
	Sint16 y2 = 0;
	Sint16 x1 = 0;
	Sint16 x2 = 0;
	const EnemyDef* gd = GASLoader::Get().GetEnemyDef(ActorDefName(weapon));
	bool usedGAS = false;
	if (gd && !gd->lookBoxes.empty()) {
		auto it = gd->lookBoxes.find(direction);
		if (it != gd->lookBoxes.end()) {
			x1 = it->second.x1;
			x2 = it->second.x2;
			y1 = it->second.y1;
			y2 = it->second.y2;
			usedGAS = true;
		}
	}
	if (!usedGAS) {
		switch(direction){
			case 0: y1=-55; y2=y1; x1=70; x2=200; break;
			case 1: y1=-37; y2=y1; x1=70; x2=200; break;
			case 2: x1=2; x2=2; y1=-150; y2=-300; break;
			case 3: x1=12; x2=12; y1=50; y2=200; break;
			case 4: x1=20; y1=-82; x2=x1+200; y2=y1-200; break;
			case 5: x1=28; y1=-30; x2=x1+200; y2=y1+200; break;
			case 6: x1=4; x2=4; y1=-150; y2=-300; break;
			case 7: x1=11; x2=11; y1=50; y2=200; break;
			case 10: y1=-55; y2=y1; x1=-100; x2=0; break;
		}
	}
	x1 *= (mirrored ? -1 : 1);
	x2 *= (mirrored ? -1 : 1);
	/*if(signed(x) + x1 < 0){
		x1 = -x;
	}
	if(signed(x) + x2 < 0){
		x2 = -x;
	}
	if(signed(y) + y1 < 0){
		y2 = -y;
	}
	if(signed(y) + y2 < 0){
		y2 = -y;
	}*/
	if(y1 == y2 || x1 == x2){
		bool target = false;
		std::vector<Object *> objects = world.TestAABB(x + x1, y + y1, x + x2, y + y2, types);
		for(std::vector<Object *>::iterator it = objects.begin(); it != objects.end(); it++){
			if(ShouldTarget(*(*it), world)){
				target = true;
				break;
			}
		}
		if(target){
			int xv2 = x2 - x1;
			int yv2 = y2 - y1;
			Object * object = world.TestIncr(x + x1, y + y1 - 1, x + x1, y + y1, &xv2, &yv2, types);
			if(object){
				if(!world.map.TestIncr(x + x1, y + y1 - 1, x + x1, y + y1, &xv2, &yv2, Platform::STAIRSDOWN | Platform::STAIRSDOWN | Platform::RECTANGLE, 0, true)){
					if(world.debugoverlay) world.debuglines.push_back({x+x1, y+y1, x+x2, y+y2, 68}); // green = hit
					return object;
				}
			}
		}
		if(world.debugoverlay) world.debuglines.push_back({x+x1, y+y1, x+x2, y+y2, 40}); // red = miss
	}else{
		int xv2 = x2 - x1;
		int yv2 = y2 - y1;
		Object * object = world.TestIncr(x + x1, y + y1 - 1, x + x1, y + y1, &xv2, &yv2, types);
		if(object && ShouldTarget(*object, world)){
			if(!world.map.TestIncr(x + x1, y + y1 - 1, x + x1, y + y1, &xv2, &yv2, Platform::STAIRSDOWN | Platform::STAIRSDOWN | Platform::RECTANGLE, 0, true)){
				if(world.debugoverlay) world.debuglines.push_back({x+x1, y+y1, x+x2, y+y2, 68}); // green = hit
				return object;
			}
		}
		if(world.debugoverlay) world.debuglines.push_back({x+x1, y+y1, x+x2, y+y2, 40}); // red = miss
	}
	return 0;
}

void Guard::Fire(World & world, Uint8 direction){
	Object * projectile = 0;
	switch(weapon){
		case 0:{
			projectile = world.CreateObject(ObjectTypes::BLASTERPROJECTILE);
		}break;
		case 1:{
			projectile = world.CreateObject(ObjectTypes::LASERPROJECTILE);
		}break;
		case 2:{
			projectile = world.CreateObject(ObjectTypes::ROCKETPROJECTILE);
			if(projectile){
				RocketProjectile * rocketprojectile = static_cast<RocketProjectile *>(projectile);
				rocketprojectile->FromSecurity();
			}
		}break;
		case 3:{
			projectile = world.CreateObject(ObjectTypes::FLAMERPROJECTILE);
		}break;
	}
	if(projectile){
		projectile->ownerid = id;
		projectile->mirrored = mirrored;
		switch(direction){
			case 0:{
				projectile->x = x + ((mirrored ? -1 : 1) * (36 + projectile->emitoffset));
				projectile->y = y - 55;
				projectile->xv = projectile->velocity * (mirrored ? -1 : 1);
			}break;
			case 1:{
				projectile->x = x + ((mirrored ? -1 : 1) * (36 + projectile->emitoffset));
				projectile->y = y - 37;
				projectile->xv = projectile->velocity * (mirrored ? -1 : 1);
			}break;
			case 2:{
				projectile->x = x + ((mirrored ? -1 : 1) * 2);
				projectile->y = y - 95 - projectile->emitoffset;
				projectile->yv = -projectile->velocity;
			}break;
			case 3:{
				projectile->x = x + ((mirrored ? -1 : 1) * 12);
				projectile->y = y - 5 + projectile->emitoffset;
				projectile->yv = projectile->velocity;
			}break;
			case 4:{
				projectile->x = x + ((mirrored ? -1 : 1) * (20 + (projectile->emitoffset * 0.70710678118655)));
				projectile->y = y - 82 - (projectile->emitoffset * 0.70710678118655);
				projectile->xv = (mirrored ? -1 : 1) * projectile->velocity * 0.70710678118655;
				projectile->yv = -projectile->velocity * 0.70710678118655;
			}break;
			case 5:{
				projectile->x = x + ((mirrored ? -1 : 1) * (28 + (projectile->emitoffset * 0.70710678118655)));
				projectile->y = y - 30 + (projectile->emitoffset * 0.70710678118655);
				projectile->xv = (mirrored ? -1 : 1) * projectile->velocity * 0.70710678118655;
				projectile->yv = projectile->velocity * 0.70710678118655;
			}break;
			case 6:{
				projectile->x = x + ((mirrored ? -1 : 1) * 4);
				projectile->y = y - 95 - projectile->emitoffset;
				projectile->yv = -projectile->velocity;
			}break;
			case 7:{
				projectile->x = x + ((mirrored ? -1 : 1) * 11);
				projectile->y = y - 10 + projectile->emitoffset;
				projectile->yv = projectile->velocity;
			}break;
		}
	}
}

bool Guard::CooledDown(World & world){
	if(world.tickcount - lastshot >= cooldowntime){
		lastshot = world.tickcount;
		return true;
	}
	return false;
}

bool Guard::ShouldTarget(Object & object, World & world){
	switch(object.type){
		case ObjectTypes::PLAYER:{
			Player * player = static_cast<Player *>(&object);
			if((!player->IsDisguised() && !player->IsInvisible(world) && !player->HasSecurityPass()) || player->id == chasing){
				return true;
			}
		}break;
		case ObjectTypes::ROBOT:{
			Robot * robot = static_cast<Robot *>(&object);
			if(robot->virusplanter){
				return true;
			}
		}break;
		case ObjectTypes::FIXEDCANNON:{
			return true;
		}break;
	}
	return false;
}