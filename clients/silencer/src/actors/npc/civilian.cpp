#include "civilian.h"
#include "btdebug.h"
#include "projectile.h"
#include "bodypart.h"
#include "npc_math.h"
#include "player.h"
#include "plasmaprojectile.h"
#include "gasloader.h"
#include "audio/soundcue.h"
#include "frameevents.h"

Civilian::Civilian() : Object(ObjectTypes::CIVILIAN){
	requiresauthority = true;
	state = NEW;
	state_i = 0;
	const EnemyDef* c = GASLoader::Get().GetEnemyDef("civilian");
	speed = c ? c->speed : 4;
	// Health pool only matters for poison DoT — direct hits kill via the state machine.
	maxhealth = c ? c->health : 10;
	health = maxhealth;
	poisonedby = 0;
	poisonedamount = 0;
	poisoned_i = 0;
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
	const EnemyDef* c = GASLoader::Get().GetEnemyDef("civilian");
	std::string treeId = (c && !c->behaviorTree.empty()) ? c->behaviorTree : "civilian";
	bt_ = BehaviorTreeLibrary::instance().get(treeId);
	if(!bt_) return;
	btctx_.actions["Run"] = [this](BTContext& ctx) -> BTResult {
		if(ctx.props){
			int bonus = ctx.props->value("speedBonus", -1);
			if(bonus >= 0) ctx.bbSet("bt_run_speed_bonus", bonus);
		}
		if(state != RUNNING){ state = RUNNING; state_i = (Uint8)-1; }
		return BTResult::Success;
	};
	btctx_.actions["Wander"] = [this](BTContext& ctx) -> BTResult {
		if(ctx.props){
			int spd = ctx.props->value("speed", -1);
			if(spd >= 0) ctx.bbSet("bt_walk_speed", spd);
		}
		return BTResult::Success;
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

void Civilian::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state, old);
	data.Serialize(write, state_i, old);
	data.Serialize(write, tractteamid, old);
	data.Serialize(write, poisonedby, old);
	data.Serialize(write, poisonedamount, old);
	data.Serialize(write, poisoned_i, old);
}

