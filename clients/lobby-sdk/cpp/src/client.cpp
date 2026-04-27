#include "silencer/lobby/client.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace silencer {
namespace lobby {

namespace {

void close_sock(int& s) {
    if (s != -1) {
        ::close(s);
        s = -1;
    }
}

// Resolves host (synchronously — fine for an outbound client; the
// reference C++ client uses a worker thread because it's tied to the
// game's main loop with no blocking allowance, but the SDK's user
// can wrap the call themselves if they need that).
bool resolve(const std::string& host, uint16_t port, sockaddr_in& out) {
    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    char portbuf[8];
    std::snprintf(portbuf, sizeof(portbuf), "%u", static_cast<unsigned>(port));
    if (::getaddrinfo(host.c_str(), portbuf, &hints, &res) != 0 || !res) {
        return false;
    }
    std::memcpy(&out, res->ai_addr, sizeof(sockaddr_in));
    ::freeaddrinfo(res);
    return true;
}

void set_nonblocking(int s) {
    int flags = ::fcntl(s, F_GETFL, 0);
    ::fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

} // namespace

Client::Client(ClientConfig cfg) : cfg_(std::move(cfg)) {}
Client::~Client() { disconnect(); }

void Client::set_state(ConnectionState s) {
    if (state_ == s) return;
    state_ = s;
    if (state_cb_) state_cb_(s);
}

void Client::close_with_error(const std::string& msg) {
    last_error_ = msg;
    if (error_cb_) error_cb_(msg);
    close_sock(sock_);
    set_state(ConnectionState::Disconnected);
}

void Client::connect() {
    disconnect();
    sockaddr_in addr{};
    if (!resolve(cfg_.host, cfg_.port, addr)) {
        close_with_error("dns: cannot resolve " + cfg_.host);
        set_state(ConnectionState::Failed);
        return;
    }
    int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        close_with_error(std::string("socket: ") + std::strerror(errno));
        set_state(ConnectionState::Failed);
        return;
    }
    int one = 1;
    ::setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    set_nonblocking(s);

    int rc = ::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (rc < 0 && errno != EINPROGRESS) {
        ::close(s);
        close_with_error(std::string("connect: ") + std::strerror(errno));
        set_state(ConnectionState::Failed);
        return;
    }
    sock_    = s;
    last_rx_ = std::chrono::steady_clock::now();
    set_state(ConnectionState::Connecting);
}

void Client::disconnect() {
    if (sock_ != -1) {
        ::shutdown(sock_, SHUT_RDWR);
        close_sock(sock_);
    }
    rx_.clear();
    motd_buf_.clear();
    account_id_ = 0;
    set_state(ConnectionState::Disconnected);
}

bool Client::poll(std::chrono::milliseconds max_wait) {
    if (sock_ == -1) return false;

    fd_set rfds, wfds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_SET(sock_, &rfds);
    if (state_ == ConnectionState::Connecting) FD_SET(sock_, &wfds);

    timeval tv;
    tv.tv_sec  = static_cast<time_t>(max_wait.count() / 1000);
    tv.tv_usec = static_cast<suseconds_t>((max_wait.count() % 1000) * 1000);
    int n = ::select(sock_ + 1, &rfds, &wfds, nullptr, &tv);
    if (n < 0) {
        if (errno == EINTR) return true;
        close_with_error(std::string("select: ") + std::strerror(errno));
        return false;
    }

    if (state_ == ConnectionState::Connecting && FD_ISSET(sock_, &wfds)) {
        int err = 0;
        socklen_t errlen = sizeof(err);
        ::getsockopt(sock_, SOL_SOCKET, SO_ERROR, &err, &errlen);
        if (err != 0) {
            close_with_error(std::string("connect: ") + std::strerror(err));
            set_state(ConnectionState::Failed);
            return false;
        }
        set_state(cfg_.version.empty() ? ConnectionState::AwaitingAuth
                                       : ConnectionState::AwaitingVersion);
    }

    if (FD_ISSET(sock_, &rfds)) {
        uint8_t tmp[2048];
        ssize_t r = ::recv(sock_, tmp, sizeof(tmp), 0);
        if (r == 0) {
            close_with_error("connection closed by peer");
            return false;
        }
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
            close_with_error(std::string("recv: ") + std::strerror(errno));
            return false;
        }
        rx_.insert(rx_.end(), tmp, tmp + r);
        last_rx_ = std::chrono::steady_clock::now();

        for (;;) {
            std::vector<uint8_t> payload;
            size_t consumed = 0;
            try {
                if (!frame_try_decode(rx_.data(), rx_.size(), payload, consumed)) break;
            } catch (const CodecError& e) {
                close_with_error(std::string("frame: ") + e.what());
                return false;
            }
            rx_.erase(rx_.begin(), rx_.begin() + static_cast<std::ptrdiff_t>(consumed));
            try {
                dispatch_frame(payload);
            } catch (const CodecError& e) {
                close_with_error(std::string("decode: ") + e.what());
                return false;
            }
        }
    }

