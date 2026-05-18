#include "gamemode.h"
#include "data_retrieval_mode.h"
#include "deathmatch_mode.h"
#include "team_deathmatch_mode.h"
#include "survival_mode.h"
#include "extraction_mode.h"
#include "assassination_mode.h"
#include "sabotage_mode.h"
#include "manhunt_mode.h"
#include "control_points_mode.h"
#include "escort_mode.h"

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

