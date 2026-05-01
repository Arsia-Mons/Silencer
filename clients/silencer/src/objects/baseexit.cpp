#include "baseexit.h"
#include "../gas/gasloader.h"

BaseExit::BaseExit() : Object(ObjectTypes::BASEEXIT){
	requiresauthority = false;
	teamid = 0;
	soundchannel = -1;
	draw = false;
}

void BaseExit::Tick(World & world){
	if(soundchannel == -1){
		const GameObjectDef* _d = GASLoader::Get().GetGameObjectDef("baseExit");
		soundchannel = EmitSound(world, world.resources.soundbank[(_d && !_d->soundAmbient.empty()) ? _d->soundAmbient : "wndloopc.wav"], 16, true);
	}
}