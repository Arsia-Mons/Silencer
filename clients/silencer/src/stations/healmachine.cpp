#include "healmachine.h"
#include "gasloader.h"
#include "audio/soundcue.h"

HealMachine::HealMachine() : Object(ObjectTypes::HEALMACHINE){
	res_bank = 172;
	res_index = 6;
	renderpass = 2;
	state_i = 0;
	cooldown = 0;
	requiresauthority = true;
}

void HealMachine::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state_i, old);
}

void HealMachine::Tick(World & world){
	//if(!world->replaying){
		if(cooldown){
			cooldown--;
		}
		if(state_i > 0){
			if(state_i == 2){
				const GameObjectDef* _d = GASLoader::Get().GetGameObjectDef("healMachine");
				const std::string& sfx = (_d && !_d->soundHeal.empty()) ? _d->soundHeal : "if15.wav";
				auto _r = ResolveSound(sfx, world.resources);
				if(_r.chunk) EmitSound(world, _r.chunk, static_cast<int>(96 * _r.volume));
			}
			state_i++;
			if(state_i >= 10){
				state_i = 0;
			}
		}
	//}
}

bool HealMachine::Activate(void){
	if(cooldown == 0){
		const GameObjectDef* def = GASLoader::Get().GetGameObjectDef("healMachine");
		cooldown = def ? def->cooldownTicks : 240;
		state_i = 1;
		return true;
	}
	return false;
}