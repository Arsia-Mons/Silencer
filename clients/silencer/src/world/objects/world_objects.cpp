#include "world_object_registry.h"
#include "world.h"
#include "serializer.h"
#include "player.h"
#include "civilian.h"
#include "robot.h"
#include "fixedcannon.h"
#include "walldefense.h"
#include "techstation.h"
#include "surveillancemonitor.h"
#include "team.h"
#include "objecttypes.h"
#include "terminal.h"
#include "basedoor.h"
#include "bodypart.h"
#include "gasloader.h"
#include "gamestateobject.h"
#include "text_wrap.h"
#include <algorithm>

#define DELTAENABLED 1

WorldObjectRegistry::WorldObjectRegistry(World & world) : world(world){
	currentid = 1;
	illuminate = 0;
}

void World::TickObjects(void){
	for(std::list<Object *>::reverse_iterator i = objects.objectlist.rbegin(); i != objects.objectlist.rend(); i++){
		Object * object = (*i);
		bool peercontrolled = false;
		bool localpeercontrolled = false;
		if(object->iscontrollable){
			for(int i = 0; i < maxpeers; i++){
				Peer * peer = peers.peerlist[i];
				if(peer && !peer->isbot){
					for(std::list<Uint16>::iterator it = peer->controlledlist.begin(); it != peer->controlledlist.end(); it++){
						if((*it) == object->id){
							peercontrolled = true;
							if(peer == peers.peerlist[peers.localpeerid]){
								localpeercontrolled = true;
							}
						}
					}
				}
			}
		}
		if(!(object->requiresmaptobeloaded && !map.loaded) && !object->wasdestroyed){
			if((!IsAuthority() && !localpeercontrolled) || (IsAuthority() && !peercontrolled)){
				object->oldx = object->x;
				object->oldy = object->y;
				object->Tick(*this);
				object->lasttick = tickcount;
			}/*else
			if(!IsAuthority() && !localpeercontrolled){
				// Tick everything to do the sounds and effects, then go back
				Serializer olddata;
				object->Serialize(Serializer::WRITE, olddata);
				//LoadSnapshot(*data, false);
				//replaying = true;
				object->oldx = object->x;
				object->oldy = object->y;
				object->Tick(*this);
				object->lasttick = tickcount;
				//replaying = false;
				object->Serialize(Serializer::READ, olddata);
				//
			}*/
		}
	}
	DestroyMarkedObjects();
	Player * localplayer = GetPeerPlayer(peers.localpeerid);
	if(localplayer){
		audio.UpdateAllVolumes(*this, localplayer->x, localplayer->y, GASLoader::Get().world.audioRange);
	}else{
		audio.UpdateAllVolumes(*this, replay.x, replay.y, GASLoader::Get().world.audioRange);
	}
}

bool World::RelevantToPlayer(Player * player, Object * object){
	switch(object->type){
		case ObjectTypes::TEAM:
		case ObjectTypes::STATE:
			return true;
		break;
		case ObjectTypes::PLAYER:{
			Player * objplayer = static_cast<Player *>(object);
			if(objplayer->hassecret || objplayer->state == Player::UNDEPLOYING){
				return true;
			}
		}break;
		case ObjectTypes::PICKUP:{
			PickUp * pickup = static_cast<PickUp *>(object);
			if(pickup->type == PickUp::SECRET){
				return true;
			}
		}break;
		case ObjectTypes::SURVEILLANCEMONITOR:{
			
		}break;
	}
	if(!player){
		return false;
	}
	if(rand() % objects.objectlist.size() == 0){
		return true;
	}
	if(object->snapshotinterval >= 0 && tickcount % (object->snapshotinterval + 1) == 0){
		return true;
	}
	if(object->issprite){
		const WorldDef& _wd = GASLoader::Get().world;
		if(abs(player->x - object->x) <= _wd.networkSyncRangeX && abs(player->y - object->y) <= _wd.networkSyncRangeY){
			return true;
		}
		for(std::vector<Uint16>::iterator it = objects.objectsbytype[ObjectTypes::SURVEILLANCEMONITOR].begin(); it != objects.objectsbytype[ObjectTypes::SURVEILLANCEMONITOR].end(); it++){
			Object * obj = GetObjectFromId((*it));
			if(obj){
				if(abs(player->x - obj->x) <= _wd.networkSyncRangeX && abs(player->y - obj->y) <= _wd.networkSyncRangeY){
					SurveillanceMonitor * surveillancemonitor = static_cast<SurveillanceMonitor *>(obj);
					if(surveillancemonitor->camera.IsVisible(*this, *object)){
						return true;
					}
				}
			}
		}
		Object * grenade = GetObjectFromId(player->currentgrenade);
		if(grenade){
			if(abs(grenade->x - object->x) <= _wd.grenadesyncRangeX && abs(grenade->y - object->y) <= _wd.grenadesyncRangeY){
				return true;
			}
		}
		Object * detonator = GetObjectFromId(player->currentdetonator);
		if(detonator){
			if(abs(detonator->x - object->x) <= _wd.grenadesyncRangeX && abs(detonator->y - object->y) <= _wd.grenadesyncRangeY){
				return true;
			}
		}
	}
	return false;
}

