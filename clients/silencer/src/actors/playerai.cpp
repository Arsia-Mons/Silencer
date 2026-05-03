#include "playerai.h"
#include <algorithm>
#include <map>
#include "basedoor.h"
#include "../gas/gasloader.h"

PlayerAI::PlayerAI(Player & player, Difficulty diff) : player(player){
	direction = false;
	targetplatformset = 0;
	ladderjumping = false;
	linktype = LINK_NONE;
	linkladder = 0;
	state = IDLE;
	difficulty = diff;
	combatTarget = 0;
	combatLockTicks = 0;
	lastHealth = player.health;
	reactionTicks = 0;
	fireBurstRemaining = 0;
	firePauseRemaining = 0;
	jetpackCooldown = 0;
	linkDir = 0;
	linkEdgeX = 0;
	linkTargetX = INT32_MIN;
	linkFromSet = nullptr;
	linkStuckTicks = 0;
	thinkDelay = 0;
	targetTerminal = 0;
}

bool PlayerAI::ScanForTarget(World & world){
	combatTarget = 0;
	const PlayerDef& pd = GASLoader::Get().player;
	int range = pd.aiCombatRange;
	std::vector<Uint8> types;
	types.push_back(ObjectTypes::PLAYER);
	// Use a wider scan box so secret holders are found at longer range
	int scanRange = (state == KILLSECRET) ? range * 3 : range;
	std::vector<Object *> candidates = world.TestAABB(
		player.x - scanRange, player.y - 80,
		player.x + scanRange, player.y + 10,
		types);
	Team * myTeam = player.GetTeam(world);
	int bestDist = scanRange + 1;
	for(auto it = candidates.begin(); it != candidates.end(); ++it){
		Player * p = static_cast<Player *>(*it);
		if(p->id == player.id) continue;
		if(p->state == Player::DEAD || p->state == Player::RESURRECTING) continue;
		if(p->IsInvisible(world) || p->IsDisguised()) continue;
		if(p->InBase(world)) continue;
		// Skip teammates
		Team * theirTeam = p->GetTeam(world);
		if(myTeam && theirTeam && myTeam->id == theirTeam->id) continue;
		// Secret holder always wins regardless of distance
		if(p->hassecret){ combatTarget = p->id; return true; }
		int dist = abs(p->x - player.x);
		if(dist < bestDist){
			bestDist = dist;
			combatTarget = p->id;
		}
	}
	return combatTarget != 0;
}

bool PlayerAI::ApplyCombat(World & world){
	const PlayerDef& pd = GASLoader::Get().player;
	if(combatLockTicks > 0) combatLockTicks--;

	// Re-scan when lock expires or we have no target
	bool hadTarget = (combatTarget != 0);
	if(combatLockTicks <= 0 || combatTarget == 0){
		ScanForTarget(world);
		combatLockTicks = pd.aiTargetLockTicks;
		// Reaction delay when freshly spotting an enemy
		if(combatTarget != 0 && !hadTarget){
			int react = pd.aiReactionTicks;
			if(difficulty == EASY)       react = react * 2;
			else if(difficulty == HARD)  react = react / 2;
			reactionTicks = react + (react > 0 ? rand() % react : 0);
			fireBurstRemaining = 0;
			firePauseRemaining = 0;
		}
	}
	if(combatTarget == 0){
		reactionTicks = 0;
		fireBurstRemaining = 0;
		firePauseRemaining = 0;
		return false;
	}

	// During reaction delay: face target but don't fire yet
	if(reactionTicks > 0){
		reactionTicks--;
		Object * robj = world.GetObjectFromId(combatTarget);
		if(robj){
			Player * rtarget = static_cast<Player *>(robj);
			if(rtarget->x > player.x){ player.input.keymoveright = true; }
			else { player.input.keymoveleft = true; }
		}
		lastHealth = player.health;
		return true;
	}

	Object * obj = world.GetObjectFromId(combatTarget);
	if(!obj || obj->type != ObjectTypes::PLAYER){
		combatTarget = 0;
		return false;
	}
	Player * target = static_cast<Player *>(obj);
	if(target->state == Player::DEAD || target->state == Player::RESURRECTING){
		combatTarget = 0;
		return false;
	}

	// Don't engage combat when too close — let nav handle separation.
	int dist = abs(target->x - player.x);
	if(dist < 80){
		return false;
	}

	// Line-of-sight check — don't fire through solid walls
	{
		int xe = 0, ye = 0;
		int midBot    = player.y - player.height / 2;
		int midTarget = target->y  - target->height / 2;
		Platform * wall = world.map.TestLine(player.x, midBot, target->x, midTarget, &xe, &ye, Platform::RECTANGLE);
		if(wall){
			combatTarget = 0;
			return false;
		}
	}

	// Force facing toward target — override nav-set movement keys so
	// STANDINGSHOOT fires in the correct direction.
	bool targetRight = (target->x > player.x);
	if(targetRight){
		player.input.keymoveleft  = false;
		player.input.keymoveright = true;
	} else {
		player.input.keymoveright = false;
		player.input.keymoveleft  = true;
	}

	// Burst fire: hold keyfire for a burst, pause, then fire again.
	// This gives bots a human-like shooting rhythm instead of holding fire every tick.
	if(firePauseRemaining > 0){
		firePauseRemaining--;
	} else {
		if(fireBurstRemaining <= 0){
			int burst = pd.aiShootBurstTicks;
			if(difficulty == EASY)       burst = burst / 2 + 1;
			else if(difficulty == HARD)  burst = burst + burst / 2;
			fireBurstRemaining = burst;
		}
		player.input.keyfire = true;
		fireBurstRemaining--;
		if(fireBurstRemaining <= 0){
			int pause = pd.aiShootPauseTicks;
			if(difficulty == EASY)       pause = pause + pause / 2;
			else if(difficulty == HARD)  pause = pause / 2;
			firePauseRemaining = pause + (pause > 0 ? rand() % pause : 0);
		}
	}

	// Jetpack dodge: use jetpack randomly in combat, more often when taking damage.
	if(jetpackCooldown > 0){
		jetpackCooldown--;
	} else if(difficulty != EASY && !player.fuellow){
		bool damaged = (player.health < lastHealth);
		int jChance = pd.aiJetpackCombatInterval;
		if(damaged) jChance = (jChance > 1) ? jChance / 2 : 1;
		if(jChance > 0 && rand() % jChance == 0){
			player.input.keyjetpack = true;
			jetpackCooldown = 30 + rand() % 30;
		}
	}

	// Evasion: MEDIUM+ jump-dodge when recently damaged
	if(difficulty != EASY){
		if(player.health < lastHealth){
			if(pd.aiEvadeInterval > 0 && rand() % pd.aiEvadeInterval == 0){
				player.input.keyjump = true;
			}
		}
	}
	lastHealth = player.health;
	return true;
}

