#include "blasterprojectile.h"
#include "shrapnel.h"
#include "overlay.h"
#include "gasloader.h"
#include "audio/soundcue.h"

BlasterProjectile::BlasterProjectile() : Object(ObjectTypes::BLASTERPROJECTILE){
	requiresauthority = true;
	res_bank = 0xFF;
	res_index = 0;
	state_i = 0;
	const WeaponDef* w = GASLoader::Get().GetWeaponDef("blaster");
	healthdamage = w ? w->healthDamage : 40;
	shielddamage = w ? w->shieldDamage : 4;
	moveamount = w ? w->moveAmount : 10;
	renderpass = 2;
	isprojectile = true;
	isphysical = true;
	snapshotinterval = (w && w->snapshotInterval) ? w->snapshotInterval : 6;
}

void BlasterProjectile::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state_i, old);
}

void BlasterProjectile::Tick(World & world){
	const WeaponDef* w = GASLoader::Get().GetWeaponDef("blaster");
	const std::vector<int>& sb = w ? w->spriteBanks : std::vector<int>();
	auto bank = [&](int i, int fb) -> int { return (int)sb.size() > i ? sb[i] : fb; };
	if(yv < 0 && xv == 0)  res_bank = bank(0, 160); // up
	if(yv < 0 && xv > 0)   res_bank = bank(1, 161); // up-right
	if(yv == 0 && xv > 0)  res_bank = bank(2, 162); // right
	if(yv > 0 && xv > 0)   res_bank = bank(3, 163); // down-right
	if(yv > 0 && xv == 0)  res_bank = bank(4, 164); // down
	if(yv > 0 && xv < 0)   res_bank = bank(5, 163); // down-left
	if(yv == 0 && xv < 0)  res_bank = bank(6, 162); // left
	if(yv < 0 && xv < 0)   res_bank = bank(7, 161); // up-left
	Uint8 life = (w && w->projectileLife) ? (Uint8)w->projectileLife : 6;
	if(state_i == 4){
		const std::string& sfx = w && !w->soundFire.empty() ? w->soundFire : "!laserme.wav";
		auto _r = ResolveSound(sfx, world.resources);
		if(_r.chunk) EmitSound(world, _r.chunk, static_cast<int>(128 * _r.volume));
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
				const std::string& hitSlot = w && !w->soundHit1.empty() ? w->soundHit1 : "rico1.wav";
				auto _r = ResolveSound(hitSlot, world.resources);
				if(_r.chunk) overlay->EmitSound(world, _r.chunk, static_cast<int>(32 * _r.volume));
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