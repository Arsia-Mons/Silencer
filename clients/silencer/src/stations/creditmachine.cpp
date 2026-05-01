#include "creditmachine.h"
#include "../gas/gasloader.h"

CreditMachine::CreditMachine() : Object(ObjectTypes::CREDITMACHINE){
	res_bank = 80;
	res_index = 0;
	state_i = 0;
	requiresauthority = true;
}

void CreditMachine::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state_i, old);
}

void CreditMachine::Tick(World & world){
	if(!world.replaying){
		res_index = state_i;
		if(state_i > 0){
			if(state_i == 4){
				{ const GameObjectDef* _d = GASLoader::Get().GetGameObjectDef("creditMachine");
				EmitSound(world, world.resources.soundbank[(_d && !_d->soundPurchase.empty()) ? _d->soundPurchase : "pwrcon1.wav"], 96); }
			}
			state_i++;
			if(state_i >= 18){
				state_i = 0;
			}
		}
	}
}

void CreditMachine::Activate(void){
	if(state_i == 0){
		state_i = 1;
	}
}