#include "gamemode.h"
#include "world.h"

// ── DataRetrievalMode ─────────────────────────────────────────────────────────

bool DataRetrievalMode::IsMatchOver(const World& w) const {
	return w.GetWinningTeamId() != 0;
}

Uint16 DataRetrievalMode::WinningTeamId(const World& w) const {
	return w.GetWinningTeamId();
}

// ── Factory ───────────────────────────────────────────────────────────────────

GameMode* GameModeFactory(GameModeId id) {
	switch(id){
		case GAMEMODE_DEATHMATCH:      return new DeathmatchMode();
		case GAMEMODE_TEAM_DEATHMATCH: return new TeamDeathmatchMode();
		case GAMEMODE_SURVIVAL:        return new SurvivalMode();
		case GAMEMODE_EXTRACTION:      return new ExtractionMode();
		case GAMEMODE_ASSASSINATION:   return new AssassinationMode();
		case GAMEMODE_SABOTAGE:        return new SabotageMode();
		case GAMEMODE_MANHUNT:         return new ManhuntMode();
		case GAMEMODE_CONTROL_POINTS:  return new ControlPointsMode();
		case GAMEMODE_ESCORT:          return new EscortMode();
		case GAMEMODE_DATA_RETRIEVAL:
		default:                       return new DataRetrievalMode();
	}
}