void PlayerAI::Tick(World & world){
	// Mirror what HandleInput does for human players: snapshot last tick's input
	// before overwriting it, so rising-edge checks (keyX && !oldinput.keyX) work correctly.
	player.oldinput = player.input;
	Input zeroinput;
	player.input = zeroinput;

	// Age out bad links
	for(int i = (int)badLinks.size() - 1; i >= 0; i--){
		if(--badLinks[i].ttl <= 0) badLinks.erase(badLinks.begin() + i);
	}
	if(state == IDLE){
		SetState(HACK);
	}
	if(player.state == Player::RESURRECTING){
		SetState(IDLE);
		badLinks.clear(); // fresh start after respawn
	}
	if(player.state == Player::RESPAWNING){
		SetState(EXITBASE);
	}
	if(player.state == Player::DEAD){
		if(world.tickcount % 2 == 0){
			player.input.keyactivate = true;
		}
	}
	if(player.hassecret){
		if(player.InBase(world)){
			if(player.InOwnBase(world)){
				SetState(RETURNSECRET);
			}else{
				SetState(EXITBASE);
			}
		}else{
			SetState(GOTOBASE);
		}
	}

	// Retreat to base when health is critical — don't fight, just run
	if(!player.hassecret && state != RETREAT && !player.InBase(world)){
		const PlayerDef& pd = GASLoader::Get().player;
		int threshold = (player.maxhealth * pd.aiRetreatHealthPct) / 100;
		if(player.health > 0 && player.health <= threshold){
			SetState(RETREAT);
		}
	}

	// KILLSECRET: if any enemy has the secret, chase them down.
	// Switch back to HACK once no enemy holds the secret.
	if(!player.hassecret && state != RETREAT && !player.InBase(world)){
		Team* myTeam = player.GetTeam(world);
		Player* secretHolder = nullptr;
		for(Uint16 sid : world.objectsbytype[ObjectTypes::PLAYER]){
			Object* sobj = world.GetObjectFromId(sid);
			if(!sobj) continue;
			Player* sp = static_cast<Player*>(sobj);
			if(sp->id == player.id || sp->state == Player::DEAD) continue;
			Team* theirTeam = sp->GetTeam(world);
			if(myTeam && theirTeam && myTeam->id == theirTeam->id) continue;
			if(sp->hassecret){ secretHolder = sp; break; }
		}
		if(secretHolder && state != KILLSECRET){
			SetState(KILLSECRET);
		} else if(!secretHolder && state == KILLSECRET){
			SetState(HACK);
		}
	}

	
	// Check if we're standing at a hackable terminal right now
	bool atHackableTerminal = false;
	if(state == HACK){
		std::vector<Uint8> types;
		types.push_back(ObjectTypes::TERMINAL);
		std::vector<Object *> collided = world.TestAABB(player.x, player.y - player.height, player.x, player.y, types);
		for(auto* obj : collided){
			Terminal * terminal = static_cast<Terminal *>(obj);
			if(terminal->state == Terminal::READY || terminal->state == Terminal::HACKERGONE){
				atHackableTerminal = true;
				break;
			}
		}
	}

	// Only pick a new nav target when not hacking and not already at a terminal
	if(!targetplatformset && player.state != Player::HACKING && !atHackableTerminal){
		if(thinkDelay > 0){
			thinkDelay--;
		} else if(state == HACK){
			std::vector<Terminal *> terminals = FindNearestTerminals(world);
			if(terminals.size() > 0){
				for(std::vector<Terminal *>::iterator it = terminals.begin(); it != terminals.end(); it++){
					if(SetTarget(world, (*it)->x, (*it)->y)){
						targetTerminal = *it;
						break;
					}
				}
			}
		} else if(state == KILLSECRET){
			// Navigate toward enemy secret holder; re-target each time we clear
			Team* myTeam = player.GetTeam(world);
			for(Uint16 sid : world.objectsbytype[ObjectTypes::PLAYER]){
				Object* sobj = world.GetObjectFromId(sid);
				if(!sobj) continue;
				Player* sp = static_cast<Player*>(sobj);
				if(sp->id == player.id || sp->state == Player::DEAD) continue;
				Team* theirTeam = sp->GetTeam(world);
				if(myTeam && theirTeam && myTeam->id == theirTeam->id) continue;
				if(sp->hassecret){ SetTarget(world, sp->x, sp->y); break; }
			}
		}
	}
	if(!FollowPath(world)){
		// Done following path
		ClearTarget();
	}

	// Combat: HACK state focuses on the terminal — skip combat so bots don't
	// get distracted or have movement keys interrupt the hacking animation.
	// KILLSECRET state always fights; so do all other states except RETREAT.
	if(state != RETREAT && state != HACK){
		ApplyCombat(world);
	}

	// While the player is in the HACKING animation, movement keys must not be set —
	// a fresh keymoveleft/keymoveright press exits the hacking state (player.cpp).
	if(player.state == Player::HACKING){
		player.input.keymoveleft  = false;
		player.input.keymoveright = false;
	}
	
	if(state == HACK){
		const PlayerDef& _pd = GASLoader::Get().player;
		if(_pd.aiDisguiseInterval > 0 && rand() % _pd.aiDisguiseInterval == 0){
			player.input.keydisguise = true;
		}
		if(atHackableTerminal){
			// Always activate — no random interval — and stay put until hacking completes.
			player.input.keyactivate = true;
			ClearTarget();
		}
	}
	
	if(state == EXITBASE){
		if(player.InBase(world)){
			if(!targetplatformset){
				BaseExit * baseexit = GetBaseExit(world);
				if(baseexit){
					SetTarget(world, baseexit->x + 1, baseexit->y);
				}
			}
		}else{
			SetState(IDLE);
		}
	}
	
	if(state == GOTOBASE){
		if(!player.InBase(world)){
			if(!targetplatformset){
				BaseDoor * basedoor = GetBaseDoor(world);
				if(basedoor){
					SetTarget(world, basedoor->x, basedoor->y);
				}
			}
			Team * team = player.GetTeam(world);
			if(team){
				std::vector<Uint8> types;
				types.push_back(ObjectTypes::BASEDOOR);
				std::vector<Object *> collided = world.TestAABB(player.x, player.y - player.height, player.x, player.y, types);
				for(std::vector<Object *>::iterator it = collided.begin(); it != collided.end(); it++){
					BaseDoor * basedoor = static_cast<BaseDoor *>(*it);
					if(basedoor->teamid == team->id){
						player.input.keyactivate = true;
					}
				}
			}
		}
	}
	
	if(state == RETURNSECRET){
		if(!targetplatformset){
			SecretReturn * secretreturn = GetSecretReturn(world);
			if(secretreturn){
				SetTarget(world, secretreturn->x, secretreturn->y + 30);
			}
		}
		if(!player.hassecret){
			SetState(EXITBASE);
		}
	}

	if(state == RETREAT){
		if(player.InBase(world)){
			const PlayerDef& pd = GASLoader::Get().player;
			int threshold = (player.maxhealth * pd.aiRetreatHealthPct) / 100;
			// Once healed enough, leave
			if(player.health >= player.maxhealth - (player.maxhealth / 4)){
				SetState(EXITBASE);
			} else {
				// Navigate to HealMachine so it can heal us
				if(!targetplatformset){
					HealMachine * hm = GetHealMachine(world);
					if(hm){
						SetTarget(world, hm->x, hm->y);
					} else {
						// No HealMachine found — just exit
						SetState(EXITBASE);
					}
				}
			}
			(void)threshold;
		} else {
			// Navigate to own base door and enter
			if(!targetplatformset){
				BaseDoor * basedoor = GetBaseDoor(world);
				if(basedoor){
					SetTarget(world, basedoor->x, basedoor->y);
				}
			}
			Team * team = player.GetTeam(world);
			if(team){
				std::vector<Uint8> types;
				types.push_back(ObjectTypes::BASEDOOR);
				std::vector<Object *> collided = world.TestAABB(player.x, player.y - player.height, player.x, player.y, types);
				for(std::vector<Object *>::iterator it = collided.begin(); it != collided.end(); it++){
					BaseDoor * basedoor = static_cast<BaseDoor *>(*it);
					if(basedoor->teamid == team->id){
						player.input.keyactivate = true;
					}
				}
			}
		}
	}

	if(player.OnGround() && player.inventoryitems[player.currentinventoryitem] == Player::INV_BASEDOOR){
		player.input.keyuse = true;
	}
}

