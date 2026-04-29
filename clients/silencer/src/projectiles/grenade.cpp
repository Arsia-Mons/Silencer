#include "grenade.h"
#include "gasloader.h"
#include "player.h"
#include "plasmaprojectile.h"
#include "flareprojectile.h"
#include "plume.h"
#include <math.h>

Grenade::Grenade() : Object(ObjectTypes::GRENADE){
	requiresauthority = true;
	renderpass = 2;
	const WeaponDef* w = GASLoader::Get().GetWeaponDef("grenade");
	res_bank = (w && !w->spriteBanks.empty()) ? w->spriteBanks[0] : 79;
	res_index = 0;
	type = PLASMA;
	ownerid = 0;
	xv = 30;
	yv = -10;
	state_i = 0;
	color = 0;
	radius = 5;
	isphysical = true;
	snapshotinterval = 6;
}

void Grenade::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, type, old);
	data.Serialize(write, state_i, old);
	data.Serialize(write, ownerid, old);
	data.Serialize(write, color, old);
}

void Grenade::Tick(World & world){
	if(state_i <= 4){
		if(state_i == 4){
			const WeaponDef* w = GASLoader::Get().GetWeaponDef("grenade");
			const std::string& sfx = w && !w->soundThrow.empty() ? w->soundThrow : "grenthro.wav";
			EmitSound(world, world.resources.soundbank[sfx], 64);
		}
		/*Player * player = (Player *)world->GetObjectFromId(ownerid);
		if(player){
			x = player->x;
			y = player->y - 60;
		}*/
	}else{
		if(state_i < 30 || (type == FLARE || type == POISONFLARE)){
			if(state_i < 30){
				res_index = state_i % 16;
			}
			Move(*this, world);
		}
		if(state_i >= 30 && (type == FLARE || type == POISONFLARE) && state_i % 3 == 0){
			for(int i = 0; i < 3; i++){
				FlareProjectile * flareprojectile = static_cast<FlareProjectile *>(world.CreateObject(ObjectTypes::FLAREPROJECTILE));
				if(flareprojectile){
					flareprojectile->ownerid = ownerid;
					flareprojectile->x = x;
					flareprojectile->y = y - 1;
					if(type == POISONFLARE){
						flareprojectile->poisonous = true;
					}
					flareprojectile->originalx = flareprojectile->x;
					flareprojectile->originaly = flareprojectile->y;
					switch(i){
						case 0:
							flareprojectile->yv = -3;
							flareprojectile->xv = -3;
						break;
						case 1:
							flareprojectile->yv = -4;
							flareprojectile->xv = 0;
						break;
						case 2:
							flareprojectile->yv = -3;
							flareprojectile->xv = 3;
						break;
					}
				}
			}
		}
		if(state_i == (GASLoader::Get().GetWeaponDef("grenade") ? GASLoader::Get().GetWeaponDef("grenade")->explosionTick : 30)){
			// initial explosion
			draw = false;
			switch(type){
				case EMP:{
					{	const WeaponDef* tw = GASLoader::Get().GetWeaponDef("empbomb");
						const std::string& sfx = (tw && !tw->soundExplosion.empty()) ? tw->soundExplosion : "q_expl02.wav";
						EmitSound(world, world.resources.soundbank[sfx], 96); }
					for(int i = 0; i < 8; i++){
						Plume * plume = (Plume *)world.CreateObject(ObjectTypes::PLUME);
						if(plume){
							plume->type = 5;
							plume->SetPosition(x + (rand() % 33) - 16, y + (rand() % 33) - 16);
						}
					}
					for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
						Object * object = *it;
						//int radius = 500;
						if(object && object->ishittable && object->id != ownerid && world.map.TeamNumberFromY(y) == world.map.TeamNumberFromY(object->y)){
							//int distance = sqrt((float)((x - object->x) * (x - object->x)) + ((y - object->y) * (y - object->y)));
							//if(distance < radius){
								Object empprojectile(ObjectTypes::FLAREPROJECTILE);
								empprojectile.healthdamage = 0;
								empprojectile.shielddamage = 0xFFFF;
								empprojectile.ownerid = ownerid;
								object->HandleHit(world, 50, 50, empprojectile);
							//}
						}
					}
				}break;
				case SHAPED:{
					{	const WeaponDef* tw = GASLoader::Get().GetWeaponDef("shapedbomb");
						const std::string& sfx = (tw && !tw->soundExplosion.empty()) ? tw->soundExplosion : "seekexp1.wav";
						EmitSound(world, world.resources.soundbank[sfx], 128); }
					Sint8 xvs[] = {-10, -5, 0, 5, 10};
					Sint8 yvs[] = {-33, -34, -35, -34, -33};
					Sint8 ys[] = {0, 0, 0, 0, 0};
					for(int i = 0; i < 5; i++){
						PlasmaProjectile * plasmaprojectile = (PlasmaProjectile *)world.CreateObject(ObjectTypes::PLASMAPROJECTILE);
						if(plasmaprojectile){
							plasmaprojectile->large = false;
							plasmaprojectile->x = x;
							plasmaprojectile->y = y + ys[i] - 1;
							plasmaprojectile->ownerid = ownerid;
							plasmaprojectile->xv = xvs[i];
							plasmaprojectile->yv = yvs[i];
						}
					}
				}break;
				case PLASMA:{
					{	const WeaponDef* tw = GASLoader::Get().GetWeaponDef("plasmabomb");
						const std::string& sfx = (tw && !tw->soundExplosion.empty()) ? tw->soundExplosion : "seekexp1.wav";
						EmitSound(world, world.resources.soundbank[sfx], 128); }
					Sint8 xvs[] = {-14, 14, -10, 10, -10, 10};
					Sint8 yvs[] = {-25, -25, -10, -10, -5, -5};
					Sint8 ys[] = {0, 0, 0, 0, 0, 0, 0, 0};
					for(int i = 0; i < 6; i++){
						PlasmaProjectile * plasmaprojectile = (PlasmaProjectile *)world.CreateObject(ObjectTypes::PLASMAPROJECTILE);
						if(plasmaprojectile){
							plasmaprojectile->large = false;
							plasmaprojectile->x = x;
							plasmaprojectile->y = y + ys[i] - 1;
							plasmaprojectile->ownerid = ownerid;
							plasmaprojectile->xv = xvs[i];
							plasmaprojectile->yv = yvs[i];
						}
					}
				}break;
				case NEUTRON:{
					world.SendSound("grenade1.wav");
					{	const WeaponDef* tw = GASLoader::Get().GetWeaponDef("neutronbomb");
						const std::string& sfx = (tw && !tw->soundExplosion.empty()) ? tw->soundExplosion : "q_expl02.wav";
						EmitSound(world, world.resources.soundbank[sfx], 96); }
					for(int i = 0; i < 8; i++){
						Plume * plume = (Plume *)world.CreateObject(ObjectTypes::PLUME);
						if(plume){
							plume->type = 5;
							plume->SetPosition(x + (rand() % 33) - 16, y + (rand() % 33) - 16);
						}
					}
				}break;
				case POISONFLARE:
				case FLARE:{
					draw = true;
					{	const char* wid = (type == POISONFLARE) ? "poisonflare" : "flarebomb";
						const WeaponDef* tw = GASLoader::Get().GetWeaponDef(wid);
						const std::string& sfx = (tw && !tw->soundExplosion.empty()) ? tw->soundExplosion : "rocket1.wav";
						EmitSound(world, world.resources.soundbank[sfx], 128); }
				}break;
			}
		}else
		if(state_i == (GASLoader::Get().GetWeaponDef("grenade") ? GASLoader::Get().GetWeaponDef("grenade")->secondaryTick : 33)){
			// secondary explosion
			switch(type){
				case SHAPED:{
					Sint8 xvs[] = {-10, -5, 5, 10};
					Sint8 yvs[] = {-29, -30, -30, -29};
					Sint8 ys[] = {0, 0, 0, 0};
					for(int i = 0; i < 4; i++){
						PlasmaProjectile * plasmaprojectile = (PlasmaProjectile *)world.CreateObject(ObjectTypes::PLASMAPROJECTILE);
						if(plasmaprojectile){
							plasmaprojectile->large = true;
							plasmaprojectile->x = x;
							plasmaprojectile->y = y + ys[i] - 1;
							plasmaprojectile->oldx = plasmaprojectile->x;
							plasmaprojectile->oldy = plasmaprojectile->y;
							plasmaprojectile->ownerid = ownerid;
							plasmaprojectile->xv = xvs[i];
							plasmaprojectile->yv = yvs[i];
						}
					}
				}break;
				case PLASMA:{
					Sint8 xvs[] = {-14, 0, 14, -5, 0, 5};
					Sint8 yvs[] = {-20, -10, -20, -15, -15, -15};
					Sint8 ys[] = {0, 0, 0, 0, 0, 0};
					for(int i = 0; i < 6; i++){
						PlasmaProjectile * plasmaprojectile = (PlasmaProjectile *)world.CreateObject(ObjectTypes::PLASMAPROJECTILE);
						if(plasmaprojectile){
							plasmaprojectile->large = true;
							plasmaprojectile->x = x;
							plasmaprojectile->y = y + ys[i] - 1;
							plasmaprojectile->ownerid = ownerid;
							plasmaprojectile->xv = xvs[i];
							plasmaprojectile->yv = yvs[i];
						}
					}
				}break;
			}
		}else
		if(state_i == (GASLoader::Get().GetWeaponDef("grenade") ? GASLoader::Get().GetWeaponDef("grenade")->destroyTick : 40) && type != NEUTRON && type != FLARE && type != POISONFLARE){
			world.MarkDestroyObject(id);
		}else
		if(state_i == (GASLoader::Get().GetWeaponDef("grenade") ? GASLoader::Get().GetWeaponDef("grenade")->neutronDestroyTick : 120) && type == NEUTRON){
			world.MarkDestroyObject(id);
			NeutronBlast(world, y, ownerid);
		}else
		if(state_i == (GASLoader::Get().GetWeaponDef("grenade") ? GASLoader::Get().GetWeaponDef("grenade")->flareDuration : 198)){
			world.MarkDestroyObject(id);
		}
	}
	state_i++;
}

