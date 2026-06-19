#ifndef HITTABLE_H
#define HITTABLE_H

#include "shared.h"
#include "serializer.h"

class Hittable
{
public:
	Hittable();
	void Serialize(bool write, Serializer & data, Serializer * old = 0);
	void Tick(class Object & object, class World & world);
	void HandleHit(class Object & object, class World & world, Uint8 x, Uint8 y, class Object & projectile);
	
	friend class Renderer;

	bool IsAlive() const { return health > 0; }
	Uint16 GetHealth() const { return health; }
	Uint16 GetMaxHealth() const { return maxhealth; }
	Uint16 GetShield() const { return shield; }
	void ApplyDamage(Uint16 dmg) { health = (health > dmg) ? health - dmg : 0; }

	// Set both health and maxhealth together (e.g. during actor init).
	void SetHealth(Uint16 hp) { health = hp; maxhealth = hp; }

	bool rumbleHit = false;  // set by HandleHit; consumed by Game::TickRumble

	bool destructible = false; // if true, emits ACTOR_KILLED via EventBus on deathprotected:
	Uint16 health;
	Uint16 maxhealth = 0;
	Uint16 shield;
	Uint8 state_hit;
	Uint8 hitx;
	Uint8 hity;
};

#endif