bool PlayerAI::CreatePathToPlatformSet(World & world, std::deque<PlatformSet *> & path, PlatformSet & to){
	Platform * currentplatform = world.map.platformids[player.currentplatformid];
	if(!currentplatform) return false;
	PlatformSet * start = currentplatform->set;
	if(!start) return false;
	if(start == &to){
		path.push_back(start);
		return true;
	}

	// BFS — avoids exponential blowup now that jump/fall/jetpack connect many more sets
	std::map<PlatformSet *, PlatformSet *> parent; // node → predecessor
	std::deque<PlatformSet *> queue;
	queue.push_back(start);
	parent[start] = nullptr;

	while(!queue.empty()){
		PlatformSet * cur = queue.front(); queue.pop_front();
		for(auto it = world.map.platformsets.begin(); it != world.map.platformsets.end(); it++){
			PlatformSet * next = it->get();
			if(parent.count(next)) continue;
			if(FindAnyLink(world, *cur, *next)){
				parent[next] = cur;
				if(next == &to){
					// Reconstruct path start → ... → to
					std::deque<PlatformSet *> rev;
					for(PlatformSet * n = next; n != nullptr; n = parent[n])
						rev.push_front(n);
					path = rev;
					return true;
				}
				queue.push_back(next);
			}
		}
	}
	return false;
}

