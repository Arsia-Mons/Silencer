#include "basedoor.h"
#include "player.h"
#include "../gas/gasloader.h"

BaseDoor::BaseDoor() : Object(ObjectTypes::BASEDOOR){
	requiresauthority = true;
	state_i = 0;
	res_bank = 101;
	res_index = 0;
	teamid = 0;
	renderpass = 1;
	teamnumber = 0;
	for(int i = 0; i < World::maxteams; i++){
		discoveredby[i] = false;
		enteredby[i] = false;
	}
}

void BaseDoor::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state_i, old);
	data.Serialize(write, color, old);
	data.Serialize(write, teamid, old);
	data.Serialize(write, teamnumber, old);
	for(int i = 0; i < World::maxteams; i++){
		data.Serialize(write, discoveredby[i], old);
	}
}

void BaseDoor::Tick(World & world){
	if(state_i == 0){
		{ const GameObjectDef* _d = GASLoader::Get().GetGameObjectDef("baseDoor");
		EmitSound(world, world.resources.soundbank[(_d && !_d->soundOpen.empty()) ? _d->soundOpen : "portal1.wav"], 64); }
	}
	if(state_i < 41){
		CheckForPlayersInView(world);
		res_bank = 101;
		res_index = state_i;
	}
	if(state_i >= 41){
		res_bank = 100;
		res_index = (state_i - 41) / 4;
	}
	if(state_i >= 41 + (4 * 4)){
		state_i = 40;
	}
	state_i++;
}

void BaseDoor::Respawn(void){
	state_i = 0;
	for(int i = 0; i < World::maxteams; i++){
		discoveredby[i] = false;
		enteredby[i] = false;
	}
}

void BaseDoor::SetTeam(Team & team){
	teamid = team.id;
	teamnumber = team.number;
	color = team.GetColor();
}

void BaseDoor::CheckForPlayersInView(World & world){
	std::vector<Uint8> types;
	types.push_back(ObjectTypes::PLAYER);
	{ const GameObjectDef* _bd = GASLoader::Get().GetGameObjectDef("baseDoor");
	  int _dw = _bd ? _bd->detectionWidth  : 320;
	  int _dh = _bd ? _bd->detectionHeight : 240;
	  std::vector<Object *> objects = world.TestAABB(x - _dw, y - _dh, x + _dw, y + _dh, types);
	  for(std::vector<Object *>::iterator it = objects.begin(); it != objects.end(); it++){
		Player * player = static_cast<Player *>(*it);
		Team * team = player->GetTeam(world);
		if(team){
			discoveredby[team->number] = true;
		}
	  }
	}
}