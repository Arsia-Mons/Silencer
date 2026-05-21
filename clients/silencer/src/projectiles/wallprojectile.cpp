#include "wallprojectile.h"
#include "overlay.h"
#include "shrapnel.h"
#include "gasloader.h"
#include "audio/soundcue.h"

WallProjectile::WallProjectile() : Object(ObjectTypes::WALLPROJECTILE){
	requiresauthority = true;
	res_bank = 0xFF;
	res_index = 0;
	state_i = 0;
	const WeaponDef* w = GASLoader::Get().GetWeaponDef("wall");
	healthdamage = w ? w->healthDamage : 10;
	shielddamage = w ? w->shieldDamage : 60;
	velocity = w ? w->velocity : 35;
	emitoffset = (w && w->emitOffset) ? w->emitOffset : 0;
	moveamount = w ? w->moveAmount : 6;
	renderpass = 2;
	isprojectile = true;
	isphysical = true;
	snapshotinterval = (w && w->snapshotInterval) ? w->snapshotInterval : 6;
}

void WallProjectile::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state_i, old);
}

void WallProjectile::Tick(World & world){
	const WeaponDef* w = GASLoader::Get().GetWeaponDef("wall");
	Uint8 life = (w && w->projectileLife) ? (Uint8)w->projectileLife : 20;
	if(state_i == 1){
		const std::string& sfx = w && !w->soundFire.empty() ? w->soundFire : "!laserel.wav";
		auto _r = ResolveSound(sfx, world.resources);
		if(_r.chunk) EmitSound(world, _r.chunk, static_cast<int>(64 * _r.volume));
	}
	if(state_i < 7){
		res_index = state_i;
	}
	if(state_i >= 7){
		if(state_i > 12 + life){
			world.MarkDestroyObject(id);
			res_index = 12;
			return;
		}
		if(state_i >= 12 + life - 5){
			res_index = state_i - life;
		}else{
			res_index = 7;
		}
	}
	if(state_i >= 7){
		Object * object = 0;
		Platform * platform = 0;
		if(TestCollision(*this, world, &platform, &object)){
			Overlay * overlay = (Overlay *)world.CreateObject(ObjectTypes::OVERLAY);
			if(overlay){
				int hob = w && w->hitOverlayBank >= 0 ? w->hitOverlayBank : 222;
				overlay->res_bank = hob;
				overlay->x = x;
				overlay->y = y;
				if(platform){
					const std::string& hitSlot = w && !w->soundHit1.empty() ? w->soundHit1 : "strike03.wav";
					auto _r = ResolveSound(hitSlot, world.resources);
					if(_r.chunk) overlay->EmitSound(world, _r.chunk, static_cast<int>(96 * _r.volume));
				}
			}
			float xn = 0, yn = 0;
			if(platform){
				platform->GetNormal(x, y, &xn, &yn);
			}
			for(int i = 0; i < 8; i++){
				Shrapnel * shrapnel = (Shrapnel *)world.CreateObject(ObjectTypes::SHRAPNEL);
				if(shrapnel){
					shrapnel->x = x;
					shrapnel->y = y;
					shrapnel->xv = (rand() % 9) - 4;
					shrapnel->yv = (rand() % 9) - 8;
					shrapnel->xv = (xn * abs(shrapnel->xv)) + (rand() % 9) - 4;
					shrapnel->yv = (yn * abs(shrapnel->yv)) + (rand() % 9) - 8;
				}
			}
			world.MarkDestroyObject(id);
		}
	}
	state_i++;
}