bool PlayerAI::FollowPath(World & world){
	Platform * currentplatform = world.map.platformids[player.currentplatformid];
	if(currentplatform && currentplatform->set == targetplatformset){
		if(platformsetpath.size() > 0){
			platformsetpath.pop_front();
			linktype = LINK_NONE;
			if(platformsetpath.size() > 0){
				targetplatformset = platformsetpath.front();
			}
		}
	}
	if(targetplatformset){
		if(currentplatform && targetplatformset != currentplatform->set){
			if(linktype == LINK_NONE){
				linkStuckTicks = 0;
				if(!FindAnyLink(world, *currentplatform->set, *targetplatformset)){
					ClearTarget();
				} else {
					linkFromSet = currentplatform->set;
				}
			}
			// If we've been stuck on this link too long, blacklist it and replan
			linkStuckTicks++;
			if(linkStuckTicks > 120){
				badLinks.push_back({currentplatform->set, targetplatformset, 600});
				linkStuckTicks = 0;
				ClearTarget();
				return false;
			}
			switch(linktype){
				case LINK_LADDER:{
					if(player.OnGround()){
						Sint16 center = linkladder->x1 + ((linkladder->x2 - linkladder->x1) / 2);
						if(center < player.x){
							player.input.keymoveleft = true;
						}else
							if(center > player.x){
								player.input.keymoveright = true;
							}
						if(linkladder->x1 <= player.x && linkladder->x2 >= player.x){
							if(linkladder->y1 < player.y){
								player.input.keymoveup = true;
								direction = true;
							}else{
								player.input.keymovedown = true;
								direction = false;
							}
						}
					}
				}break;
				case LINK_FALL:{
					// Walk off the edge in the right direction
					if(linkDir > 0) player.input.keymoveright = true;
					else player.input.keymoveleft = true;
				}break;
				case LINK_JUMP:{
					if(player.OnGround()){
						if(linkDir > 0) player.input.keymoveright = true;
						else player.input.keymoveleft = true;
						// Jump once we reach the edge
						bool atEdge = (linkDir > 0) ? (player.x >= linkEdgeX) : (player.x <= linkEdgeX);
						if(atEdge) player.input.keyjump = true;
					} else {
						// Airborne: keep moving toward target
						if(linkDir > 0) player.input.keymoveright = true;
						else player.input.keymoveleft = true;
					}
				}break;
				case LINK_JETPACK:{
					// Move directly toward linkEdgeX regardless of which side it's on.
					const int EDGE_DEAD = 32; // half a tile — proportionate to 64px tile grid
					if(player.x < linkEdgeX - EDGE_DEAD) player.input.keymoveright = true;
					else if(player.x > linkEdgeX + EDGE_DEAD) player.input.keymoveleft = true;
					bool atEdge = (player.x >= linkEdgeX - EDGE_DEAD && player.x <= linkEdgeX + EDGE_DEAD);
					// Once at the edge OR already mid-launch (state left ground), hold jetpack.
					// Reset stuck timer whenever at edge — bot may be waiting for fuel to refill,
					// which is not "stuck" and should not trigger a replan.
					if(atEdge || !player.OnGround()){
						linkStuckTicks = 0;
						if(!player.fuellow) player.input.keyjetpack = true;
					}
				}break;
			}
		}
	}
	// Airborne continuation for jump/fall/jetpack links — the switch above only runs
	// when currentplatform is non-null (bot on ground). Once airborne, currentplatform
	// is null so we must keep applying direction and jetpack thrust here every tick.
	// Also enforce a stuck timeout while airborne so bots can't fly into walls forever.
	if(targetplatformset && !currentplatform &&
	   (linktype == LINK_JUMP || linktype == LINK_FALL || linktype == LINK_JETPACK)){
		linkStuckTicks++;
		if(linkStuckTicks > 180){
			if(linkFromSet && targetplatformset)
				badLinks.push_back({linkFromSet, targetplatformset, 600});
			linkStuckTicks = 0;
			ClearTarget();
			return false;
		}
		if(linktype == LINK_JETPACK && !targetplatformset->platforms.empty()){
			// Horizontal: push toward targetX (or dest center) until in range.
			bool inRange;
			if(linkTargetX != INT32_MIN){
				inRange = (linkDir > 0) ? (player.x >= linkTargetX) : (player.x <= linkTargetX);
			} else {
				int toX1 = 32767, toX2 = -32768;
				for(auto* p : targetplatformset->platforms){
					if(p->x1 < toX1) toX1 = p->x1;
					if(p->x2 > toX2) toX2 = p->x2;
				}
				inRange = (player.x >= toX1 && player.x <= toX2);
			}
			if(!inRange){
				if(linkDir > 0) player.input.keymoveright = true;
				else            player.input.keymoveleft  = true;
			}
			// Vertical: hold jetpack until we reach the target platform surface.
			if(!player.fuellow){
				int targetY = INT32_MAX;
				for(auto* p : targetplatformset->platforms)
					if((int)p->y1 < targetY) targetY = (int)p->y1;
				if(player.y > targetY) player.input.keyjetpack = true;
			}
		} else if(linktype == LINK_JUMP && !targetplatformset->platforms.empty()){
			// Clamp horizontal push for jump links — stop when within target platform x range
			int toX1 = 32767, toX2 = -32768;
			for(auto* p : targetplatformset->platforms){
				if(p->x1 < toX1) toX1 = p->x1;
				if(p->x2 > toX2) toX2 = p->x2;
			}
			if(player.x < toX1 || player.x > toX2){
				if(linkDir > 0) player.input.keymoveright = true;
				else            player.input.keymoveleft  = true;
			}
		} else if(linktype == LINK_FALL){
			// FALL: platforms overlap in X so gravity handles landing; no horizontal push needed.
		} else {
			if(linkDir > 0) player.input.keymoveright = true;
			else            player.input.keymoveleft  = true;
		}
	}
	if(targetplatformset){
		const PlayerDef& _pd2 = GASLoader::Get().player;
		if(currentplatform && targetplatformset == currentplatform->set){
			int diff = abs(targetx - player.x);
			if(diff > _pd2.aiArrivalThreshold){
				if(targetx < player.x){
					player.input.keymoveleft = true;
				}else
				if(targetx > player.x){
					player.input.keymoveright = true;
				}
			}else{
				return false;
			}
		}
		if(player.state == Player::LADDER){
			ladderjumping = false;
			if(direction){
				player.input.keymoveup = true;
				if(_pd2.aiLadderJumpUpInterval > 0 && rand() % _pd2.aiLadderJumpUpInterval == 0){
					player.input.keyjump = true;
					player.input.keymoveleft = false;
					player.input.keymoveright = false;
					ladderjumping = true;
				}
			}else{
				player.input.keymovedown = true;
				if(_pd2.aiLadderJumpDownInterval > 0 && rand() % _pd2.aiLadderJumpDownInterval == 0){
					player.input.keyjump = true;
				}
			}
		}
		if(player.state == Player::FALLING || player.state == Player::JUMPING){
			if(linktype != LINK_JUMP && linktype != LINK_FALL && linktype != LINK_JETPACK){
				player.input.keymoveleft = false;
				player.input.keymoveright = false;
			}
			if(ladderjumping){
				if(_pd2.aiLadderJumpUpInterval > 0 && rand() % _pd2.aiLadderJumpUpInterval == 0){
					player.input.keymoveup = true;
				}
			}
		}
	}else{
		if(player.state == Player::LADDER){
			player.input.keymovedown = true;
			player.input.keyjump = true;
		}
	}
	
	return true;
}