bool Grenade::WasThrown(void){
	if(state_i > 4){
		return true;
	}
	return false;
}

bool Grenade::UpdatePosition(World & world, Player & player){
	if(state_i <= 0){
		if(state_i == 0){
			xv = (GASLoader::Get().GetWeaponDef("grenade") ? GASLoader::Get().GetWeaponDef("grenade")->throwSpeedStanding : 20);
			mirrored = player.mirrored;
			if(player.input.keymoveleft){
				mirrored = true;
			}
			if(player.input.keymoveright){
				mirrored = false;
			}
			x = player.x + (5 * (mirrored ? -1 : 1));
			y = player.y - 70;
			if(player.input.keymoveleft || player.input.keymoveright){
				xv = (GASLoader::Get().GetWeaponDef("grenade") ? GASLoader::Get().GetWeaponDef("grenade")->throwSpeedMoving : 30);
				if(player.state == Player::RUNNING){
					xv = (GASLoader::Get().GetWeaponDef("grenade") ? GASLoader::Get().GetWeaponDef("grenade")->throwSpeedRunning : 26) + abs(player.xv);
				}
			}
			if(player.input.keymovedown){
				y = player.y - 30;
				x = player.x;
				xv = 0;
				yv = 5;
			}
			if(player.input.keylookdownleft || player.input.keylookdownright){
				y = player.y - 30;
				xv = 25;
				yv = 10;
			}
			if(player.input.keymoveup){
				x = player.x;
				xv = 5;
				yv = -30;
			}
			if(player.input.keylookupleft || player.input.keylookupright){
				xv = 25;
				yv = -20;
			}
			if(player.state == Player::CROUCHEDTHROWING){
				xv = 20;
				y = player.y - 30;
				yv = -10;
			}
			if(xv < 0){
				if(!mirrored){
					xv = abs(xv);
				}
			}else{
				if(mirrored){
					xv = -xv;
				}
			}
			xv += (player.xv / 4);
			yv += (player.yv / 4);
			if(world.map.TestAABB(x - radius, y - radius, x + radius, y + radius, Platform::RECTANGLE | Platform::STAIRSUP | Platform::STAIRSDOWN, 0, true)){
				return false;
			}
		}
	}
	return true;
}

