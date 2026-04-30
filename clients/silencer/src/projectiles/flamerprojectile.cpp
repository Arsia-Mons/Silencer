#include "flamerprojectile.h"
#include "plume.h"
#include "gasloader.h"
#include <algorithm>

const int FlamerProjectile::MAX_PLUMES;
FlamerProjectile::FlamerProjectile() : Object(ObjectTypes::FLAMERPROJECTILE){
	requiresauthority = true;
	res_bank = 0xFF;
	res_index = 0;
	state_i = 0;
	bypassshield = true;
	const WeaponDef* w = GASLoader::Get().GetWeaponDef("flamer");
	healthdamage = w ? w->healthDamage : 2;
	shielddamage = w ? w->shieldDamage : 1;
	velocity = w ? w->velocity : 7;
	drawcheckered = true;
	plumecount = (w && w->trailPlumes > 0) ? std::min(w->trailPlumes, MAX_PLUMES) : 7;
	for(int i = 0; i < MAX_PLUMES; i++){
		plumeids[i] = 0;
	}
	emitoffset = (w && w->emitOffset) ? w->emitOffset : -7;
	moveamount = w ? w->moveAmount : 6;
	soundplaying = 0;
	renderpass = 2;
	radius = w ? w->radius : 10;
	stopatobjectcollision = false;
	isprojectile = true;
	isphysical = true;
	snapshotinterval = (w && w->snapshotInterval) ? w->snapshotInterval : 6;
	hitonce = false;
}

void FlamerProjectile::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state_i, old);
}

void FlamerProjectile::Tick(World & world){
	for(int i = 0; i < plumecount; i++){
		if(!plumeids[i]){
			Plume * plume = (Plume *)world.CreateObject(ObjectTypes::PLUME);
			if(plume){
				plume->type = 4;
				plume->xv = (rand() % 17) - 8 + (xv * 12);
				plume->yv = (rand() % 17) - 8 + (yv * 12);
				plume->SetPosition(x - (xv * ((i + 1) * 1)), y - (yv * ((i + 1) * 1)));
				plumeids[i] = plume->id;
				//plume->state_i = i;
				break;
			}
		}
	}
	Object * object = 0;
	Platform * platform = 0;
	if(TestCollision(*this, world, &platform, &object)){
		float xn = 0, yn = 0;
		if(platform){
			platform->GetNormal(x, y, &xn, &yn);
			xv /= 3;
			yv /= 3;
			for(int i = 0; i < plumecount; i++){
				Plume * plume = (Plume *)world.GetObjectFromId(plumeids[i]);
				if(plume){
					plume->xv /= 3;
					plume->yv /= 3;
					//world->MarkDestroyObject(plumeids[i]);
				}
			}
		}
		//world->MarkDestroyObject(id);
	}
	res_index = state_i;
	if(state_i >= 14){
		world.MarkDestroyObject(id);
	}
	state_i++;
}