bool PlayerAI::SetTarget(World & world, Sint16 x, Sint16 y){
	ClearTarget();
	Platform * platform = world.map.TestAABB(x, y, x, y, Platform::RECTANGLE | Platform::STAIRSUP | Platform::STAIRSDOWN);
	if(platform){
		if(CreatePathToPlatformSet(world, platformsetpath, *platform->set)){
			linktype = LINK_NONE;
			targetx = x;
			targety = y;
			targetplatformset = platformsetpath.front();
			//printf("created path of size %d\n", platformsetpath.size());
			return true;
		}else{
			//printf("could not create path to target\n");
			//ClearTarget();
		}
	}
	return false;
}

void PlayerAI::ClearTarget(void){
	targetplatformset = 0;
	platformsetpath.clear();
	linkStuckTicks = 0;
	linkFromSet = nullptr;
	targetTerminal = 0;
}

void PlayerAI::GetCurrentNode(int & x, int & y){
	x = player.x / 64;
	y = (player.y - 32) / 64;
}

Uint8 PlayerAI::GetNodeType(World & world, unsigned int x, unsigned int y){
	if(x > world.map.expandedwidth || y > world.map.expandedheight){
		return 0;
	}
	return world.map.nodetypes[(y * world.map.expandedwidth) + x];
}

Platform * PlayerAI::FindClosestLadderToPlatform(World & world, PlatformSet & from, PlatformSet & to, Sint16 x){
	Platform * ladder = 0;
	std::vector<Platform *> ladders = world.map.LaddersToPlatform(from, to);
	for(std::vector<Platform *>::iterator it = ladders.begin(); it != ladders.end(); it++){
		Platform * newladder = *it;
		if(!ladder){
			ladder = newladder;
		}
		if(abs(x - newladder->x1) < abs(x - ladder->x1)){
			ladder = newladder;
		}
	}
	return ladder;
}

