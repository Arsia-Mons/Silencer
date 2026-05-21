#include "object.h"
#include "resources.h"
#include "audio.h"
#include "EventBus.h"
#include "gasloader.h"

Object::Object(Uint8 type){
	Object::type = type;
	id = 0;
	requiresauthority = false;
	requiresmaptobeloaded = true;
	snapshotinterval = -1;
	lasttick = 0;
	lastsnapshottick = 0;
	wasdestroyed = false;
	issprite = true;
	isphysical = false;
	ishittable = false;
	isbipedal = false;
	isprojectile = false;
	iscontrollable = false;
	collectible = false;
	collected   = false;
	is_moving   = false;
}

bool Object::RequiresAuthority(void){
	return requiresauthority;
}

void Object::Tick(World & world){
	const float dt = 1.f / GASLoader::Get().gameengine.ticksPerSecond;

	// Scripted movement (MOVE_ACTOR action)
	if (is_moving && move_duration > 0.f) {
		move_elapsed += dt;
		float t = move_elapsed / move_duration;
		if (t >= 1.f) {
			t = 1.f;
			is_moving = false;
		}
		x = static_cast<Sint16>(move_origin_x + (move_target_x - move_origin_x) * t);
		y = static_cast<Sint16>(move_origin_y + (move_target_y - move_origin_y) * t);
	}

	// Collectible item pickup (authority only; one-shot by default)
	if (collectible && !collected && world.IsAuthority()) {
		int r = 16; // overlap radius in world units
		std::vector<Uint8> playertypes = { ObjectTypes::PLAYER };
		std::vector<Object *> nearby = world.TestAABB(x - r, y - r, x + r, y + r, playertypes);
		if (!nearby.empty()) {
			collected = true;
			GameEvent ev;
			ev.type     = EventType::ITEM_COLLECTED;
			ev.actor_id = id;
			ev.player_id = nearby[0]->id;
			world.triggerGraph.Bus().Emit(ev);
		}
	}
}

void Object::Serialize(bool write, Serializer & data, Serializer * old){
	// type and id are not delta compressed so we can look up the object
	data.Serialize(write, type);
	data.Serialize(write, id);
	if(Serializer::WRITE == write && old){
		old->readoffset += sizeof(type) * 8;
		old->readoffset += sizeof(id) * 8;
	}
	//
	if(issprite){
		Sprite::Serialize(write, data, old);
	}
	if(isphysical){
		Physical::Serialize(write, data, old);
	}
	if(ishittable){
		Hittable::Serialize(write, data, old);
	}
	if(isbipedal){
		Bipedal::Serialize(write, data, old);
	}
	if(isprojectile){
		Projectile::Serialize(write, data, old);
	}
}

void Object::OnDestroy(World & world){
	
}

void Object::HandleHit(World & world, Uint8 x, Uint8 y, Object & projectile){

}

void Object::HandleInput(Input & input){
	
}

void Object::HandleDisconnect(World & world, Uint8 peerid){
	
}

int Object::EmitSound(class World & world, Mix_Chunk * chunk, Uint8 volume, bool loop, int maxInstances){
	return Audio::GetInstance().EmitSound(world, id, chunk, volume, loop, maxInstances);
}

int Object::EmitGlobalSound(class World & world, Mix_Chunk * chunk, Uint8 volume){
	return Audio::GetInstance().EmitSound(world, 0, chunk, volume, false);
}