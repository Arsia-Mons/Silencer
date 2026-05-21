#include "laserprojectile.h"
#include "shrapnel.h"
#include "overlay.h"
#include "gasloader.h"
#include "audio/soundcue.h"

LaserProjectile::LaserProjectile() : Object(ObjectTypes::LASERPROJECTILE){
	requiresauthority = true;
	res_bank = 0xFF;
	res_index = 0;
	state_i = 0;
	const WeaponDef* w = GASLoader::Get().GetWeaponDef("laser");
	healthdamage = w ? w->healthDamage : 10;
	shielddamage = w ? w->shieldDamage : 60;
	velocity = w ? w->velocity : 30;
	emitoffset = (w && w->emitOffset) ? w->emitOffset : 24;
	moveamount = w ? w->moveAmount : 12;
	renderpass = 2;
	isprojectile = true;
	isphysical = true;
	snapshotinterval = (w && w->snapshotInterval) ? w->snapshotInterval : 6;
}

void LaserProjectile::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state_i, old);
}

void LaserProjectile::Tick(World & world){
	const WeaponDef* w = GASLoader::Get().GetWeaponDef("laser");
	const std::vector<int>& sb = w ? w->spriteBanks : std::vector<int>();
	auto bank = [&](int i, int fb) -> int { return (int)sb.size() > i ? sb[i] : fb; };
	if(yv < 0 && xv == 0)  res_bank = bank(0, 165); // up
	if(yv < 0 && xv > 0)   res_bank = bank(1, 166); // up-right
	if(yv == 0 && xv > 0)  res_bank = bank(2, 167); // right
	if(yv > 0 && xv > 0)   res_bank = bank(3, 168); // down-right
	if(yv > 0 && xv == 0)  res_bank = bank(4, 169); // down
	if(yv > 0 && xv < 0)   res_bank = bank(5, 168); // down-left
	if(yv == 0 && xv < 0)  res_bank = bank(6, 167); // left
	if(yv < 0 && xv < 0)   res_bank = bank(7, 166); // up-left
	Uint8 life = (w && w->projectileLife) ? (Uint8)w->projectileLife : 10;
	if(state_i == 1){
		const std::string& sfx = w && !w->soundFire.empty() ? w->soundFire : "!laserel.wav";
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