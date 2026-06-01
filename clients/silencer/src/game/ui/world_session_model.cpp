#include "ui/world_session_model.h"

#include "game.h"
#include "world.h"
#include "objecttypes.h"
#include "player.h"
#include "gamestateobject.h"
#include "gamemode.h"
#include "buyableitem.h"
#include "basedoor.h"
#include "team.h"
#include "gasloader.h"

#include <cstring>
#include <string>
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

  // In-match overlay read-state: the viewed agent's open station (buy XOR tech),
  // chat-compose state, and the world-owned player-list flag + roster. The buy
  // and tech menus share the row source (CollectBuyMenuItems); only one is open.
  if (p) {
    snap.buy_active = p->isbuying;
    snap.tech_active = p->techstationactive;
    snap.chat_active = p->chatActive;
    snap.chat_with_team = p->chatwithteam;
    snap.chat_text = p->chatText;
    if (p->isbuying || p->techstationactive) {
      const bool tech = p->techstationactive;
      snap.buytech_selected =
          tech ? p->techifacelastitem : p->buyifacelastitem;
      std::vector<BuyableItem *> items;
      p->CollectBuyMenuItems(world, tech, items);
      for (BuyableItem *bi : items) {
        client::ui::TechItem row;
        row.label = bi->name;
        if (tech) {
          row.detail = std::to_string((int)bi->techslots) +
                       (bi->techslots == 1 ? " slot" : " slots");
          row.affordable = true;
        } else {
          row.detail = "$" + std::to_string(bi->price);
          row.affordable = (int)p->credits >= bi->price;
        }
        snap.buytech_items.push_back(std::move(row));
      }
    }
  }
  snap.chat_show_ticks = world.messaging.showchat_i;
  snap.show_player_list = world.IsShowingPlayerList();
  // The named scoreboard roster (per-account display names) needs the protected
  // ingameusers/lobby seam; the scoreboard renders from the public per-team
  // scores for now and the roster is a follow-up. `players` stays empty.

  return snap;
}

InGameUiControlResult ConfigureInGameUi(Game &game, InGameUiMode mode) {
  InGameUiControlResult result;
  World &world = game.GetWorld();

  Player *player = world.GetPeerPlayer(world.viewedpeerid);
  if (!player) {
    result.error = "no viewed player";
    return result;
  }

  auto populate = [&]() {
    std::vector<BuyableItem *> buyItems;
    std::vector<BuyableItem *> techItems;
    player->CollectBuyMenuItems(world, false, buyItems);
    player->CollectBuyMenuItems(world, true, techItems);
    result.available = true;
    result.chat_active = player->chatActive;
    result.buy_active = player->isbuying;
    result.tech_active = player->techstationactive;
    result.show_chat_ticks = world.messaging.showchat_i;
    result.show_player_list = world.IsShowingPlayerList();
    result.buy_item_count = (int)buyItems.size();
    result.tech_item_count = (int)techItems.size();
    result.buy_selected_index = player->buyifacelastitem;
    result.tech_selected_index = player->techifacelastitem;
  };

  auto clear = [&]() {
    player->chatActive = false;
    player->chatText[0] = '\0';
    player->isbuying = false;
    player->techstationactive = false;
    world.messaging.showchat_i = 0;
    world.SetShowingPlayerList(false);
  };

  if (mode == InGameUiMode::Clear) {
    clear();
    populate();
    return result;
  }

  if (mode != InGameUiMode::Status)
    clear();

  if (mode == InGameUiMode::Chat || mode == InGameUiMode::All) {
    player->chatActive = true;
    player->chatwithteam = false;
    std::strncpy(player->chatText, "ui chat smoke",
                 sizeof(player->chatText) - 1);
    player->chatText[sizeof(player->chatText) - 1] = '\0';
    world.messaging.showchat_i = GASLoader::Get().gameengine.chatDisplayTicks;
  }
  if (mode == InGameUiMode::Buy || mode == InGameUiMode::All) {
    player->isbuying = true;
    player->buyifacelastitem = 0;
    player->buyifacelastscrolled = 0;
  }
  if (mode == InGameUiMode::Tech || mode == InGameUiMode::All) {
    // Tech needs a team + a base door to enter; in headless single-player there
    // may be neither, so seed a team and warp into its base (ports the legacy
    // control path verbatim).
    Team *team = player->GetTeam(world);
    const std::vector<Uint16> &teams = world.GetObjectsByType(ObjectTypes::TEAM);
    if (!team && !teams.empty())
      team = static_cast<Team *>(world.GetObjectFromId(teams[0]));
    if (!team) {
      team = static_cast<Team *>(world.CreateObject(ObjectTypes::TEAM));
      if (team) {
        team->agency = Team::NOXIS;
        team->number = 0;
        team->color = ((8 << 4) + 13);
      }
    }
    if (team) {
      player->SetTeamId(team->id);
      team->AddPeer(world.GetLocalPeerId());
    }
    BaseDoor *door = nullptr;
    if (team) {
      team->disabledtech = 0xffffffff;
      door = static_cast<BaseDoor *>(world.GetObjectFromId(team->basedoorid));
      for (Uint16 objectid : world.GetObjectsByType(ObjectTypes::BASEDOOR)) {
        auto *candidate =
            static_cast<BaseDoor *>(world.GetObjectFromId(objectid));
        if (candidate && candidate->teamid == team->id) {
          door = candidate;
          break;
        }
      }
    }
    if (door)
      player->SetBaseDoorEntering(door->id);
    if (team) {
      int baseY = ((world.map.height + 10) * 64) + (team->number * 26 * 64) + 64;
      player->y = static_cast<Sint16>(baseY);
    }
    player->techstationactive = true;
    player->techifacelastitem = 0;
    player->techifacelastscrolled = 0;
  }
  if (mode == InGameUiMode::PlayerList || mode == InGameUiMode::All)
    world.SetShowingPlayerList(true);

  populate();
  return result;
}

} // namespace silencer::game_ui
