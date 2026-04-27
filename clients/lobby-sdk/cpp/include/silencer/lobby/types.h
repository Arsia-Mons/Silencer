#ifndef SILENCER_LOBBY_TYPES_H
#define SILENCER_LOBBY_TYPES_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace silencer {
namespace lobby {

// Opcodes — must match services/lobby/protocol.go.
enum Op : uint8_t {
    OpAuth          = 0,
    OpMOTD          = 1,
    OpChat          = 2,
    OpNewGame       = 3,
    OpDelGame       = 4,
    OpChannel       = 5,
    OpConnect       = 6,
    OpVersion       = 7,
    OpUserInfo      = 8,
    OpPing          = 9,
    OpUpgradeStat   = 10,
    OpRegisterStats = 11,
    OpPresence      = 12,
    OpSetGame       = 13,
};

enum class Platform : uint8_t {
    Unknown    = 0,
    MacOSARM64 = 1,
    WindowsX64 = 2,
};

enum class SecurityLevel : uint8_t {
    None   = 0,
    Low    = 1,
    Medium = 2,
    High   = 3,
};

// Presence status; also reused by SetGame.
enum class GameStatus : uint8_t {
    Lobby   = 0,
    Pregame = 1,
    Playing = 2,
};

struct AgencyStats {
    uint16_t wins             = 0;
    uint16_t losses           = 0;
    uint16_t xp_to_next_level = 0;
    uint8_t  level            = 0;
    uint8_t  endurance        = 0;
    uint8_t  shield           = 0;
    uint8_t  jetpack          = 0;
    uint8_t  tech_slots       = 0;
    uint8_t  hacking          = 0;
    uint8_t  contacts         = 0;
};

struct UserInfo {
    uint32_t                       account_id = 0;
    std::array<AgencyStats, 5>     agencies   = {};
    std::string                    name;
};

struct LobbyGame {
    uint32_t                  id              = 0;
    uint32_t                  account_id      = 0; // host
    std::string               name;
    std::string               password;
    std::string               hostname;          // "ip,port"
    std::string               map_name;
    std::array<uint8_t, 20>   map_hash        = {};
    uint8_t                   players         = 0;
    uint8_t                   state           = 0;
    SecurityLevel             security_level  = SecurityLevel::Medium;
    uint8_t                   min_level       = 0;
    uint8_t                   max_level       = 99;
    uint8_t                   max_players     = 24;
    uint8_t                   max_teams       = 6;
    uint8_t                   extra           = 0;
    uint16_t                  port            = 0;
};

struct WeaponStats {
    uint32_t fires        = 0;
    uint32_t hits         = 0;
    uint32_t player_kills = 0;
};

// Mirrors the wire layout of services/lobby/client.go:handleRegisterStats.
// 34 × u32 LE = 136 bytes.
struct MatchStats {
    std::array<WeaponStats, 4> weapons             = {};
    uint32_t civilians_killed                       = 0;
    uint32_t guards_killed                          = 0;
    uint32_t robots_killed                          = 0;
    uint32_t defense_killed                         = 0;
    uint32_t secrets_picked_up                      = 0;
    uint32_t secrets_returned                       = 0;
    uint32_t secrets_stolen                         = 0;
    uint32_t secrets_dropped                        = 0;
    uint32_t powerups_picked_up                     = 0;
    uint32_t deaths                                 = 0;
    uint32_t kills                                  = 0;
    uint32_t suicides                               = 0;
    uint32_t poisons                                = 0;
    uint32_t tracts_planted                         = 0;
    uint32_t grenades_thrown                        = 0;
    uint32_t neutrons_thrown                        = 0;
    uint32_t emps_thrown                            = 0;
    uint32_t shaped_thrown                          = 0;
    uint32_t plasmas_thrown                         = 0;
    uint32_t flares_thrown                          = 0;
    uint32_t poison_flares_thrown                   = 0;
    uint32_t health_packs_used                      = 0;
    uint32_t fixed_cannons_placed                   = 0;
    uint32_t fixed_cannons_destroyed                = 0;
    uint32_t dets_planted                           = 0;
    uint32_t cameras_planted                        = 0;
    uint32_t viruses_used                           = 0;
    uint32_t files_hacked                           = 0;
    uint32_t files_returned                         = 0;
    uint32_t credits_earned                         = 0;
    uint32_t credits_spent                          = 0;
    uint32_t heals_done                             = 0;
};

// Inbound events delivered by Client::poll() callbacks.

struct AuthResult {
    bool        ok         = false;
    uint32_t    account_id = 0;
    std::string error;
};

struct VersionResult {
    bool                       ok        = false;
    std::string                update_url;       // empty unless reject + update available
    std::array<uint8_t, 32>    sha256    = {};   // installer hash, valid iff !ok && !update_url.empty()
};

struct ChatMessage {
    std::string channel;
    std::string text;
    uint8_t     color      = 0;
    uint8_t     brightness = 128;
};

struct PresenceUpdate {
    bool        removed     = false;
    uint32_t    account_id  = 0;
    uint32_t    game_id     = 0;
    GameStatus  status      = GameStatus::Lobby;
    std::string name;
};

struct NewGameEvent {
    uint8_t   status = 0; // 1 = success/advertise, 2 = create failed
    LobbyGame game;
};

} // namespace lobby
} // namespace silencer

#endif