void Grenade::SetType(Uint8 type){
	Grenade::type = type;
	switch(type){
		case EMP:
			color = (8 << 4) + (10 & 0xF);
		break;
		case SHAPED:
			color = (9 << 4) + (13 & 0xF);
		break;
		case PLASMA:
			color = (9 << 4) + (14 & 0xF);
		break;
		case NEUTRON:
			color = (15 << 4) + (13 & 0xF);
		break;
		case POISONFLARE:
			color = (11 << 4) + (14 & 0xF);
		break;
		case FLARE:
			color = (9 << 4) + (12 & 0xF);
		break;
	}
}

void Grenade::Move(Object & object, World & world, int v){
	int xv2 = object.xv;
	int yv2 = object.yv;
	Platform * platform = world.map.TestIncr(object.x - object.radius, object.y - object.radius, object.x + object.radius, object.y + object.radius, &xv2, &yv2, Platform::RECTANGLE | Platform::STAIRSUP | Platform::STAIRSDOWN);
	if(platform){
		Uint32 yt = platform->XtoY(object.x);
		if(platform->y2 - platform->y1 <= 1 && object.y >= yt){
			xv2 = object.xv;
			yv2 = object.yv;
			platform = world.map.TestIncr(object.x - object.radius, object.y - object.radius, object.x + object.radius, object.y + object.radius, &xv2, &yv2, Platform::RECTANGLE | Platform::STAIRSUP | Platform::STAIRSDOWN, platform);
		}
	}
	int morex = abs(object.xv - xv2);
	int morey = abs(object.yv - yv2);
	int movedx = abs(object.xv) - morex;
	int movedy = abs(object.yv) - morey;
	int volume = 0;
	if(platform){
		if(!v){
			volume = (object.xv + object.yv) * 10;
			if(volume > 128){
				volume = 128;
			}
		}
		float xn, yn;
		platform->GetNormal(object.x, object.y, &xn, &yn);
		if(xn){
			object.xv = (xn * abs(object.xv)) * 0.5;
		}else{
			object.xv *= 0.8;
		}
		if(yn){
			object.yv = (yn * abs(object.yv)) * 0.4;
		}else{
			object.yv *= 0.8;
		}
	}
	object.x += xv2;
	object.y += yv2;
	if(morex || morey){
		if(!v){
			v = object.yv;
			if(!v){
				v = 1;
			}
		}
		Move(object, world, v);
	}
	if(movedx == 0 && v && abs(movedy - v) <= 2){
		object.xv = 0;
		object.yv = 0;
	}else{
		object.EmitSound(world, world.resources.soundbank[GASLoader::Get().GetWeaponDef("grenade") && !GASLoader::Get().GetWeaponDef("grenade")->soundLand.empty() ? GASLoader::Get().GetWeaponDef("grenade")->soundLand : "land1.wav"], volume);
		object.yv += world.gravity;
	}
}

void Grenade::NeutronBlast(World & world, Sint16 y, Uint16 ownerid){
	for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
		Object * object = *it;
		if(object && object->ishittable && world.map.TeamNumberFromY(y) == world.map.TeamNumberFromY(object->y)){
			bool invulnerable = false;
			if(object->type == ObjectTypes::PLAYER){
				Player * player = static_cast<Player *>(object);
				if(player->hasdepositor){
					invulnerable = true;
					player->hasdepositor = false;
				}
			}
			if(!invulnerable){
				Object neutronprojectile(ObjectTypes::FLAREPROJECTILE);
				neutronprojectile.healthdamage = 0xFFFF;
				neutronprojectile.shielddamage = 0xFFFF;
				neutronprojectile.ownerid = ownerid;
				object->HandleHit(world, 50, 50, neutronprojectile);
			}
		}
	}
}