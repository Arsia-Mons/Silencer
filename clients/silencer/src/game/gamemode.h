#ifndef GAMEMODE_H
#define GAMEMODE_H

#include "shared.h"

class World;
class Player;
class Object;
class Team;
class GameStateObject;

// Numeric id transmitted in GameStateObject and game config.
enum GameModeId : Uint8 {
	GAMEMODE_DATA_RETRIEVAL  = 0,
	GAMEMODE_DEATHMATCH      = 1,
	GAMEMODE_TEAM_DEATHMATCH = 2,
	GAMEMODE_SURVIVAL        = 3,
	GAMEMODE_EXTRACTION      = 4,
	GAMEMODE_ASSASSINATION   = 5,
	GAMEMODE_SABOTAGE        = 6,
	GAMEMODE_MANHUNT         = 7,
	GAMEMODE_CONTROL_POINTS  = 8,
	GAMEMODE_ESCORT          = 9,
};

// Authority-only base class. One instance owned by World on the authority peer.
// Replicas never instantiate a GameMode — they read GameStateObject instead.
class GameMode {
public:
	virtual ~GameMode() = default;

	virtual GameModeId  Id()   const = 0;
	virtual const char* Name() const = 0;

	// Called once by World when the match becomes active (all peers ready).
	virtual void OnMatchStart(World&) {}

	// Called every world tick after all objects have ticked.
	virtual void Tick(World&) {}

	// Called by Team when a secret has been delivered to base.
	virtual void OnSecretDelivered(World&, Team&) {}

	// Called by Player::HandleHit when a player's health reaches zero.
	virtual void OnPlayerDied(World&, Player& victim, Object* attacker) {
		(void)victim; (void)attacker;
	}

	// Called when all players on a team are eliminated (dead/disconnected).
	// Useful for last-team-standing logic (Survival, Manhunt).
	virtual void OnTeamEliminated(World&, Team&) {}

	// Called when a team completes a mode-specific objective (capture point,
	// deliver item, plant bomb, etc.). Meaning is mode-defined.
	virtual void OnObjectiveCaptured(World&, Team&) {}

	// Called once by World when winningteamid is first set (match over).
	// Override to broadcast results, apply bonuses, record stats, etc.
	virtual void OnMatchEnd(World&) {}

	// Called each authority tick by GameStateObject to let the mode write
	// per-team scores into gso.score[]. Index = team.number.
	virtual void UpdateScores(GameStateObject&, const World&) const {}

	// Called by Player when deciding whether a dead player may respawn.
	// Returns the GAS per-mode respawn flag by default.
	virtual bool AllowRespawn(const World&) const;

	// Returns true when the match should end.
	virtual bool IsMatchOver(const World&) const = 0;

	// Returns the winning team id, or 0 for no winner / draw.
	virtual Uint16 WinningTeamId(const World&) const { return 0; }
};

// Factory — returns a heap-allocated GameMode for the given id.
// Defaults to DataRetrievalMode for unknown ids.
GameMode* GameModeFactory(GameModeId id);

#endif