bool World::BelongsToTeam(Object & object, Uint16 teamid){
	switch(object.type){
		case ObjectTypes::PLAYER:{
			Player * player = static_cast<Player *>(&object);
			if(player->teamid == teamid){
				return true;
			}
		}break;
		case ObjectTypes::CIVILIAN:{
			Civilian * civilian = static_cast<Civilian *>(&object);
			if(civilian->tractteamid == teamid){
				return true;
			}
		}break;
		case ObjectTypes::ROBOT:{
			Robot * robot = static_cast<Robot *>(&object);
			if(robot->virusplanter == teamid){
				return true;
			}
		}break;
		case ObjectTypes::FIXEDCANNON:{
			FixedCannon * fixedcannon = static_cast<FixedCannon *>(&object);
			Player * player = static_cast<Player *>(GetObjectFromId(fixedcannon->ownerid));
			if(player && player->teamid == teamid){
				return true;
			}
		}break;
		case ObjectTypes::WALLDEFENSE:{
			WallDefense * walldefense = static_cast<WallDefense *>(&object);
			if(walldefense->teamid == teamid){
				return true;
			}
		}break;
		case ObjectTypes::TECHSTATION:{
			TechStation * techstation = static_cast<TechStation *>(&object);
			if(techstation->teamid == teamid){
				return true;
			}
		}break;
	}
	return false;
}

bool World::IsCollidable(Uint8 type){
	switch(type){
		case ObjectTypes::SHRAPNEL:
		case ObjectTypes::PLUME:
			return false;
		break;
		default:
			return true;
		break;
	}
}

void World::Illuminate(void){
	objects.illuminate = GASLoader::Get().world.illuminateLevel;
}

void World::SetSystemCamera(bool system, Uint16 objectfollow, Sint16 x, Sint16 y){
	systemcameraactive[system] = true;
	systemcamerafollow[system] = objectfollow;
	systemcamerax[system] = x;
	systemcameray[system] = y;
}

void World::BroadcastCamera(Sint16 x, Sint16 y){
	if(!IsAuthority()) return;
	char msg[5];
	msg[0] = MSG_CAMERA;
	memcpy(&msg[1], &x, 2);
	memcpy(&msg[3], &y, 2);
	for(unsigned int i = 0; i < maxpeers; i++){
		Peer * p = peers.peerlist[i];
		if(p && i != peers.localpeerid) SendPacket(p, msg, 5);
	}
}

Object * WorldObjectRegistry::GetObjectFromId(Uint16 id){
	if(objectidlookup.find(id) != objectidlookup.end()){
		return objectidlookup[id];
	}
	return 0;
}

Object * WorldObjectRegistry::CreateObject(Uint8 type, Uint16 id){
	if(world.replaying){ // Do not create objects when rewinding/replaying game state
		return 0;
	}
	if(objectlist.size() == maxobjects){
		return 0;
	}
	Object * object = world.objecttypes.CreateFromType(type);
	if(!object){
		return 0;
	}
	if(id == 0 && object->RequiresAuthority() && world.mode != World::AUTHORITY){
		delete object;
		return 0;
	}
	Uint16 searchid = currentid;
	if(id == 0){
		if(object->RequiresAuthority()){
			searchid = currentid;
		}else{
			searchid = currentid | 0x8000;
		}
		while(currentid <= maxobjects){
			if(!objectidlookup[searchid]){
				break;
			}
			currentid++;
			searchid++;
			if(currentid >= maxobjects){
				currentid = 1;
				searchid = 1;
			}
		}
		object->id = searchid;
	}else{
		object->id = id;
	}
	objectlist.push_back(object);
	if(world.IsCollidable(type)){
		tobjectlist.push_back(object);
	}
	objectsbytype[type].push_back(object->id);
	objectidlookup[object->id] = object;
	return object;
}

