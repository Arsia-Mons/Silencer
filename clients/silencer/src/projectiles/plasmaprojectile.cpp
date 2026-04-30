#include "plasmaprojectile.h"
#include "plume.h"
#include "gasloader.h"

PlasmaProjectile::PlasmaProjectile() : Object(ObjectTypes::PLASMAPROJECTILE){
	requiresauthority = false;
	res_bank = 0xFF;
	res_index = 0;
	state_i = 0;
	const WeaponDef* w = GASLoader::Get().GetWeaponDef("plasma");
	healthdamage = w ? w->healthDamage : 4;
	shielddamage = w ? w->shieldDamage : 5;
	velocity = w ? w->velocity : 5;
	drawcheckered = true;
	renderpass = 3;
	large = false;
	moveamount = w ? w->moveAmount : 4;
	radius = w ? w->radius : 5;
	renderpass = 2;
	stopatobjectcollision = false;
	isprojectile = true;
	isphysical = true;
	snapshotinterval = 6;
}

void PlasmaProjectile::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state_i, old);
	data.Serialize(write, large, old);
}

void PlasmaProjectile::Tick(World & world){
	if(large){
		const WeaponDef* w = GASLoader::Get().GetWeaponDef("plasma");
		healthdamage = w ? w->healthDamageLarge : 5;
		shielddamage = w ? w->shieldDamageLarge : 6;
	}
	Plume * plume = (Plume *)world.CreateObject(ObjectTypes::PLUME);
	if(plume){
		if(large){
			plume->type = 7;
		}else{
			plume->type = 4;
		}
		plume->quick = true;
		/*if(state_i > 5){
			plume->state_i = state_i;
		}*/
		plume->renderpass = renderpass;
		plume->SetPosition(x, y);
	}
	Object * object = 0;
	Platform * platform = 0;
	if(TestCollision(*this, world, &platform, &object)){
		float xn = 0, yn = 0;
		if(platform){
			platform->GetNormal(x, y, &xn, &yn);
			if(xn){
				xv = (xn * abs(xv));
			}
			if(yn){
				yv = (yn * abs(yv));
			}
			xv *= 0.8;
			yv *= 0.8;
		}
		//world->MarkDestroyObject(id);
	}
	{ const WeaponDef* _gd = GASLoader::Get().GetWeaponDef("plasma"); yv += _gd ? _gd->plasmaGravity : 2; }
	res_index = state_i;
	const WeaponDef* _pgd = GASLoader::Get().GetWeaponDef("plasma");
	Uint8 life = _pgd ? _pgd->plasmaLifeNormal : 20;
	if(large){
		life = _pgd ? _pgd->plasmaLifeLarge : 7;
	}
	if(state_i >= life){
		world.MarkDestroyObject(id);
	}
	state_i++;
}