    auto now = std::chrono::steady_clock::now();
    if (now - last_rx_ > cfg_.read_timeout) {
        close_with_error("read timeout");
        return false;
    }
    return sock_ != -1;
}

void Client::dispatch_frame(const std::vector<uint8_t>& payload) {
    if (payload.empty()) return;
    Reader r(payload.data(), payload.size());
    uint8_t op = r.u8();
    switch (op) {
        case OpVersion: {
            VersionResult v = decode_version_reply(r);
            if (v.ok) {
                set_state(ConnectionState::AwaitingAuth);
            } else {
                last_error_ = "version rejected";
                set_state(ConnectionState::Failed);
            }
            if (version_cb_) version_cb_(v);
            break;
        }
        case OpAuth: {
            AuthResult a = decode_auth_reply(r);
            if (a.ok) {
                account_id_ = a.account_id;
                set_state(ConnectionState::Authenticated);
            } else {
                last_error_ = a.error;
                set_state(ConnectionState::Failed);
            }
            if (auth_cb_) auth_cb_(a);
            break;
        }
        case OpMOTD: {
            MotdChunk c = decode_motd(r, payload.size());
            if (c.terminator) {
                if (motd_cb_) motd_cb_(motd_buf_);
                motd_buf_.clear();
            } else {
                motd_buf_ += c.text;
            }
            break;
        }
        case OpChat: {
            ChatMessage m = decode_chat_push(r);
            if (chat_cb_) chat_cb_(m);
            break;
        }
        case OpNewGame: {
            NewGameEvent ev = decode_new_game(r);
            if (new_game_cb_) new_game_cb_(ev);
            break;
        }
        case OpDelGame: {
            uint32_t id = decode_del_game(r);
            if (del_game_cb_) del_game_cb_(id);
            break;
        }
        case OpChannel: {
            std::string ch = decode_channel(r);
            if (channel_cb_) channel_cb_(ch);
            break;
        }
        case OpUserInfo: {
            UserInfo u = decode_user_info(r);
            if (user_info_cb_) user_info_cb_(u);
            break;
        }
        case OpPing:
            send_raw(encode_ping_ack());
            break;
        case OpPresence: {
            PresenceUpdate p = decode_presence(r);
            if (presence_cb_) presence_cb_(p);
            break;
        }
        case OpUpgradeStat:
            if (stat_cb_) stat_cb_();
            break;
        case OpConnect:
            // reserved, ignore
            break;
        default:
            // Unknown opcode: surface as a non-fatal error and skip.
            if (error_cb_) error_cb_("unknown opcode " + std::to_string(op));
            break;
    }
}

void Client::send_raw(const std::vector<uint8_t>& payload) {
    if (sock_ == -1) return;
    auto frame = frame_encode(payload);
    size_t off = 0;
    while (off < frame.size()) {
        ssize_t r = ::send(sock_, frame.data() + off, frame.size() - off, 0);
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(sock_, &wfds);
                timeval tv{1, 0};
                if (::select(sock_ + 1, nullptr, &wfds, nullptr, &tv) <= 0) {
                    close_with_error("send: timeout");
                    return;
                }
                continue;
            }
            close_with_error(std::string("send: ") + std::strerror(errno));
            return;
        }
        off += static_cast<size_t>(r);
    }
}

void Client::send_version() {
    send_raw(encode_version_request(cfg_.version, cfg_.platform));
}

void Client::send_credentials(const std::string& username, const std::string& password) {
    auto h = sha1(password.data(), password.size());
    send_raw(encode_auth_request(username, h));
}

void Client::send_chat(const std::string& channel, const std::string& message) {
    send_raw(encode_chat(channel, message));
}

void Client::join_channel(const std::string& channel) {
    // Server-side: any chat starting with "/join " is a switch.
    // The reference client sends its current channel as the "channel"
    // field — we match that to keep server-side logging consistent.
    send_raw(encode_join_channel("", channel));
}

void Client::create_game(const LobbyGame& g) {
    send_raw(encode_new_game(g));
}

void Client::request_user_info(uint32_t account_id) {
    send_raw(encode_user_info_request(account_id));
}

void Client::upgrade_stat(uint8_t agency_idx, uint8_t stat_id) {
    send_raw(encode_upgrade_stat(agency_idx, stat_id));
}

void Client::set_game(uint32_t game_id, GameStatus status) {
    send_raw(encode_set_game(game_id, status));
}

void Client::register_stats(uint32_t game_id, uint8_t team_number, uint32_t account_id,
                            uint8_t stats_agency, bool won, uint32_t xp,
                            const MatchStats& stats) {
    send_raw(encode_register_stats(game_id, team_number, account_id, stats_agency,
                                   won, xp, stats));
}

} // namespace lobby
} // namespace silencer
