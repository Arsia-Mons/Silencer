#include "healmachine.h"
#include "gasloader.h"

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
				{ const GameObjectDef* _d = GASLoader::Get().GetGameObjectDef("healMachine");
				EmitSound(world, world.resources.soundbank[(_d && !_d->soundHeal.empty()) ? _d->soundHeal : "if15.wav"], 96); }
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