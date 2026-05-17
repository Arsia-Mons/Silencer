#ifndef GAMEMODE_H
#define GAMEMODE_H

#include "shared.h"

class World;
class Player;
class Object;
class Team;

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

	virtual GameModeId Id() const = 0;
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

	// Returns true when the match should end.
	virtual bool IsMatchOver(const World&) const = 0;

	// Returns the winning team id, or 0 for no winner / draw.
	virtual Uint16 WinningTeamId(const World&) const { return 0; }
};

// ── Concrete modes ────────────────────────────────────────────────────────────

// Existing gameplay: deliver secretsNeededToWin secrets to base.
// Win detection stays in Team::Tick(); this class simply delegates.
class DataRetrievalMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_DATA_RETRIEVAL; }
	const char* Name() const override { return "Data Retrieval"; }
	bool IsMatchOver(const World& w) const override;
	Uint16 WinningTeamId(const World& w) const override;
};

// Stubs — IsMatchOver always returns false until their phase issue lands.
class DeathmatchMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_DEATHMATCH; }
	const char* Name() const override { return "Deathmatch"; }
	bool IsMatchOver(const World&) const override { return false; }
};

class TeamDeathmatchMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_TEAM_DEATHMATCH; }
	const char* Name() const override { return "Team Deathmatch"; }
	bool IsMatchOver(const World&) const override { return false; }
};

class SurvivalMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_SURVIVAL; }
	const char* Name() const override { return "Survival"; }
	bool IsMatchOver(const World&) const override { return false; }
};

class ExtractionMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_EXTRACTION; }
	const char* Name() const override { return "Extraction"; }
	bool IsMatchOver(const World&) const override { return false; }
};

class AssassinationMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_ASSASSINATION; }
	const char* Name() const override { return "Assassination"; }
	bool IsMatchOver(const World&) const override { return false; }
};

class SabotageMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_SABOTAGE; }
	const char* Name() const override { return "Sabotage"; }
	bool IsMatchOver(const World&) const override { return false; }
};

class ManhuntMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_MANHUNT; }
	const char* Name() const override { return "Manhunt"; }
	bool IsMatchOver(const World&) const override { return false; }
};

class ControlPointsMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_CONTROL_POINTS; }
	const char* Name() const override { return "Control Points"; }
	bool IsMatchOver(const World&) const override { return false; }
};

class EscortMode : public GameMode {
public:
	GameModeId  Id()   const override { return GAMEMODE_ESCORT; }
	const char* Name() const override { return "Escort"; }
	bool IsMatchOver(const World&) const override { return false; }
};

// Factory — returns a heap-allocated GameMode for the given id.
// Defaults to DataRetrievalMode for unknown ids.
GameMode* GameModeFactory(GameModeId id);

#endif
