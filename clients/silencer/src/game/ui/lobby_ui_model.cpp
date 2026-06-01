#include "ui/lobby_ui_model.h"

#include "game.h"
#include "lobby.h"
#include "lobbygame.h"
#include "world.h"
#include "objecttypes.h"
#include "actor/team.h"
#include "actor/user.h"
#include "peer.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace silencer::game_ui {
namespace {

constexpr int kVisibleLogLines = 15;
// Keep the joined connect log within the per-call-site text scratch the screen
// renders it through (REACT_TEXT_STORAGE_CAP), preferring the recent tail.
constexpr size_t kStatusLogCap = 180;

// Agency display names, indexed by Team::{NOXIS..BLACKROSE} / Character::agencyIdx.
const char *const kAgency[5] = {"Noxis", "Lazarus", "Caliber", "Static",
                                "Black Rose"};

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

// Build the post-match progression fields from the local user's stats copy.
// Lobby mutex is held by the caller. Leaves `progression_loaded` false until the
// user record arrives. The detailed per-weapon breakdown the legacy screen
// scrolled is deferred to SIL-21 (which adds the scroll container); SIL-20 shows
// experience + the upgradeable levels + a compact result line.
void BuildProgression(client::ui::LobbySnapshot &snap, Lobby &lobby) {
  User *user = lobby.GetUserInfo(lobby.accountid);
  if (!user || user->retrieving)
    return;
  snap.progression_loaded = true;

  Stats &stats = user->statscopy;
  snap.experience = stats.CalculateExperience();

  auto &ag = user->agency[user->statsagency];
  char summary[160];
  snprintf(summary, sizeof(summary),
           "Level %u\n%u kills / %u deaths\n%u wins / %u losses",
           (unsigned)ag.level, (unsigned)stats.kills, (unsigned)stats.deaths,
           (unsigned)ag.wins, (unsigned)ag.losses);
  snap.stats_text = summary;

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

  const std::vector<Uint16> &teamIds = world.GetObjectsByType(ObjectTypes::TEAM);
  for (Uint16 teamId : teamIds) {
    Team *team = static_cast<Team *>(world.GetObjectFromId(teamId));
    if (!team || team->numpeers == 0)
      continue;
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
      row.name = user->DisplayName();
      if (row.is_local)
        row.name += " (you)";
      const char *agency = (team->agency < 5) ? kAgency[team->agency] : "?";
      char detail[96];
      snprintf(detail, sizeof(detail), "%s · L:%u", agency,
               (unsigned)user->agency[team->agency].level);
      row.detail = detail;
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
    for (const Lobby::Character &c : lobby.characters)
      snap.character_names.push_back(c.name);
  }
  if (postMatchPhase)
    BuildProgression(snap, lobby);
  if (lobbyPhase) {
    BuildLobbyPanels(snap, lobby);
    if (world.IsConnected())
      BuildStaging(snap, world, lobby);
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
    if (snap.lobby_chat.size() > kStatusLogCap)
      snap.lobby_chat.erase(0, snap.lobby_chat.size() - kStatusLogCap);
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
