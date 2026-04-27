#ifndef SILENCER_LOBBY_CLIENT_H
#define SILENCER_LOBBY_CLIENT_H

#include "codec.h"
#include "types.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace silencer {
namespace lobby {

struct ClientConfig {
    std::string host           = "127.0.0.1";
    uint16_t    port           = 517;
    std::string version        = "";                       // empty = skip version check
    Platform    platform       = Platform::Unknown;
    // Inactivity threshold; the SDK closes the socket if no bytes arrive
    // for this long. 30 s is the server-side limit, so 20 s matches the
    // reference C++ client (clients/silencer/src/lobby.cpp).
    std::chrono::milliseconds read_timeout = std::chrono::seconds(20);
};

enum class ConnectionState {
    Disconnected,
    Connecting,
    AwaitingVersion,
    AwaitingAuth,
    Authenticated,
    Failed,
};

// Non-blocking, single-threaded TCP client. Drive it from your main
// loop by calling poll() repeatedly. The callbacks fire from inside
// poll() on the same thread.
//
// Lifecycle:
//   Client c(cfg);
//   c.connect();              // returns immediately
//   c.send_version();         // queue a version handshake
//   while (c.state() != Disconnected) {
//     c.poll(/*timeout=*/100ms);
//     ...
//   }
class Client {
public:
    explicit Client(ClientConfig cfg);
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    // ---- lifecycle --------------------------------------------------
    void connect();
    void disconnect();
    // Blocks up to `max_wait` waiting for socket activity, then drains
    // anything received and dispatches callbacks. Returns whether the
    // connection is still open.
    bool poll(std::chrono::milliseconds max_wait);

    ConnectionState state() const { return state_; }
    const std::string& last_error() const { return last_error_; }
    uint32_t account_id() const { return account_id_; }

    // ---- outbound ---------------------------------------------------
    void send_version();
    // password is hashed with SHA-1 internally; pass the raw password.
    void send_credentials(const std::string& username, const std::string& password);
    void send_chat(const std::string& channel, const std::string& message);
    void join_channel(const std::string& channel);
    void create_game(const LobbyGame& g);
    void request_user_info(uint32_t account_id);
    void upgrade_stat(uint8_t agency_idx, uint8_t stat_id);
    void set_game(uint32_t game_id, GameStatus status);
    void register_stats(uint32_t game_id, uint8_t team_number, uint32_t account_id,
                        uint8_t stats_agency, bool won, uint32_t xp,
                        const MatchStats& stats);

    // ---- callbacks --------------------------------------------------
    using StateChangedFn = std::function<void(ConnectionState)>;
    using VersionFn      = std::function<void(const VersionResult&)>;
    using AuthFn         = std::function<void(const AuthResult&)>;
    using MotdFn         = std::function<void(const std::string& full_motd)>;
    using ChatFn         = std::function<void(const ChatMessage&)>;
    using ChannelFn      = std::function<void(const std::string&)>;
    using NewGameFn      = std::function<void(const NewGameEvent&)>;
    using DelGameFn      = std::function<void(uint32_t game_id)>;
    using UserInfoFn     = std::function<void(const UserInfo&)>;
    using PresenceFn     = std::function<void(const PresenceUpdate&)>;
    using StatUpgradedFn = std::function<void()>;
    using ErrorFn        = std::function<void(const std::string&)>;

    void on_state_changed(StateChangedFn fn) { state_cb_     = std::move(fn); }
    void on_version(VersionFn fn)            { version_cb_   = std::move(fn); }
    void on_auth(AuthFn fn)                  { auth_cb_      = std::move(fn); }
    void on_motd(MotdFn fn)                  { motd_cb_      = std::move(fn); }
    void on_chat(ChatFn fn)                  { chat_cb_      = std::move(fn); }
    void on_channel(ChannelFn fn)            { channel_cb_   = std::move(fn); }
    void on_new_game(NewGameFn fn)           { new_game_cb_  = std::move(fn); }
    void on_del_game(DelGameFn fn)           { del_game_cb_  = std::move(fn); }
    void on_user_info(UserInfoFn fn)         { user_info_cb_ = std::move(fn); }
    void on_presence(PresenceFn fn)          { presence_cb_  = std::move(fn); }
    void on_stat_upgraded(StatUpgradedFn fn) { stat_cb_      = std::move(fn); }
    void on_error(ErrorFn fn)                { error_cb_     = std::move(fn); }

private:
    void send_raw(const std::vector<uint8_t>& payload);
    void dispatch_frame(const std::vector<uint8_t>& payload);
    void close_with_error(const std::string& msg);
    void set_state(ConnectionState s);

    ClientConfig    cfg_;
    int             sock_           = -1;
    ConnectionState state_          = ConnectionState::Disconnected;
    std::string     last_error_;
    uint32_t        account_id_     = 0;

    std::vector<uint8_t> rx_;        // accumulated inbound bytes
    std::string          motd_buf_;
    std::chrono::steady_clock::time_point last_rx_;

    StateChangedFn state_cb_;
    VersionFn      version_cb_;
    AuthFn         auth_cb_;
    MotdFn         motd_cb_;
    ChatFn         chat_cb_;
    ChannelFn      channel_cb_;
    NewGameFn      new_game_cb_;
    DelGameFn      del_game_cb_;
    UserInfoFn     user_info_cb_;
    PresenceFn     presence_cb_;
    StatUpgradedFn stat_cb_;
    ErrorFn        error_cb_;
};

// Computes raw SHA-1 of the input (20 bytes). Exposed for callers
// that want to pre-hash passwords.
std::array<uint8_t, 20> sha1(const void* data, size_t len);

} // namespace lobby
} // namespace silencer

#endif