bool PlayerAI::FindLink(World & world, int type, PlatformSet & from, PlatformSet & to){
	// Reject blacklisted links immediately
	for(auto& bl : badLinks){
		if(bl.from == &from && bl.to == &to){ linktype = LINK_NONE; return false; }
	}
	// Helpers to get bounding x and representative y of a platform set
	auto getX1 = [](PlatformSet& ps) -> int {
		int x = 32767;
		for(auto* p : ps.platforms) if(p->x1 < x) x = p->x1;
		return x;
	};
	auto getX2 = [](PlatformSet& ps) -> int {
		int x = -32768;
		for(auto* p : ps.platforms) if(p->x2 > x) x = p->x2;
		return x;
	};
	auto getY = [](PlatformSet& ps) -> int {
		return ps.platforms.empty() ? 0 : (int)ps.platforms[0]->y1;
	};

	switch(type){
		default:
		case LINK_NONE:
			linktype = LINK_NONE;
			return false;
		break;
		case LINK_LADDER: {
			Platform * ladder = FindClosestLadderToPlatform(world, from, to, player.x);
			if(ladder){
				linktype = LINK_LADDER;
				linkladder = ladder;
				return true;
			}
		} break;
		case LINK_FALL: {
			if(from.platforms.empty() || to.platforms.empty()) break;
			int fromY = getY(from);
			int toY   = getY(to);
			if(toY <= fromY + 10) break; // to must be clearly below from
			int fromX1 = getX1(from), fromX2 = getX2(from);
			int toX1   = getX1(to),   toX2   = getX2(to);
			// x-ranges must overlap (within small drift margin)
			if(toX2 < fromX1 - 30 || toX1 > fromX2 + 30) break;
			int fromCX = (fromX1 + fromX2) / 2;
			int toCX   = (toX1   + toX2)   / 2;
			linkDir  = (toCX >= fromCX) ? 1 : -1;
			linkEdgeX = ((linkDir > 0) ? fromX2 : fromX1);
			linktype = LINK_FALL;
			return true;
		} break;
		case LINK_JUMP: {
			if(from.platforms.empty() || to.platforms.empty()) break;
			int fromY      = getY(from);
			int toY        = getY(to);
			int heightDiff = fromY - toY; // positive = to is higher up
			if(heightDiff > 50)  break; // too high — use jetpack
			if(heightDiff < -20) break; // too far below — use fall
			int fromX1 = getX1(from), fromX2 = getX2(from);
			int toX1   = getX1(to),   toX2   = getX2(to);
			int gap = std::max(0, std::max(toX1 - fromX2, fromX1 - toX2));
			if(gap > 160) break; // too far horizontally for a running jump
			int fromCX = (fromX1 + fromX2) / 2;
			int toCX   = (toX1   + toX2)   / 2;
			linkDir  = (toCX >= fromCX) ? 1 : -1;
			// Check the full travel corridor for wall-like platforms.
			// Walls are taller than wide (height > width); floors are wider than tall.
			// Check from 2px above the lower platform up to player-body height so the
			// source/target floor platforms themselves are excluded from the box.
			{
				int cX1 = std::min(fromX1, toX1) + 1;
				int cX2 = std::max(fromX2, toX2) - 1;
				int cY1 = std::min(fromY, toY) - 52;
				int cY2 = std::max(fromY, toY) - 2;
				if(cX2 > cX1){
					Platform* exc = nullptr;
					bool blocked = false;
					for(int i = 0; i < 32; ++i){
						Platform* hit = world.map.TestAABB(cX1, cY1, cX2, cY2, Platform::RECTANGLE, exc);
						if(!hit) break;
						if((hit->y2 - hit->y1) > (hit->x2 - hit->x1)){ blocked = true; break; }
						exc = hit;
					}
					if(blocked) break;
				}
			}
			linkEdgeX = ((linkDir > 0) ? fromX2 - 8 : fromX1 + 8);
			linktype = LINK_JUMP;
			return true;
		} break;
		case LINK_JETPACK: {
			if(from.platforms.empty() || to.platforms.empty()) break;
			int fromY      = getY(from);
			int toY        = getY(to);
			int heightDiff = fromY - toY; // positive = to is higher up
			if(heightDiff <= 50)  break; // jump can handle it
			if(heightDiff > 600)  break; // probably a different zone
			int fromX1 = getX1(from), fromX2 = getX2(from);
			int toX1   = getX1(to),   toX2   = getX2(to);
			int gap = std::max(0, std::max(toX1 - fromX2, fromX1 - toX2));
			if(gap > 250) break; // too far horizontally
			int fromCX = (fromX1 + fromX2) / 2;
			int toCX   = (toX1   + toX2)   / 2;
			linkDir  = (toCX >= fromCX) ? 1 : -1;
			// Check for a wall in the horizontal gap between platforms.
			if(gap > 0){
				int gX1 = (linkDir > 0) ? fromX2 + 1 : toX2 + 1;
				int gX2 = (linkDir > 0) ? toX1 - 1   : fromX1 - 1;
				if(gX2 > gX1 && world.map.TestAABB(gX1, fromY - 50, gX2, fromY, Platform::RECTANGLE)) break;
			}
			// Helper: returns true if a narrow vertical column at x is clear of floor
			// platforms (wider than tall) between toY and fromY.
			auto columnClear = [&](int x) -> bool {
				Platform* exc = nullptr;
				for(int i = 0; i < 32; ++i){
					Platform* hit = world.map.TestAABB(x - 8, toY + 1, x + 8, fromY - 1, Platform::RECTANGLE, exc);
					if(!hit) return true;
					if((hit->x2 - hit->x1) > (hit->y2 - hit->y1)) return false; // floor blocks path
					exc = hit; // wall platform — skip and keep searching
				}
				return true;
			};
			int oX1 = std::max(fromX1, toX1);
			int oX2 = std::min(fromX2, toX2);
			if(oX1 < oX2){
				// Platforms overlap in X — scan the overlap for a clear vertical column.
				// Search outward from the center in 16px steps so we prefer the most
				// natural (central) launch point.
				int mid  = (oX1 + oX2) / 2;
				int half = (oX2 - oX1) / 2;
				int clearX = INT32_MIN;
				for(int step = 0; step <= half + 16 && clearX == INT32_MIN; step += 16){
					int candidates[2] = { mid + step, mid - step };
					int nCandidates = (step == 0) ? 1 : 2;
					for(int j = 0; j < nCandidates && clearX == INT32_MIN; ++j){
						int x = candidates[j];
						if(x < oX1 + 8 || x > oX2 - 8) continue;
						if(columnClear(x)) clearX = x;
					}
				}
				if(clearX == INT32_MIN) break; // every column blocked — no safe route
				// Walk toward the clear column, then jet straight up through it.
				linkDir     = (clearX >= fromCX) ? 1 : -1;
				linkEdgeX   = clearX;
				linkTargetX = clearX;
			} else {
				// No X overlap — platforms are side by side; bot arcs diagonally.
				// Check the entire union X span for blocking floor platforms.
				int fullX1 = std::min(fromX1, toX1) + 1;
				int fullX2 = std::max(fromX2, toX2) - 1;
				if(fullX2 > fullX1){
					Platform* exc = nullptr;
					bool blocked = false;
					for(int i = 0; i < 32; ++i){
						Platform* hit = world.map.TestAABB(fullX1, toY + 1, fullX2, fromY - 1, Platform::RECTANGLE, exc);
						if(!hit) break;
						if((hit->x2 - hit->x1) > (hit->y2 - hit->y1)){ blocked = true; break; }
						exc = hit;
					}
					if(blocked) break;
				}
				linkEdgeX   = ((linkDir > 0) ? fromX2 : fromX1);
				linkTargetX = INT32_MIN;
			}
			linktype = LINK_JETPACK;
			return true;
		} break;
	}
	linktype = LINK_NONE;
	linkTargetX = INT32_MIN;
	return false;
}

