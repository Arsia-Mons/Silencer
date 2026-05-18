#ifndef GAMESTATEOBJECT_H
#define GAMESTATEOBJECT_H

#include "shared.h"
#include "object.h"
#include "gamemode.h"

// Replicated object that holds current match state. Created once by the
// authority at match start; replicas receive it via snapshot like any Object.
// Authority writes all fields via Tick(); replicas just read them.
class GameStateObject : public Object {
public:
	GameStateObject();
	void Serialize(bool write, Serializer& data, Serializer* old = nullptr);
	void Tick(World& world);

	GameModeId modeId        = GAMEMODE_DATA_RETRIEVAL;
	Uint8      matchPhase    = 0;  // 0=warmup 1=active 2=over
	Uint16     matchTimeSecs = 0;  // elapsed match seconds
	Uint16     score[6]      = {}; // per-team score (index = team slot, up to World::maxteams)
};

#endif
