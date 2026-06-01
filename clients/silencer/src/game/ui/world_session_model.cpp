#include "ui/world_session_model.h"

#include "game.h"
#include "world.h"
#include "objecttypes.h"
#include "player.h"
#include "gamestateobject.h"
#include "gamemode.h"

#include <vector>

namespace silencer::game_ui {
namespace {

// Replica-safe mode name (GameMode::Name() is authority-only; map the replicated
// modeId instead so the HUD labels the match for both authority and replicas).
const char *ModeName(uint8_t id) {
  switch ((GameModeId)id) {
  case GAMEMODE_DATA_RETRIEVAL:
    return "Data Retrieval";
  case GAMEMODE_DEATHMATCH:
    return "Deathmatch";
  case GAMEMODE_TEAM_DEATHMATCH:
    return "Team Deathmatch";
  case GAMEMODE_SURVIVAL:
    return "Survival";
  case GAMEMODE_EXTRACTION:
    return "Extraction";
  case GAMEMODE_ASSASSINATION:
    return "Assassination";
  case GAMEMODE_SABOTAGE:
    return "Sabotage";
  case GAMEMODE_MANHUNT:
    return "Manhunt";
  case GAMEMODE_CONTROL_POINTS:
    return "Control Points";
  case GAMEMODE_ESCORT:
    return "Escort";
  }
  return "Match";
}

} // namespace

client::ui::WorldSessionSnapshot
CaptureWorldSessionSnapshot(Game &game, client::ui::SessionPhase phase) {
  client::ui::WorldSessionSnapshot snap;
  using P = client::ui::SessionPhase;
  if (phase != P::InMatch && phase != P::SinglePlayer)
    return snap;

  World &world = game.GetWorld();

  // Viewed agent (the spectated peer's controlled player; self when not
  // spectating). Null while loading / dead / spectating an empty slot.
  Player *p = world.GetPeerPlayer(world.viewedpeerid);
  if (p) {
    snap.player_valid = true;
    snap.health = p->GetHealth();
    snap.max_health = p->maxhealth;
    snap.shield = p->GetShield();
    snap.max_shield = p->maxshield;
    snap.fuel = p->fuel;
    snap.max_fuel = p->maxfuel;
    snap.fuel_low = p->fuellow;
    snap.current_weapon = p->currentweapon;
    snap.laser_ammo = p->laserammo;
    snap.rocket_ammo = p->rocketammo;
    snap.flamer_ammo = p->flamerammo;
    snap.files = p->files;
    snap.max_files = p->maxfiles;
    snap.credits = p->credits;
    for (int i = 0; i < 4; ++i) {
      snap.inventory_items[i] = p->inventoryitems[i];
      snap.inventory_counts[i] = p->inventoryitemsnum[i];
    }
    snap.current_inventory = p->currentinventoryitem;
  }

  // Match state from the replicated GameStateObject (created by the authority at
  // match start; replicas receive it via snapshot). Valid only once the map has
  // loaded and the state object exists.
  const std::vector<Uint16> &gsoIds =
      world.GetObjectsByType(ObjectTypes::GAMESTATEOBJ);
  GameStateObject *gso =
      gsoIds.empty()
          ? nullptr
          : static_cast<GameStateObject *>(world.GetObjectFromId(gsoIds.front()));
  if (gso && world.map.loaded) {
    snap.match_valid = true;
    snap.mode_id = (uint8_t)gso->modeId;
    snap.mode_name = ModeName(snap.mode_id);
    snap.match_phase = gso->matchPhase;
    snap.match_time_secs = gso->matchTimeSecs;
    snap.winning_team_id = gso->winningTeamId;
    int teams = (int)world.GetObjectsByType(ObjectTypes::TEAM).size();
    if (teams > 6)
      teams = 6;
    for (int i = 0; i < teams; ++i)
      snap.scores.push_back(gso->score[i]);
  }

  // The current center status message (deploy/objective banners).
  const char *msg = world.messaging.GetMessageText();
  if (msg && msg[0])
    snap.message = msg;

  return snap;
}

} // namespace silencer::game_ui