void Civilian::Tick(World & world){
	Hittable::Tick(*this, world);
	Bipedal::Tick(*this, world);
	TickPoison(world);
	switch(state){
		case NEW:{
			if(FindCurrentPlatform(*this, world)){
				state = WALKING;
				state_i = -1;
				break;
			}
			/*res_bank = 121;
			res_index = 0;
			yv += world->gravity;
			if(yv > world->maxyvelocity){
				yv = world->maxyvelocity;
			}
			Uint32 xe = x + xv;
			Uint32 ye = y + yv;
			Platform * platform = world->map.TestLine(x, y, xe, ye, &xe, &ye, Platform::RECTANGLE | Platform::STAIRSUP | Platform::STAIRSDOWN);
			if(platform){
				currentplatformid = platform->id;
				state = WALKING;
				state_i = 0;
			}
			x = xe;
			y = ye;*/
		}break;
		case STANDING:{
			if(CheckTractVictim(world)){
				break;
			}
			if(state_i >= 10){
				state_i = 0;
			}
			res_bank = 121;
			res_index = state_i;
		}break;
		case WALKING:{
			if(CheckTractVictim(world)){
				break;
			}
			if(state_i >= 20){
				state_i = 0;
			}
			res_bank = 122;
			res_index = state_i;
			// play per-frame events defined in actordefs/civilian.json
			{
				auto it = world.resources.actordefs.find("civilian");
				if(it != world.resources.actordefs.end()){
					auto* seq = it->second.GetSequence("WALKING");
					std::string ev; int evVol;
					if(seq && seq->GetFrameSoundByIndex(state_i, ev, evVol)){
						FireFrameEvent(ev, &it->second, currentplatformid, *this, world);
					}
				}
			}
			if(DistanceToEnd(*this, world) <= world.minwalldistance){
				mirrored = mirrored ? false : true;
			}
			xv = (mirrored ? -1 : 1) * btctx_.bb<int>("bt_walk_speed", speed);
			FollowGround(*this, world, xv);
			if(state_i % 10 == 0){
				if(!bt_) InitBT();
				if(bt_){
					btctx_.dt = 10.0f / GASLoader::Get().gameengine.ticksPerSecond;
					btctx_.userData = &world;
					btctx_.bbSet("health_pct", GetMaxHealth() > 0 ? (float)GetHealth() / (float)GetMaxHealth() : 0.0f);
					btctx_.bbSet("on_ladder", false);
					{
						const char* sn = "unknown";
						switch(state){
							case NEW:           sn = "new"; break;
							case STANDING:      sn = "standing"; break;
							case WALKING:       sn = "walking"; break;
							case RUNNING:       sn = "running"; break;
							case DYINGFORWARD:  sn = "dyingforward"; break;
							case DYINGBACKWARD: sn = "dyingbackward"; break;
							case DYINGEXPLODE:  sn = "dyingexplode"; break;
							case DEAD:          sn = "dead"; break;
						}
						btctx_.bbSet("state_name", std::string{sn});
					}
					btctx_.bbSet("dist_to_target", -1);
					btctx_.bbSet("threat_nearby", Look(world));
					bt_->tick(btctx_);
					BTDebug::broadcast("civilian", id, btctx_.blackboard, btctx_.nodeResults);
				}else{
					Look(world);
				}
			}
		}break;
		case RUNNING:{
			if(CheckTractVictim(world)){
				break;
			}
			{ const EnemyDef* _cd = GASLoader::Get().GetEnemyDef("civilian");
			  int _rdt = _cd ? _cd->runDurationTicks : 150;
			  if(state_i >= _rdt){
				state = WALKING;
				state_i = -1;
				break;
			  }
			}
			{ const EnemyDef* _gd = GASLoader::Get().GetEnemyDef("civilian"); int _bonus = btctx_.bb<int>("bt_run_speed_bonus", _gd ? _gd->runSpeedBonus : 5); xv = (mirrored ? -1 : 1) * (_bonus + speed); }
			res_bank = 123;
			res_index = state_i % 15;
			// play per-frame events defined in actordefs/civilian.json
			{
				auto it = world.resources.actordefs.find("civilian");
				if(it != world.resources.actordefs.end()){
					auto* seq = it->second.GetSequence("RUNNING");
					std::string ev; int evVol;
					if(seq && seq->GetFrameSoundByIndex(state_i % 15, ev, evVol)){
						FireFrameEvent(ev, &it->second, currentplatformid, *this, world);
					}
				}
			}
			if(DistanceToEnd(*this, world) <= world.minwalldistance){
				mirrored = mirrored ? false : true;
			}
			FollowGround(*this, world, xv);
			if(state_i % 10 == 9){
				Look(world);
			}
		}break;
		case DYINGFORWARD:{
			tractteamid = 0;
			if(state_i == 0){
				const EnemyDef* gd = GASLoader::Get().GetEnemyDef("civilian");
				static const EnemyDef _ced;
				const std::string& hurtSlot = gd ? gd->soundHurt : _ced.soundHurt;
				if(!hurtSlot.empty()){
					auto r = ResolveSound(hurtSlot, world.resources);
					if(r.chunk) EmitSound(world, r.chunk, static_cast<int>(128 * r.volume));
				}
			}
			collidable = false;
			if(state_i >= 14){
				state = DEAD;
				state_i = -1;
				break;
			}
			FollowGround(*this, world, xv);
			res_bank = 126;
			res_index = state_i;
		}break;
		case DYINGBACKWARD:{
			tractteamid = 0;
			if(state_i == 0){
				const EnemyDef* gd = GASLoader::Get().GetEnemyDef("civilian");
				static const EnemyDef _ced;
				const std::string& hurtSlot = gd ? gd->soundHurt : _ced.soundHurt;
				if(!hurtSlot.empty()){
					auto r = ResolveSound(hurtSlot, world.resources);
					if(r.chunk) EmitSound(world, r.chunk, static_cast<int>(128 * r.volume));
				}
			}
			collidable = false;
			if(state_i >= 14){
				state = DEAD;
				state_i = -1;
				break;
			}
			FollowGround(*this, world, xv);
			res_bank = 125;
			res_index = state_i;
		}break;
		case DYINGEXPLODE:{
			tractteamid = 0;
			draw = false;
			state = DEAD;
			state_i = -1;
			break;
		}break;
		case DEAD:{
			collidable = false;
			{ const EnemyDef* _cd = GASLoader::Get().GetEnemyDef("civilian");
			  int _drt = _cd ? _cd->deadRespawnTicks : 100;
			  if(state_i >= _drt){
				draw = true;
				collidable = true;
				state = WALKING;
				state_warp = _cd ? _cd->warpTeleportTick : GASLoader::Get().player.warpTeleportTick;
				state_i = -1;
				health = maxhealth;
				poisonedby = 0;
				poisonedamount = 0;
				poisoned_i = 0;
				break;
			  }
			}
		}break;
	}
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
		xv = npc_math::AbsInt(xv) * xpcnt;
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

bool Civilian::Poison(World & world, Uint16 playerid, Uint8 amount){
	if(state == DYINGFORWARD || state == DYINGBACKWARD || state == DYINGEXPLODE || state == DEAD){
		return false;
	}
	int maxpois = GASLoader::Get().player.maxPoisoned;
	if(poisonedamount >= maxpois){
		return false;
	}
	poisonedby = playerid;
	poisonedamount += amount;
	if(poisonedamount > maxpois){
		poisonedamount = maxpois;
	}
	state_hit = 1 + (3 * 32);
	hitx = 50;
	hity = 50;
	return true;
}

void Civilian::TickPoison(World & world){
	if(!poisonedby || state == DYINGFORWARD || state == DYINGBACKWARD || state == DYINGEXPLODE || state == DEAD){
		return;
	}
	const EnemyDef* cd = GASLoader::Get().GetEnemyDef("civilian");
	int cycle = cd ? cd->poisonTickCycle : 24;
	int damage = cd ? cd->poisonDamagePerTick : 1;
	if(poisoned_i % (cycle / poisonedamount) == 0){
		health = health > damage ? health - damage : 0;
		if(destructible && world.IsAuthority()){
			GameEvent ev;
			ev.actor_id = id;
			if(health == 0){
				ev.type = EventType::ACTOR_KILLED;
			}else{
				ev.type = EventType::ACTOR_DAMAGED;
				ev.value = damage;
			}
			world.triggerGraph.Bus().Emit(ev);
		}
		if(health == 0){
			// Silent collapse in place — no knockback, no threat reaction.
			state = DYINGFORWARD;
			state_i = 0;
			xv = 0;
			Object * owner = world.GetObjectFromId(poisonedby);
			if(owner && owner->type == ObjectTypes::PLAYER){
				Peer * peer = static_cast<Player *>(owner)->GetPeer(world);
				if(peer){
					peer->stats.civilianskilled++;
				}
			}
			poisonedby = 0;
			poisonedamount = 0;
		}
	}
	poisoned_i++;
	if(poisoned_i >= cycle){
		poisoned_i = 0;
	}
}

bool Civilian::Look(World & world){
	std::vector<Uint8> types;
	types.push_back(ObjectTypes::BLASTERPROJECTILE);
	types.push_back(ObjectTypes::LASERPROJECTILE);
	types.push_back(ObjectTypes::ROCKETPROJECTILE);
	types.push_back(ObjectTypes::FLAMERPROJECTILE);
	types.push_back(ObjectTypes::PLASMAPROJECTILE);
	types.push_back(ObjectTypes::WALLPROJECTILE);
	types.push_back(ObjectTypes::FLAREPROJECTILE);
	const EnemyDef* _civgd = GASLoader::Get().GetEnemyDef("civilian");
	int _tdx = _civgd ? _civgd->threatDetectX : 200;
	int _tdy = _civgd ? _civgd->threatDetectY : 100;
	std::vector<Object *> objects = world.TestAABB(x - _tdx, y - _tdy, x + _tdx, y + _tdy, types);
	if(objects.size() > 0){
		if(objects[0]->x > x){
			mirrored = true;
		}else{
			mirrored = false;
		}
		if(state != RUNNING){
			state = RUNNING;
			state_i = -1;
		}
		return true;
	}
	return false;
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
			const EnemyDef* def = GASLoader::Get().GetEnemyDef("civilian");
			const std::string& sfx = (def && !def->soundDeath.empty()) ? def->soundDeath : "seekexp1.wav";
			auto _r = ResolveSound(sfx, world.resources);
			if(_r.chunk) EmitSound(world, _r.chunk, static_cast<int>(128 * _r.volume));
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