bool PlayerAI::FindAnyLink(World & world, PlatformSet & from, PlatformSet & to){
	// Ladders are always resolved dynamically — they're explicit LADDER geometry.
	if(FindLink(world, LINK_LADDER, from, to)) return true;

	// If the map has designer-baked nav links, use those exclusively for
	// jump/fall/jetpack; skip the runtime heuristics entirely.
	if(!world.map.navlinks.empty()){
		for(const auto & nl : world.map.navlinks){
			if(nl.from->set != &from || nl.to->set != &to) continue;
			if(from.platforms.empty() || to.platforms.empty()) continue;
			// Trust the designer — bypass geometry thresholds and set link params directly.
			int fX1 = 32767, fX2 = -32768, tX1 = 32767, tX2 = -32768;
			for(auto* p : from.platforms){ if(p->x1 < fX1) fX1 = p->x1; if(p->x2 > fX2) fX2 = p->x2; }
			for(auto* p : to.platforms)  { if(p->x1 < tX1) tX1 = p->x1; if(p->x2 > tX2) tX2 = p->x2; }
			int fromCX = (fX1 + fX2) / 2, toCX = (tX1 + tX2) / 2;
			int ltype = nl.type == Map::NAVLINK_JUMP  ? LINK_JUMP  :
			            nl.type == Map::NAVLINK_FALL  ? LINK_FALL  : LINK_JETPACK;
			linkTargetX = INT32_MIN;
			if(ltype == LINK_JETPACK){
				// Ground phase: walk to sourceX (or platform edge if not set).
				if(nl.sourceX != INT32_MIN){
					linkEdgeX = nl.sourceX;
				} else {
					int defaultDir = (toCX >= fromCX) ? 1 : -1;
					linkEdgeX = (defaultDir > 0) ? fX2 : fX1;
				}
				// Air phase: linkDir points from launch point toward destination.
				int airFrom = (nl.sourceX != INT32_MIN) ? nl.sourceX : (int)linkEdgeX;
				if(nl.targetX != INT32_MIN){
					linkTargetX = nl.targetX;
					linkDir = (nl.targetX >= airFrom) ? 1 : -1;
				} else {
					linkDir = (toCX >= airFrom) ? 1 : -1;
				}
			} else {
				linkDir   = (toCX >= fromCX) ? 1 : -1;
				linkEdgeX = ((linkDir > 0) ? fX2 : fX1);
				if(ltype == LINK_JUMP) linkEdgeX = ((linkDir > 0) ? fX2 - 8 : fX1 + 8);
			}
			linktype = ltype;
			return true;
		}
		// No baked link for this pair — bot cannot traverse it.
		return false;
	}

	// No baked links — fall back to runtime geometry heuristics (old maps).
	int starttype = (rand() % (LINK_MAXENUM - 2)) + 2; // skip LINK_NONE and LINK_LADDER
	int type = starttype;
	do{
		if(FindLink(world, type, from, to)) return true;
		if(++type >= LINK_MAXENUM) type = 2;
	}while(type != starttype);
	return false;
}