void WorldObjectRegistry::MarkDestroyObject(Uint16 id){
	if(world.replaying){
		return;
	}
	Object * object = GetObjectFromId(id);
	if(object){
		object->wasdestroyed = true;
		objectdestroylist.push_back(id);
	}
}

void WorldObjectRegistry::DestroyMarkedObjects(void){
	for(std::list<Uint16>::iterator i = objectdestroylist.begin(); i != objectdestroylist.end(); i++){
		DestroyObject((*i));
	}
	objectdestroylist.clear();
}

void WorldObjectRegistry::DestroyObject(Uint16 id){
	Object * object = GetObjectFromId(id);
	if(object){
		objectidlookup.erase(object->id);
		object->OnDestroy(world);
		objectlist.remove(object);
		if(world.IsCollidable(object->type)){
			tobjectlist.remove(object);
		}
		std::vector<Uint16>::iterator f = std::find(objectsbytype[object->type].begin(), objectsbytype[object->type].end(), id);
		if(f != objectsbytype[object->type].end()){
			objectsbytype[object->type].erase(f);
		}
		delete object;
	}
}

void WorldObjectRegistry::DestroyAllObjects(void){
	for(std::list<Object *>::iterator j = objectlist.begin(); j != objectlist.end(); j++){
		(*j)->OnDestroy(world);
		delete (*j);
	}
	objectlist.clear();
	tobjectlist.clear();
	objectidlookup.clear();
	objectdestroylist.clear();
	for(int i = 0; i < ObjectTypes::MAX_OBJECT_TYPE; i++){
		objectsbytype[i].clear();
	}
}

bool WorldObjectRegistry::TestAABB(int x1, int y1, int x2, int y2, Object * object, std::vector<Uint8> & types, bool){
	int sx1 = 0, sy1 = 0, sx2 = 0, sy2 = 0;
	object->GetAABB(world.resources, &sx1, &sy1, &sx2, &sy2);
	Uint8 type = object->type;
	if(types.size() > 0 && std::find(types.begin(), types.end(), type) == types.end()){
		return false;
	}
	if(((x1 <= sx1 && x2 >= sx1) || (x1 <= sx2 && x2 >= sx2) || (x1 >= sx1 && x2 <= sx2)) &&
	   ((y1 <= sy1 && y2 >= sy1) || (y1 <= sy2 && y2 >= sy2) || (y1 >= sy1 && y2 <= sy2))){
		return true;
	}
	return false;
}

std::vector<Object *> WorldObjectRegistry::TestAABB(int x1, int y1, int x2, int y2, std::vector<Uint8> & types, Uint16 except, Uint16 teamid, bool onlycollidable){
	std::vector<Object *> objects;
	for(std::list<Object *>::iterator i = tobjectlist.begin(); i != tobjectlist.end(); i++){
		Object * object = (*i);
		if(object->issprite){
			if(object->id != except){
				if(!object->isphysical || !onlycollidable || (object->isphysical && object->collidable)){
					if(!teamid || (teamid && !world.BelongsToTeam(*object, teamid))){
						if(TestAABB(x1, y1, x2, y2, object, types)){
							objects.push_back(object);
						}
					}
				}
			}
		}
	}
	return objects;
}

