#include "ui/lobby_ui_model.h"

#include "game.h"
#include "lobby.h"
#include "lobbygame.h"
#include "world.h"
#include "objecttypes.h"
#include "actor/team.h"
#include "actor/user.h"
#include "peer.h"
#include "buyableitem.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace silencer::game_ui {
namespace {

constexpr int kVisibleLogLines = 15;
// Keep the joined connect log within the per-call-site text scratch the screen
// renders it through (REACT_TEXT_STORAGE_CAP), preferring the recent tail.
constexpr size_t kStatusLogCap = 180;
// The lobby chat transcript wraps to multiple rows, so a single 180-byte tail
// would discard most of the visible scrollback (and clip a long message). The
// screen copies it through the per-frame string arena (copy_string), not the
// 192-byte text scratch, so this larger tail renders in full. Bounded (one
// well's worth of wrapped text), not unbounded.
constexpr size_t kLobbyChatLogCap = 900;

// Agency display names, indexed by Team::{NOXIS..BLACKROSE} / Character::agencyIdx.
const char *const kAgency[5] = {"Noxis", "Lazarus", "Caliber", "Static",
                                "Black Rose"};

std::string NormalizeTechDescription(const char *description) {
  if (!description || !description[0])
    return {};

  std::string out;
  bool pending_space = false;
  for (const unsigned char *p =
           reinterpret_cast<const unsigned char *>(description);
       *p; ++p) {
    if (std::isspace(*p)) {
      pending_space = true;
      continue;
    }
    if (pending_space && !out.empty())
      out.push_back(' ');
    out.push_back((char)*p);
    pending_space = false;
  }
  return out;
}

const char *SecurityLabel(Uint8 level) {
  switch (level) {
  case LobbyGame::SECHIGH:
    return "High";
  case LobbyGame::SECMEDIUM:
    return "Medium";
  case LobbyGame::SECLOW:
    return "Low";
  default:
    return "Open";
  }
}

// One origin summaryLine: label + spaces padding the value flush-right into
// the 30-char Body column (origin mission_summary_screen AddSummaryLine —
// kSummaryW 180 / advance 6; non-percentage values carry a trailing space).
void AddSummaryLine(std::vector<std::string> &out, const char *name,
                    Uint32 value, bool percentage = false) {
  char valuetext[64];
  snprintf(valuetext, sizeof(valuetext), "%u%s", (unsigned)value,
           percentage ? "%" : " ");
  std::string line = name;
  const int maxchars = 30;
  const int used = (int)(line.size() + strlen(valuetext));
  for (int i = 0; i < maxchars - used; i++)
    line += ' ';
  line += valuetext;
  out.push_back(line);
}

// Build the post-match progression fields from the local user's stats copy.
// Lobby mutex is held by the caller. Leaves `progression_loaded` false until
// the user record arrives. The summary lines mirror origin's Refresh()
// verbatim (mission_summary_screen.cpp) — order, labels, indent and padding.
void BuildProgression(client::ui::LobbySnapshot &snap, Lobby &lobby) {
  User *user = lobby.GetUserInfo(lobby.accountid);
  if (!user || user->retrieving)
    return;
  snap.progression_loaded = true;

  Stats &stats = user->statscopy;
  snap.experience = stats.CalculateExperience();

  auto &ag = user->agency[user->statsagency];
  std::vector<std::string> &ls = snap.summary_lines;
  ls.clear();
  AddSummaryLine(ls, "Kills:", stats.kills);
  AddSummaryLine(ls, "Deaths:", stats.deaths);
  AddSummaryLine(ls, "Suicides", stats.suicides);
  ls.push_back("");
  ls.push_back("Secrets");
  AddSummaryLine(ls, "  Returned:", stats.secretsreturned);
  AddSummaryLine(ls, "  Stolen:", stats.secretsstolen);
  AddSummaryLine(ls, "  Picked up:", stats.secretspickedup);
  AddSummaryLine(ls, "  Fumbled:", stats.secretsdropped);
  ls.push_back("");
  AddSummaryLine(ls, "Civilians killed:", stats.civilianskilled);
  AddSummaryLine(ls, "Guards killed:", stats.guardskilled);
  AddSummaryLine(ls, "Robots killed:", stats.robotskilled);
  AddSummaryLine(ls, "Defenses destroyed:", stats.defensekilled);
  AddSummaryLine(ls, "Fixed Cannons destroyed:", stats.fixedcannonsdestroyed);
  ls.push_back("");
  ls.push_back("Files");
  AddSummaryLine(ls, "  Hacked:", stats.fileshacked);
  AddSummaryLine(ls, "  Returned:", stats.filesreturned);
  ls.push_back("");
  AddSummaryLine(ls, "Powerups picked up:", stats.powerupspickedup);
  AddSummaryLine(ls, "Health packs used:", stats.healthpacksused);
  AddSummaryLine(ls, "Cameras placed:", stats.camerasplanted);
  AddSummaryLine(ls, "Detonators planted:", stats.detsplanted);
  AddSummaryLine(ls, "Fixed Cannons placed:", stats.fixedcannonsplaced);
  AddSummaryLine(ls, "Viruses used:", stats.virusesused);
  AddSummaryLine(ls, "Poisons:", stats.poisons);
  AddSummaryLine(ls, "Lazarus Tracts planted:", stats.tractsplanted);
  ls.push_back("");
  ls.push_back("Grenades thrown");
  AddSummaryLine(ls, "  E.M.P:", stats.empsthrown);
  AddSummaryLine(ls, "  Plasma:", stats.plasmasthrown);
  AddSummaryLine(ls, "  Shaped:", stats.shapedthrown);
  AddSummaryLine(ls, "  Flare:", stats.flaresthrown);
  AddSummaryLine(ls, "  Poison Flare:", stats.poisonflaresthrown);
  AddSummaryLine(ls, "  Neutron:", stats.neutronsthrown);
  static const char *const kWeaponNames[4] = {"Blaster", "Laser", "Rocket",
                                              "Flamer"};
  for (int i = 0; i < 4; i++) {
    ls.push_back("");
    ls.push_back(kWeaponNames[i]);
    AddSummaryLine(ls, "  Shots fired:", stats.weaponfires[i]);
    AddSummaryLine(ls, "  Hits:", stats.weaponhits[i]);
    const Uint32 accuracy =
        stats.weaponfires[i]
            ? (Uint32)((float(stats.weaponhits[i]) / stats.weaponfires[i]) * 100)
            : 0;
    AddSummaryLine(ls, "  Accuracy:", accuracy, true);
    AddSummaryLine(ls, "  Player kills:", stats.playerkillsweapon[i]);
  }

  snap.levels[0] = ag.endurance;
  snap.levels[1] = ag.shield;
  snap.levels[2] = ag.jetpack;
  snap.levels[3] = ag.techslots;
  snap.levels[4] = ag.hacking;
  snap.levels[5] = ag.contacts;
  int totalbonusupgrades = ag.endurance + ag.shield + ag.jetpack + ag.techslots +
                           ag.hacking + ag.contacts;
  int maxupgrades = ag.level;
  int possible = user->TotalUpgradePointsPossible(user->statsagency);
  if (maxupgrades > possible)
    maxupgrades = possible;
  snap.upgrade_banner = (totalbonusupgrades - ag.defaultbonuses < maxupgrades);
  if (snap.upgrade_banner) {
    snap.upgrades_available[0] = ag.endurance < ag.maxendurance;
    snap.upgrades_available[1] = ag.shield < ag.maxshield;
    snap.upgrades_available[2] = ag.jetpack < ag.maxjetpack;
    snap.upgrades_available[3] = ag.techslots < ag.maxtechslots;
    snap.upgrades_available[4] = ag.hacking < ag.maxhacking;
    snap.upgrades_available[5] = ag.contacts < ag.maxcontacts;
  }
}

// Cap a joined display string to the screen's text-scratch buffer (keeps the
// head — fine for name lists; the chat tail is capped separately).
void CapHead(std::string &s, size_t cap = 180) {
  if (s.size() > cap)
    s.erase(cap);
}

// Build the lobby read panels (selected agent + presence + games). Lobby mutex
// held by the caller. Chat is drained on the tick and joined separately.
void BuildLobbyPanels(client::ui::LobbySnapshot &snap, Lobby &lobby) {
  const Lobby::Character *ch = lobby.GetSelectedCharacter();
  if (ch) {
    char line[160];
    User *user = lobby.GetUserInfo(lobby.accountid);
    if (user && !user->retrieving && ch->agencyIdx < 5) {
      const auto &ag = user->agency[ch->agencyIdx];
      snprintf(line, sizeof(line), "%s\n%s  Level %u\n%u wins / %u losses",
               ch->name, kAgency[ch->agencyIdx], (unsigned)ag.level,
               (unsigned)ag.wins, (unsigned)ag.losses);
    } else {
      snprintf(line, sizeof(line), "%s", ch->name);
    }
    snap.lobby_agent = line;
  }
  for (auto &kv : lobby.presence) {
    if (!snap.lobby_presence.empty())
      snap.lobby_presence += "\n";
    snap.lobby_presence += kv.second.name;
    // origin chat_panel RebuildPresenceEntries: peers in a game append
    // " [<game name>]" (bank 133 renders the brackets as dashes).
    if (kv.second.gameid != 0) {
      LobbyGame *g = lobby.GetGameById(kv.second.gameid);
      if (g) {
        snap.lobby_presence += " [";
        snap.lobby_presence += g->name;
        snap.lobby_presence += "]";
      }
    }
  }
  CapHead(snap.lobby_presence);
  for (LobbyGame *g : lobby.games) {
    if (!g)
      continue;
    client::ui::GameBrowserEntry e;
    e.id = g->id;
    e.name = g->name;
    const bool ingame = (g->state == 1);
    e.password_protected = (g->password[0] != '\0');
    e.joinable = (!ingame && g->players < g->maxplayers) ||
                 (ingame && g->canrejoin);
    e.spectatable = ingame && g->spectatable;
    User *creator = lobby.GetUserInfo(g->accountid);
    const char *who =
        (creator && !creator->retrieving) ? creator->DisplayName() : "?";
    char detail[160];
    if (ingame) {
      snprintf(detail, sizeof(detail), "%s · %s Sec · by %s · in progress%s",
               g->mapname, SecurityLabel(g->securitylevel), who,
               e.password_protected ? " · locked" : "");
    } else {
      snprintf(detail, sizeof(detail), "%s · %s Sec · by %s · %u/%u players%s",
               g->mapname, SecurityLabel(g->securitylevel), who,
               (unsigned)g->players, (unsigned)g->maxplayers,
               e.password_protected ? " · locked" : "");
    }
    e.detail = detail;
    snap.games.push_back(std::move(e));
  }
}

// Build the pre-match staging roster from the connected world (doc §6). Called
// only while connected to a game; reads world teams/peers (single-thread game
// state) + the lobby user records (caller holds Lobby::LockMutex). Mirrors the
// legacy GameJoinPanel: one row per non-observer peer, the host-ready guard
// (is_host && !AllPeersDownloadedMap) pre-resolved into the Ready label.
void BuildStaging(client::ui::LobbySnapshot &snap, World &world, Lobby &lobby) {
  snap.staging_active = true;
  snap.staging_in_lobby = world.IsInLobby();
  const Uint8 localid = world.GetLocalPeerId();
  Peer *localpeer = world.GetPeer(localid);
  snap.staging_is_host = localpeer && localpeer->ishost;
  snap.staging_ready_blocked =
      snap.staging_is_host && !world.AllPeersDownloadedMap();
  snap.staging_ready_label =
      (snap.staging_in_lobby && snap.staging_ready_blocked) ? "Waiting..."
                                                            : "Ready";

  // Pre-match tech loadout (origin GameTechPanelTick): slots-left from the
  // user's agency techslots minus the chosen items; one row per GAS buyable
  // that participates in tech choice.
  if (localpeer) {
    Team *team = world.GetPeerTeam(localid);
    int slotsleft = 0;
    if (team) {
      if (User *user = lobby.GetUserInfo(localpeer->accountid)) {
        slotsleft =
            user->agency[team->agency].techslots - world.TechSlotsUsed(*localpeer);
        snap.staging_tech_slots_label =
            "Tech slots left: " + std::to_string(slotsleft);
      }
    }
    snap.staging_tech_choices = localpeer->techchoices;
    Team *techteam = world.GetPeerTeam(localid);
    for (BuyableItem *item : world.buyableitems) {
      if (!item || item->techchoice == 0)
        continue;
      // origin tech_tree_grid: agency-specific items only show for their
      // agency (and !techslots rows are skipped — techchoice==0 covers it).
      if (item->agencyspecific != -1 && techteam &&
          item->agencyspecific != techteam->agency)
        continue;
      client::ui::StagingTechRow row;
      row.name = item->name;
      row.description_title = "-" + row.name + "-";
      row.description = NormalizeTechDescription(item->description);
      row.slots = item->techslots;
      row.choice_mask = item->techchoice;
      row.selected = (localpeer->techchoices & item->techchoice) != 0;
      row.interactable = row.selected || item->techslots <= slotsleft;
      snap.staging_tech.push_back(row);
    }
  }

  const std::vector<Uint16> &teamIds = world.GetObjectsByType(ObjectTypes::TEAM);
  for (Uint16 teamId : teamIds) {
    Team *team = static_cast<Team *>(world.GetObjectFromId(teamId));
    if (!team || team->numpeers == 0)
      continue;
    bool drew_emblem = false;
    for (int i = 0; i < team->numpeers; ++i) {
      Peer *peer = world.GetPeer(team->peers[i]);
      if (!peer || peer->observer || peer->disconnected)
        continue;
      User *user = lobby.GetUserInfo(peer->accountid);
      if (!user || user->retrieving || !user->DisplayName()[0])
        continue;
      client::ui::StagingRosterRow row;
      row.is_local = (team->peers[i] == localid);
      row.ready = peer->isready;
      row.team_number = team->number;
      row.peer_slot = (uint8_t)i;
      row.agency = team->agency;
      row.draw_emblem = !drew_emblem;
      drew_emblem = true;
      // origin game_join_panel: plain display name; bots get " [BOT]".
      row.name = user->DisplayName();
      if (peer->isbot)
        row.name += " [BOT]";
      row.level = "L:" + std::to_string((unsigned)user->agency[team->agency].level);
      snap.staging_roster.push_back(std::move(row));
    }
  }
}

} // namespace