std::vector<Terminal *> PlayerAI::FindNearestTerminals(World & world){
	Team * team = player.GetTeam(world);

	// Collect terminals already claimed by a teammate bot
	std::vector<Terminal *> claimed;
	for(Uint16 id : world.objectsbytype[ObjectTypes::PLAYER]){
		Object* obj = world.GetObjectFromId(id);
		if(!obj) continue;
		Player* p = static_cast<Player*>(obj);
		if(p == &player || !p->ai) continue;
		if(p->GetTeam(world) == team && p->ai->targetTerminal)
			claimed.push_back(p->ai->targetTerminal);
	}

	std::vector<Terminal *> terminals;
	for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
		if((*it)->type == ObjectTypes::TERMINAL){
			Terminal * terminal = static_cast<Terminal *>(*it);
			if(terminal->state == Terminal::READY || terminal->state == Terminal::HACKERGONE || (terminal->state == Terminal::SECRETREADY && team && team->beamingterminalid == terminal->id)){
				bool isClaimed = false;
				for(Terminal* c : claimed){ if(c == terminal){ isClaimed = true; break; } }
				if(!isClaimed)
					terminals.push_back(terminal);
			}
		}
	}
	// Fall back to all available terminals if everything is claimed
	if(terminals.empty()){
		for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
			if((*it)->type == ObjectTypes::TERMINAL){
				Terminal * terminal = static_cast<Terminal *>(*it);
				if(terminal->state == Terminal::READY || terminal->state == Terminal::HACKERGONE || (terminal->state == Terminal::SECRETREADY && team && team->beamingterminalid == terminal->id))
					terminals.push_back(terminal);
			}
		}
	}
	// Sort: SECRETREADY first, then by Manhattan distance so each bot targets its nearest terminal.
	std::sort(terminals.begin(), terminals.end(), [this](Terminal* a, Terminal* b){
		bool aSecret = (a->state == Terminal::SECRETREADY);
		bool bSecret = (b->state == Terminal::SECRETREADY);
		if(aSecret != bSecret) return aSecret > bSecret;
		int distA = abs(a->x - player.x) + abs(a->y - player.y);
		int distB = abs(b->x - player.x) + abs(b->y - player.y);
		return distA < distB;
	});
	return terminals;
}

BaseExit * PlayerAI::GetBaseExit(World & world){
	if(player.InBase(world)){
		BaseDoor * basedoor = static_cast<BaseDoor *>(world.GetObjectFromId(player.basedoorentering));
		if(basedoor){
			for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
				if((*it)->type == ObjectTypes::BASEEXIT){
					BaseExit * baseexit = static_cast<BaseExit *>(*it);
					if(baseexit->teamid == basedoor->teamid){
						return baseexit;
					}
				}
			}
		}
	}
	return 0;
}

SecretReturn * PlayerAI::GetSecretReturn(World & world){
	if(player.InOwnBase(world)){
		Team * team = player.GetTeam(world);
		for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
			if((*it)->type == ObjectTypes::SECRETRETURN){
				SecretReturn * secretreturn = static_cast<SecretReturn *>(*it);
				if(secretreturn->teamid == team->id){
					return secretreturn;
				}
			}
		}
	}
	return 0;
}

BaseDoor * PlayerAI::GetBaseDoor(World & world){
	if(!player.InBase(world)){
		Team * team = player.GetTeam(world);
		if(team){
			for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
				if((*it)->type == ObjectTypes::BASEDOOR){
					BaseDoor * basedoor = static_cast<BaseDoor *>(*it);
					if(basedoor->teamid == team->id){
						return basedoor;
					}
				}
			}
		}
	}
	return 0;
}

void PlayerAI::SetState(Uint8 state){
	if(PlayerAI::state != state){
		PlayerAI::state = state;
		ClearTarget();
		// Short random pause before acting in new state — feels more human
		const PlayerDef& pd = GASLoader::Get().player;
		if(pd.aiThinkDelayMax > 0){
			thinkDelay = rand() % pd.aiThinkDelayMax;
		}
	}
}

HealMachine * PlayerAI::GetHealMachine(World & world){
	// Find any HealMachine in the world — healing eligibility is enforced by player.cpp
	// (checks team->basedoorid == basedoorentering), so any machine in own base works.
	HealMachine * best = 0;
	int bestDist = INT_MAX;
	for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
		if((*it)->type == ObjectTypes::HEALMACHINE){
			HealMachine * hm = static_cast<HealMachine *>(*it);
			int dist = abs(hm->x - player.x) + abs(hm->y - player.y);
			if(dist < bestDist){
				bestDist = dist;
				best = hm;
			}
		}
	}
	return best;
}