Object * WorldObjectRegistry::TestIncr(int x1, int y1, int x2, int y2, int * xv, int * yv, std::vector<Uint8> & types, Uint16 except, Uint16 teamid){
	int xb1 = x1 + (*xv < 0 ? *xv : 0);
	int yb1 = y1 + (*yv < 0 ? *yv : 0);
	int xb2 = x2 + (*xv > 0 ? *xv : 0);
	int yb2 = y2 + (*yv > 0 ? *yv : 0);
	std::vector<Object *> testobjects = TestAABB(xb1, yb1, xb2, yb2, types, except, teamid); // broadphase
	std::vector<Object *> test;
	for(std::vector<Object *>::iterator it = testobjects.begin(); it != testobjects.end(); it++){
		if((*it)->issprite){
			test.push_back(*it);
		}
	}
	if(test.size() == 0){
		return 0;
	}
	int dx = *xv;
	int dy = *yv;
	int step;
	int error;
	int oldxv = *xv;
	int oldyv = *yv;
	int yv0 = 0;
	int xv0 = 0;
	*xv = 0;
	*yv = 0;
	float slope = 0;
	if(dx){
		slope = (float)dy / dx;
	}else{
		if(oldyv > 0){
			while(*yv < oldyv){
				for(std::vector<Object *>::iterator i = test.begin(); i != test.end(); i++){
					Object * object = *i;
					if(TestAABB(x1 + *xv, y1 + *yv, x2 + *xv, y2 + *yv, object, types)){
						*yv = yv0;
						return object;
					}
				}
				yv0 = *yv;
				(*yv)++;
			}
		}else{
			while(*yv > oldyv){
				for(std::vector<Object *>::iterator i = test.begin(); i != test.end(); i++){
					Object * object = *i;
					if(TestAABB(x1 + *xv, y1 + *yv, x2 + *xv, y2 + *yv, object, types)){
						*yv = yv0;
						return object;
					}
				}
				yv0 = *yv;
				(*yv)--;
			}
		}
	}
	if(slope > -1 && slope < 1){
		error = -dx / 2;
		oldyv > 0 ? step = 1 : step = -1;
		if(oldxv > 0){
			while(*xv < oldxv){
				for(std::vector<Object *>::iterator i = test.begin(); i != test.end(); i++){
					Object * object = *i;
					if(TestAABB(x1 + *xv, y1 + *yv, x2 + *xv, y2 + *yv, object, types)){
						*yv = yv0;
						*xv = xv0;
						return object;
					}
				}
				error += dy * step;
				if(error >= 0){
					yv0 = *yv;
					*yv += step;
					error -= dx;
				}
				xv0 = *xv;
				(*xv)++;
			}
		}else{
			while(*xv > oldxv){
				for(std::vector<Object *>::iterator i = test.begin(); i != test.end(); i++){
					Object * object = *i;
					if(TestAABB(x1 + *xv, y1 + *yv, x2 + *xv, y2 + *yv, object, types)){
						*yv = yv0;
						*xv = xv0;
						return object;
					}
				}
				error += dy * -step;
				if(error <= 0){
					yv0 = *yv;
					*yv += step;
					error -= dx;
				}
				xv0 = *xv;
				(*xv)--;
			}
		}
	}else{
		error = -dy / 2;
		oldxv > 0 ? step = 1 : step = -1;
		if(oldyv > 0){
			while(*yv < oldyv){
				for(std::vector<Object *>::iterator i = test.begin(); i != test.end(); i++){
					Object * object = *i;
					if(TestAABB(x1 + *xv, y1 + *yv, x2 + *xv, y2 + *yv, object, types)){
						*xv = xv0;
						*yv = yv0;
						return object;
					}
				}
				error += dx * step;
				if(error >= 0){
					xv0 = *xv;
					*xv += step;
					error -= dy;
				}
				yv0 = *yv;
				(*yv)++;
			}
		}else{
			while(*yv > oldyv){
				for(std::vector<Object *>::iterator i = test.begin(); i != test.end(); i++){
					Object * object = *i;
					if(TestAABB(x1 + *xv, y1 + *yv, x2 + *xv, y2 + *yv, object, types)){
						*xv = xv0;
						*yv = yv0;
						return object;
					}
				}
				error += dx * -step;
				if(error <= 0){
					xv0 = *xv;
					*xv += step;
					error -= dy;
				}
				yv0 = *yv;
				(*yv)--;
			}
		}
	}
	for(std::vector<Object *>::iterator i = test.begin(); i != test.end(); i++){
		Object * object = *i;
		if(TestAABB(x1 + *xv, y1 + *yv, x2 + *xv, y2 + *yv, object, types)){
			*xv = xv0;
			*yv = yv0;
			return object;
		}
	}
	return 0;
}
