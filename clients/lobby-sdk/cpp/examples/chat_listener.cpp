// Minimal example: connects to a lobby, authenticates, and prints
// every chat message it sees. Exits on Ctrl-C or disconnect.
//
//   chat_listener <host> <port> <version> <username> <password>

#include "silencer/lobby/client.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace silencer::lobby;

static volatile std::sig_atomic_t g_stop = 0;
static void on_signal(int) { g_stop = 1; }

int main(int argc, char** argv) {
    if (argc != 6) {
        std::fprintf(stderr,
            "usage: %s <host> <port> <version> <username> <password>\n", argv[0]);
        return 1;
    }
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    ClientConfig cfg;
    cfg.host    = argv[1];
    cfg.port    = static_cast<uint16_t>(std::atoi(argv[2]));
    cfg.version = argv[3];

    Client c(cfg);

    c.on_state_changed([&](ConnectionState s) {
        std::fprintf(stderr, "[state] %d\n", static_cast<int>(s));
        if (s == ConnectionState::AwaitingVersion) c.send_version();
        if (s == ConnectionState::AwaitingAuth)    c.send_credentials(argv[4], argv[5]);
    });
    c.on_auth([&](const AuthResult& a) {
        if (a.ok) std::fprintf(stderr, "[auth] ok account_id=%u\n", a.account_id);
        else      std::fprintf(stderr, "[auth] FAIL: %s\n", a.error.c_str());
    });
    c.on_motd([](const std::string& m) {
        std::fprintf(stderr, "[motd]\n%s\n", m.c_str());
    });
    c.on_channel([](const std::string& ch) {
        std::fprintf(stderr, "[channel] %s\n", ch.c_str());
    });
    c.on_chat([](const ChatMessage& m) {
        std::printf("[%s] %s\n", m.channel.c_str(), m.text.c_str());
        std::fflush(stdout);
    });
    c.on_presence([](const PresenceUpdate& p) {
        std::fprintf(stderr, "[presence] %s %s acct=%u game=%u\n",
                     p.removed ? "leave" : "join",
                     p.name.c_str(), p.account_id, p.game_id);
    });
    c.on_error([](const std::string& msg) {
        std::fprintf(stderr, "[error] %s\n", msg.c_str());
    });

    c.connect();
    while (!g_stop && c.state() != ConnectionState::Disconnected
                  && c.state() != ConnectionState::Failed) {
        c.poll(std::chrono::milliseconds(200));
    }
    c.disconnect();
    return 0;
}
