#include "ui/world_session_model.h"
#include "ui/game_ui_pipeline.h"

#include "game.h"
#include "world.h"
#include "objecttypes.h"
#include "player.h"
#include "peer.h"
#include "gamestateobject.h"
#include "gamemode.h"
#include "buyableitem.h"
#include "basedoor.h"
#include "team.h"
#include "lobby.h"
#include "actor/user.h"
#include "gasloader.h"

#include <SDL3/SDL_timer.h>

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
      snap.inventory_res_index[i] =
          game.GetRenderer().InvIdToResIndex(p->inventoryitems[i]);
      const char *letter = Renderer::InvIdToLetter(p->inventoryitems[i]);
      snap.inventory_letters[i] = letter && letter[0] ? letter[0] : 0;
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
    snap.chat_caret_on = (SDL_GetTicks() / 50) % 32 < 16;
    snap.chat_text = p->chatText;
    if (p->isbuying || p->techstationactive) {
      // Port of origin HudView.cpp PopulateBuyTech: visible 5-row scroll
      // window, give-row peer names, UP/DOWN/repair price column, wall-clock
      // selection pulse (SDL_GetTicks/50, brightness 128..136), and the
      // per-row icon at that brightness.
      const bool tech = p->techstationactive;
      Team *buyTechTeam = p->GetTeam(world);
      Team *otherteam = nullptr;
      if (!p->InOwnBase(world)) {
        for (Uint16 tid : world.GetObjectsByType(ObjectTypes::TEAM)) {
          Team *t = static_cast<Team *>(world.GetObjectFromId(tid));
          if (t && t != buyTechTeam)
            otherteam = t;
        }
      }
      std::vector<BuyableItem *> items;
      p->CollectBuyMenuItems(world, tech, items);
      int selecteditem = tech ? p->techifacelastitem : p->buyifacelastitem;
      int scrolled = tech ? p->techifacelastscrolled : p->buyifacelastscrolled;
      if (selecteditem < 0)
        selecteditem = 0;
      if (!items.empty() && selecteditem >= (int)items.size())
        selecteditem = (int)items.size() - 1;
      if (scrolled < 0)
        scrolled = 0;
      snap.buytech_selected = selecteditem;
      Uint8 uiTick = (Uint8)(SDL_GetTicks() / 50);
      Uint8 selectedBrightness = 128;
      if (uiTick % 16 >= 8)
        selectedBrightness += (uiTick % 8);
      else
        selectedBrightness += 8 - (uiTick % 8);
      GameUiPipeline &pipe = game.GetUiPipeline();
      const Resources &res = world.resources;
      for (int i = scrolled;
           i < (int)items.size() && (int)snap.buytech_items.size() < 5; ++i) {
        BuyableItem *bi = items[i];
        client::ui::TechItem row;
        row.index = i;
        row.label = bi->name;
        int peerIndex = -1;
        if (bi->id == World::BUY_GIVE0) peerIndex = 0;
        if (bi->id == World::BUY_GIVE1) peerIndex = 1;
        if (bi->id == World::BUY_GIVE2) peerIndex = 2;
        if (bi->id == World::BUY_GIVE3) peerIndex = 3;
        if (peerIndex >= 0 && buyTechTeam) {
          Peer *targetPeer = world.GetPeer(buyTechTeam->peers[peerIndex]);
          if (targetPeer) {
            User *user = world.lobby.GetUserInfo(targetPeer->accountid);
            if (user)
              row.label += user->DisplayName();
          }
        }
        if (p->isbuying) {
          row.price = (buyTechTeam && (buyTechTeam->disabledtech & bi->techchoice))
                          ? "DOWN"
                          : std::to_string(bi->price);
        } else if (!p->InOwnBase(world)) {
          row.price = (otherteam && (otherteam->disabledtech & bi->techchoice))
                          ? "DOWN"
                          : std::to_string(bi->repairprice);
        } else if (buyTechTeam && (buyTechTeam->disabledtech & bi->techchoice)) {
          row.price = std::to_string(bi->repairprice);
        } else {
          row.price = "UP";
        }
        row.selected = (i == selecteditem);
        row.brightness = row.selected ? selectedBrightness : (Uint8)128;
        row.icon =
            pipe.EnsureHudRampVariant(bi->res_bank, bi->res_index, 0, 0,
                                      row.brightness);
        if (bi->res_bank < res.spritebank.size() &&
            bi->res_index < res.spritebank[bi->res_bank].size()) {
          Surface *icon = res.spritebank[bi->res_bank][bi->res_index].get();
          if (icon) {
            row.icon_w = (uint16_t)icon->w;
            row.icon_h = (uint16_t)icon->h;
          }
        }
        if (bi->res_bank < res.spriteoffsetx.size() &&
            bi->res_index < res.spriteoffsetx[bi->res_bank].size())
          row.icon_ox = (int16_t)res.spriteoffsetx[bi->res_bank][bi->res_index];
        if (bi->res_bank < res.spriteoffsety.size() &&
            bi->res_index < res.spriteoffsety[bi->res_bank].size())
          row.icon_oy = (int16_t)res.spriteoffsety[bi->res_bank][bi->res_index];
        snap.buytech_items.push_back(std::move(row));
      }
      snap.buytech_footer =
          (p->isbuying || p->InOwnBase(world))
              ? "Available Credits: " + std::to_string((int)p->credits)
              : "Viruses Available: " +
                    std::to_string((int)p->InventoryItemCount(Player::INV_VIRUS));
    }
  }
  snap.chat_show_ticks = world.messaging.showchat_i;
  snap.show_player_list = world.IsShowingPlayerList();

  // --- in-game HUD (use_hud): pulse clock, message reveal, team strip ---
  snap.hud_phase = game.GetRenderer().GetHudAnimationPhase();
  snap.hud_tick = world.tickcount;
  snap.quit_state = world.quitstate;
  snap.message_i = world.messaging.message_i;
  snap.message_type = world.messaging.messagetype;
  snap.message_time = world.messaging.messagetime;
  if (p)
    snap.poisoned = p->GetPoisonedBy() != 0;
  GameUiPipeline &pipe = game.GetUiPipeline();
  const Uint8 pulse = snap.hud_phase;
  for (Uint16 tid : world.GetObjectsByType(ObjectTypes::TEAM)) {
    Team *team = static_cast<Team *>(world.GetObjectFromId(tid));
    if (!team)
      continue;
    client::ui::WorldSessionSnapshot::HudTeamStrip row;
    row.secrets = team->secrets;
    row.beaming = team->beamingterminalid != 0;
    row.num_peers = team->numpeers;
    for (int i = 0; i < row.num_peers && i < 4; ++i) {
      Peer *tp = world.GetPeer(team->peers[i]);
      if (!tp)
        continue;
      Player *pp = world.GetPeerPlayer(tp->id);
      if (!pp)
        continue;
      auto &slot = row.peers[i];
      slot.present = true;
      // Player's state enum is private; 20/21 = DYING/DEAD (origin
      // HudClayHelpers kPlayerStateDead = 21).
      slot.dead = pp->GetState() == 21 || pp->GetState() == 20;
      slot.has_secret = pp->hassecret;
      User *user = world.lobby.GetUserInfo(tp->accountid);
      if (user) {
        slot.name = user->DisplayName();
        if (tp->isbot)
          slot.name += " [BOT]";
        slot.level = user->agency[team->agency].level;
        slot.endurance = user->agency[team->agency].endurance;
        slot.shield = user->agency[team->agency].shield;
        slot.jetpack = user->agency[team->agency].jetpack;
        slot.hacking = user->agency[team->agency].hacking;
        slot.contacts = user->agency[team->agency].contacts;
      }
      // origin hud_teams.cpp pulses: in-base ramp 210 (triangle on phase>>2,
      // period 8), has-secret ramp 114 (triangle on phase, period 16).
      Uint8 rampColor = 0, rampPlus = 0;
      if (pp->InBase(world) || pp->hassecret) {
        Uint8 time = 4, shift = 2;
        rampColor = 210;
        if (pp->hassecret) {
          time = 8;
          rampColor = 114;
          shift = 0;
        }
        if ((pulse >> shift) % (time * 2) < time)
          rampPlus += (pulse >> shift) % time;
        else
          rampPlus += time - ((pulse >> shift) % time);
      }
      slot.sprite = pipe.EnsureHudRampVariant(
          103, (uint16_t)((slot.dead ? 8 : 4) + i), rampColor, rampPlus);
    }
    client::ui::ChromeTextures::Sprite emblem =
        pipe.EnsureHudTeamEmblem(team->agency, team->GetColor());
    row.emblem = emblem.id;
    row.emblem_w = emblem.w;
    row.emblem_h = emblem.h;
    snap.hud_teams.push_back(row);
  }

  // Chat scrollback (oldest -> newest). The HUD chat box shows the last few
  // lines above the active compose line; cap so the box stays compact.
  {
    const std::deque<std::string> &lines = world.messaging.chatlines;
    const size_t kMaxLog = 4;
    size_t start = lines.size() > kMaxLog ? lines.size() - kMaxLog : 0;
    for (size_t i = start; i < lines.size(); ++i)
      snap.chat_log.push_back(lines[i]);
  }

  // Scoreboard roster: one row per connected peer with a controlled player. The
  // display name resolves through the lobby's account-keyed user info (charname),
  // falling back to the local username then a neutral placeholder. Stat columns
  // come from the peer's replicated Stats (kills/score/secrets/jets/hacks/
  // contacts); all-zero at match start, matching the golden.
  Lobby &lobby = world.lobby;
  Uint8 localId = world.GetLocalPeerId();
  for (unsigned int i = 0; i < WorldPeerRegistry::maxpeers; ++i) {
    Peer *peer = world.peers.peerlist[i];
    if (!peer)
      continue;
    Player *pp = world.GetPeerPlayer(peer->id);
    if (!pp)
      continue;
    client::ui::ScoreboardPlayer row;
    bool isLocal = (peer->id == localId);
    const char *name = nullptr;
    User *ui = lobby.GetUserInfo(peer->accountid);
    if (ui && ui->DisplayName() && ui->DisplayName()[0])
      name = ui->DisplayName();
    else if (isLocal && lobby.GetLocalUsername() && lobby.GetLocalUsername()[0])
      name = lobby.GetLocalUsername();
    row.name = (name && name[0]) ? name : "AgentZero";
    Team *team = world.GetPeerTeam(peer->id);
    row.team_number = team ? team->number : 0;
    row.local = isLocal;
    Stats &st = peer->stats;
    row.kills = (int)st.kills;
    row.score = (int)st.CalculateExperience();
    row.secrets = (int)st.secretsreturned;
    row.jets = 0;
    row.hacks = (int)st.fileshacked;
    row.contacts = 0;
    snap.players.push_back(std::move(row));
  }

  return snap;
}

InGameUiControlResult ConfigureInGameUi(Game &game, InGameUiMode mode,
                                        const InGameUiChatSeed &chat) {
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
    if (!chat.line.empty()) {
      world.messaging.chatlines.push_back(chat.line);
      while ((int)world.messaging.chatlines.size() >
             GASLoader::Get().gameengine.chatMaxLines)
        world.messaging.chatlines.pop_front();
    } else {
      player->chatActive = true;
      player->chatwithteam = false;
      std::strncpy(player->chatText, chat.text.c_str(),
                   sizeof(player->chatText) - 1);
      player->chatText[sizeof(player->chatText) - 1] = '\0';
    }
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