client::ui::LobbySnapshot CaptureLobbySnapshot(Game &game,
                                               client::ui::SessionPhase phase) {
  client::ui::LobbySnapshot snap;
  using P = client::ui::SessionPhase;
  const bool connectPhase = (phase == P::Connecting);
  const bool postMatchPhase = (phase == P::PostMatch);
  const bool charCreatePhase = (phase == P::CharacterCreate);
  const bool lobbyPhase = (phase == P::Lobby);
  if (!connectPhase && !postMatchPhase && !charCreatePhase && !lobbyPhase)
    return snap;

  World &world = game.GetWorld();
  Lobby &lobby = world.lobby;
  lobby.LockMutex();
  const int st = (int)lobby.state;
  snap.authenticated = (st == Lobby::AUTHENTICATED);
  if (connectPhase) {
    snap.awaiting_credentials = (st == Lobby::AUTHENTICATING);
    snap.credentials_pending = (st == Lobby::AUTHSENT);
    snap.connecting = !snap.awaiting_credentials && !snap.credentials_pending &&
                      st != Lobby::AUTHENTICATED && st != Lobby::IDLE;
  }
  if (charCreatePhase) {
    snap.characters_received = lobby.charactersreceived;
    snap.character_names.reserve(lobby.characters.size());
    snap.character_profiles.reserve(lobby.characters.size());
    for (const Lobby::Character &c : lobby.characters) {
      snap.character_names.push_back(c.name);
      client::ui::CharacterProfile profile;
      profile.name = c.name;
      profile.agency_index = c.agencyIdx;
      profile.rename_available = c.renameAvailable;
      profile.wins = c.stats.wins;
      profile.losses = c.stats.losses;
      profile.xp = c.stats.xp;
      profile.level = c.stats.level;
      profile.endurance = c.stats.endurance;
      profile.shield = c.stats.shield;
      profile.jetpack = c.stats.jetpack;
      profile.techslots = c.stats.techslots;
      profile.hacking = c.stats.hacking;
      profile.contacts = c.stats.contacts;
      snap.character_profiles.push_back(profile);
    }
  }
  if (postMatchPhase)
    BuildProgression(snap, lobby);
  if (lobbyPhase) {
    BuildLobbyPanels(snap, lobby);
    snap.chat_channel = lobby.channel;
    // SIL-234: a create/join is in flight (clicked, not yet connected). The
    // right column holds a stable "Connecting" panel through this window so it
    // never flashes the games browser before staging mounts. creategameclicked
    // covers create-from-form; joininggame covers the post-create/join connect.
    snap.joining = (game.creategameclicked || game.joininggame) &&
                   !world.IsConnected();
    if (world.IsConnected()) {
      BuildStaging(snap, world, lobby);
      if (LobbyGame *lg = lobby.GetGameById(game.currentlobbygameid)) {
        snap.staging_map_name = lg->mapname;
        // origin joins the game channel on connect ("#<name>-<id>", ambience_
        // mixer GetGameChannelName); the chat header shows it.
        snap.chat_channel =
            "#" + std::string(lg->name) + "-" + std::to_string(lg->id);
      }
    }
  }
  lobby.UnlockMutex();

  // The lobby chat scrollback lives on the game-owned drain buffer (single
  // thread), read outside the mutex; show the recent tail.
  if (lobbyPhase) {
    const std::vector<std::string> &chat = game.LobbyChatLog();
    const int count = (int)chat.size();
    const int start = std::max(0, count - kVisibleLogLines);
    for (int i = start; i < count; ++i) {
      if (!snap.lobby_chat.empty())
        snap.lobby_chat += "\n";
      snap.lobby_chat += chat[(size_t)i];
    }
    if (snap.lobby_chat.size() > kLobbyChatLogCap)
      snap.lobby_chat.erase(0, snap.lobby_chat.size() - kLobbyChatLogCap);
  }

  // The connect log lives on the game-owned flow (single-thread), not the lobby,
  // so it is read outside the mutex. Show the last lines, newline-joined.
  if (connectPhase) {
    const std::vector<std::string> &log = game.LobbyConnectLog();
    const int count = (int)log.size();
    const int start = std::max(0, count - kVisibleLogLines);
    for (int i = start; i < count; ++i) {
      if (!snap.status_log.empty())
        snap.status_log += "\n";
      snap.status_log += log[(size_t)i];
    }
    // Keep the recent tail within the screen's text-scratch cap.
    if (snap.status_log.size() > kStatusLogCap)
      snap.status_log.erase(0, snap.status_log.size() - kStatusLogCap);
  }
  return snap;
}

} // namespace silencer::game_ui
