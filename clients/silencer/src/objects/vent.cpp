#include "vent.h"
#include "plume.h"
#include "../gas/gasloader.h"

Vent::Vent() : Object(ObjectTypes::VENT){
	requiresauthority = false;
	res_bank = 179;
	active = 0;
}

void Vent::Tick(World & world){
	if(active >= 1){
		if(active == 1){
			{ const GameObjectDef* _d = GASLoader::Get().GetGameObjectDef("vent");
			EmitSound(world, world.resources.soundbank[(_d && !_d->soundAmbient.empty()) ? _d->soundAmbient : "airvent2.wav"], 96); }
		}
		{ const GameObjectDef* _vd = GASLoader::Get().GetGameObjectDef("vent");
			int _dur  = _vd ? _vd->ventActiveDuration : 18;
			int _cyc  = _vd ? _vd->ventCycleTicks     : 20;
			int _cnt  = _vd ? _vd->ventPlumeCount     : 4;
			int _spX  = _vd ? _vd->ventSpreadX        : 80;
			int _spY  = _vd ? _vd->ventSpreadY        : 8;
			int _offY = _vd ? _vd->ventYOffset        : 3;
			int _bYV  = _vd ? _vd->ventBaseYV         : 30;
			int _rYV  = _vd ? _vd->ventYVRange        : 20;
			if(active <= _dur){
				for(int i = 0; i < _cnt; i++){
					Plume * plume = (Plume *)world.CreateObject(ObjectTypes::PLUME);
					if(plume){
						plume->type = rand() % 2;
						plume->cycle = true;
						plume->renderpass = 2;
						plume->SetPosition(x + (rand() % _spX - _spX/2), y - (rand() % _spY) + _offY);
						plume->yv = -(rand() % _rYV) - _bYV;
						plume->xv = (rand() % 5) - 2;
					}
				}
			}
			active++;
			if(active >= _cyc){ active = 0; }
		}
	}
	res_index = (state_i / 4) % 8;
	state_i++;
}

void Vent::Activate(void){
	if(active == 0){
		active = 1;
	}
}