#ifndef OBJECT_H
#define OBJECT_H

#include "shared.h"
#include "serializer.h"
#include "input.h"
#include "world.h"
#include "sprite.h"
#include "physical.h"
#include "hittable.h"
#include "bipedal.h"
#include "projectile.h"

class Object : public Sprite, public Physical, public Hittable, public Bipedal, public Projectile
{
public:
	Object(Uint8 type);
	bool RequiresAuthority(void);
	virtual void Tick(class World & world);
	virtual void Serialize(bool write, Serializer & data, Serializer * old = 0);
	virtual void OnDestroy(class World & world);
	virtual void HandleHit(class World & world, Uint8 x, Uint8 y, class Object & projectile);
	virtual void HandleInput(Input & input);
	virtual void HandleDisconnect(World & world, Uint8 peerid);
	int EmitSound(class World & world, Mix_Chunk * chunk, Uint8 volume = 128, bool loop = false);
	// Play sound at full volume globally (no distance/occlusion attenuation).
	int EmitGlobalSound(class World & world, Mix_Chunk * chunk, Uint8 volume = 128);
	bool requiresauthority;
	bool requiresmaptobeloaded;
	Uint8 type;
	Uint16 id;
	int snapshotinterval;
	Uint32 lastsnapshottick;
	bool wasdestroyed;
	bool issprite;
	bool isphysical;
	bool ishittable;
	bool isbipedal;
	bool isprojectile;
	bool iscontrollable;

	// ── Collectible item pickup ───────────────────────────────────────────
	bool collectible = false;  // emits ITEM_COLLECTED when a player overlaps
	bool collected   = false;  // set true after first pickup (for one-shot items)

	// ── Scripted movement (MOVE_ACTOR action) ────────────────────────────
	bool   is_moving      = false;
	Sint16 move_origin_x  = 0;
	Sint16 move_origin_y  = 0;
	Sint16 move_target_x  = 0;
	Sint16 move_target_y  = 0;
	float  move_duration  = 0.f; // total seconds for the move
	float  move_elapsed   = 0.f; // seconds elapsed so far
};

#endif