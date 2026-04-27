// Unit + golden-vector tests for the C++ lobby SDK codec.
//
// The golden vectors live in shared/lobby-protocol/vectors.json. This
// test extracts (name, hex) pairs from that file via a focused string
// scan (the file shape is stable; we deliberately don't pull in a JSON
// dependency for one test) and asserts:
//   1) decode(hex)        produces the expected struct
//   2) encode(struct)     reproduces the hex (unless the vector is in
//                         the skip-encode set)
// The expected struct values are hard-coded below so any drift between
// the wire format and either side of the codec triggers a test failure.

#include "silencer/lobby/client.h"
#include "silencer/lobby/codec.h"
#include "silencer/lobby/types.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace silencer::lobby;

// ---- helpers -----------------------------------------------------------

static std::vector<uint8_t> from_hex(const std::string& hex) {
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<uint8_t>((val(hex[i]) << 4) | val(hex[i + 1])));
    }
    return out;
}

static std::string to_hex(const std::vector<uint8_t>& b) {
    static const char* digits = "0123456789abcdef";
    std::string s;
    s.reserve(b.size() * 2);
    for (uint8_t v : b) {
        s.push_back(digits[v >> 4]);
        s.push_back(digits[v & 0xF]);
    }
    return s;
}

// Returns a name→hex map for every vector at the canonical "vectors[i]"
// depth in the JSON. Anchors to the 6-space indent that Python emits
// (`json.dumps(indent=2)`) so we don't accidentally match `"name":` or
// `"hex":` inside nested `value` objects.
static std::map<std::string, std::string> load_vectors(const char* path) {
    std::ifstream f(path);
    if (!f) {
        std::fprintf(stderr, "FAIL: cannot open %s\n", path);
        std::exit(2);
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string src = ss.str();

    static const std::string kNameAnchor = "\n      \"name\": \"";
    static const std::string kHexAnchor  = "\n      \"hex\": \"";

    std::map<std::string, std::string> out;
    size_t pos = 0;
    while (true) {
        size_t nk = src.find(kNameAnchor, pos);
        if (nk == std::string::npos) break;
        size_t name_open  = nk + kNameAnchor.size();
        size_t name_close = src.find('"', name_open);
        if (name_close == std::string::npos) break;
        std::string name = src.substr(name_open, name_close - name_open);

        size_t next_name = src.find(kNameAnchor, name_close);
        size_t hk        = src.find(kHexAnchor, name_close);
        pos = (next_name == std::string::npos) ? name_close + 1 : next_name;
        if (hk == std::string::npos || (next_name != std::string::npos && hk > next_name)) {
            continue; // vector has no hex (encode-only / decode-only marker)
        }
        size_t hex_open  = hk + kHexAnchor.size();
        size_t hex_close = src.find('"', hex_open);
        out[name] = src.substr(hex_open, hex_close - hex_open);
    }
    return out;
}

static int g_failures = 0;
static std::string g_current;

#define CHECK(cond) do {                                                       \
    if (!(cond)) {                                                             \
        std::fprintf(stderr, "FAIL [%s] %s:%d: %s\n",                          \
                     g_current.c_str(), __FILE__, __LINE__, #cond);            \
        ++g_failures;                                                          \
    }                                                                          \
} while (0)

#define CHECK_EQ(a, b) do {                                                    \
    auto _av = (a); auto _bv = (b);                                            \
    if (!(_av == _bv)) {                                                       \
        std::fprintf(stderr, "FAIL [%s] %s:%d: %s != %s\n",                    \
                     g_current.c_str(), __FILE__, __LINE__, #a, #b);           \
        ++g_failures;                                                          \
    }                                                                          \
} while (0)

// Decode the full frame (length prefix + payload), pop the leading byte,
// and return the payload (opcode + body) for the codec to inspect.
static std::vector<uint8_t> unframe(const std::vector<uint8_t>& wire) {
    std::vector<uint8_t> payload;
    size_t consumed = 0;
    bool ok = frame_try_decode(wire.data(), wire.size(), payload, consumed);
    CHECK(ok);
    CHECK_EQ(consumed, wire.size());
    return payload;
}

static std::vector<uint8_t> frame(const std::vector<uint8_t>& payload) {
    return frame_encode(payload);
}

// ---- per-vector tests --------------------------------------------------

// Vectors where we test decode but skip the encode equality (the wire
// shape sent by the reference C client is technically valid but the
// SDK encodes a canonical form that differs by a trailing byte).
static const std::set<std::string> kSkipEncode = {
    "chat_request",     // C client doesn't null-terminate the message field
    "setgame_request",  // C client appends a stray padding byte
};

static void test_auth_request(const std::string& hex) {
    auto wire = from_hex(hex);
    auto payload = unframe(wire);
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpAuth);
    auto user = r.cstr(17);
    CHECK_EQ(user, std::string("alice"));
    std::array<uint8_t, 20> hash{};
    r.read_into(hash.data(), 20);
    for (int i = 0; i < 20; ++i) CHECK_EQ(hash[i], i);
    if (!kSkipEncode.count("auth_request")) {
        std::array<uint8_t, 20> h{};
        for (int i = 0; i < 20; ++i) h[i] = static_cast<uint8_t>(i);
        auto enc = frame(encode_auth_request("alice", h));
        CHECK_EQ(to_hex(enc), hex);
    }
}

static void test_auth_reply_success(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpAuth);
    auto a = decode_auth_reply(r);
    CHECK(a.ok);
    CHECK_EQ(a.account_id, 0x80000000u);

    Writer w; w.u8(OpAuth); encode_auth_reply(w, a);
    CHECK_EQ(to_hex(frame(w.bytes())), hex);
}

static void test_auth_reply_failure(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpAuth);
    auto a = decode_auth_reply(r);
    CHECK(!a.ok);
    CHECK_EQ(a.error, std::string("Incorrect password for bob"));

    Writer w; w.u8(OpAuth); encode_auth_reply(w, a);
    CHECK_EQ(to_hex(frame(w.bytes())), hex);
}

static void test_version_request(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpVersion);
    auto ver = r.cstr(64);
    CHECK_EQ(ver, std::string("1.2.3"));
    CHECK_EQ(static_cast<int>(r.u8()), 1);

    auto enc = frame(encode_version_request("1.2.3", Platform::MacOSARM64));
    CHECK_EQ(to_hex(enc), hex);
}

static void test_version_reply_ok(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpVersion);
    auto v = decode_version_reply(r);
    CHECK(v.ok);
    CHECK(v.update_url.empty());

    Writer w; w.u8(OpVersion); encode_version_reply(w, v);
    CHECK_EQ(to_hex(frame(w.bytes())), hex);
}

static void test_version_reply_reject_with_update(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpVersion);
    auto v = decode_version_reply(r);
    CHECK(!v.ok);
    CHECK_EQ(v.update_url, std::string("https://example.com/silencer.dmg"));
    for (int i = 0; i < 32; ++i) CHECK_EQ(v.sha256[i], 0xAA);

    Writer w; w.u8(OpVersion); encode_version_reply(w, v);
    CHECK_EQ(to_hex(frame(w.bytes())), hex);
}

static void test_version_reply_reject_bare(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpVersion);
    auto v = decode_version_reply(r);
    CHECK(!v.ok);
    CHECK(v.update_url.empty());

    Writer w; w.u8(OpVersion); encode_version_reply(w, v);
    CHECK_EQ(to_hex(frame(w.bytes())), hex);
}

static void test_motd_chunk(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpMOTD);
    auto m = decode_motd(r, payload.size());
    CHECK(!m.terminator);
    CHECK_EQ(m.text, std::string("Hello"));

    Writer w; w.u8(OpMOTD); encode_motd_chunk(w, m);
    CHECK_EQ(to_hex(frame(w.bytes())), hex);
}

static void test_motd_terminator(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpMOTD);
    auto m = decode_motd(r, payload.size());
    CHECK(m.terminator);

    Writer w; w.u8(OpMOTD); encode_motd_chunk(w, m);
    CHECK_EQ(to_hex(frame(w.bytes())), hex);
}

static void test_chat_push(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpChat);
    auto m = decode_chat_push(r);
    CHECK_EQ(m.channel,    std::string("room1"));
    CHECK_EQ(m.text,       std::string("hi there!"));
    CHECK_EQ(m.color,      255);
    CHECK_EQ(m.brightness, 127);

    Writer w; w.u8(OpChat); encode_chat_push(w, m);
    CHECK_EQ(to_hex(frame(w.bytes())), hex);
}

static LobbyGame make_test_game() {
    LobbyGame g;
    g.id              = 100;
    g.account_id      = 10;
    g.name            = "Test";
    g.password        = "";
    g.hostname        = "123.456.789.0,5000";
    g.map_name        = "TestServ";
    for (int i = 0; i < 20; i += 4) {
        g.map_hash[i + 0] = 0xDE; g.map_hash[i + 1] = 0xAD;
        g.map_hash[i + 2] = 0xBE; g.map_hash[i + 3] = 0xEF;
    }
    g.players         = 2;
    g.state           = 0;
    g.security_level  = SecurityLevel::Medium;
    g.min_level       = 0;
    g.max_level       = 99;
    g.max_players     = 24;
    g.max_teams       = 6;
    g.extra           = 0;
    g.port            = 5000;
    return g;
}

static void test_newgame_push(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpNewGame);
    auto ev = decode_new_game(r);
    CHECK_EQ(ev.status,           1);
    CHECK_EQ(ev.game.id,          100u);
    CHECK_EQ(ev.game.account_id,  10u);
    CHECK_EQ(ev.game.name,        std::string("Test"));
    CHECK_EQ(ev.game.hostname,    std::string("123.456.789.0,5000"));
    CHECK_EQ(ev.game.map_name,    std::string("TestServ"));
    CHECK_EQ(ev.game.port,        5000);
    CHECK_EQ(ev.game.security_level, SecurityLevel::Medium);

    Writer w;
    w.u8(OpNewGame);
    w.u8(ev.status);
    encode_lobby_game(w, ev.game);
    CHECK_EQ(to_hex(frame(w.bytes())), hex);
}

static void test_delgame_push(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpDelGame);
    CHECK_EQ(decode_del_game(r), 100u);
}

static void test_channel_push(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpChannel);
    CHECK_EQ(decode_channel(r), std::string("Home"));
}

static void test_userinfo_request(const std::string& hex) {
    auto enc = frame(encode_user_info_request(200));
    CHECK_EQ(to_hex(enc), hex);
}

static void test_userinfo_reply(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpUserInfo);
    auto u = decode_user_info(r);
    CHECK_EQ(u.account_id, 200u);
    CHECK_EQ(u.name, std::string("admin"));
    CHECK_EQ(u.agencies[0].wins,  10);
    CHECK_EQ(u.agencies[1].level, 13);

    Writer w; w.u8(OpUserInfo); encode_user_info_body(w, u);
    CHECK_EQ(to_hex(frame(w.bytes())), hex);
}

static void test_ping(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    CHECK_EQ(payload.size(), 1u);
    CHECK_EQ(payload[0], OpPing);
}

static void test_ping_ack(const std::string& hex) {
    auto enc = frame(encode_ping_ack());
    CHECK_EQ(to_hex(enc), hex);
}

static void test_upgradestat_request(const std::string& hex) {
    auto enc = frame(encode_upgrade_stat(2, 3));
    CHECK_EQ(to_hex(enc), hex);
}

static void test_upgradestat_reply(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    CHECK_EQ(payload.size(), 1u);
    CHECK_EQ(payload[0], OpUpgradeStat);
}

static void test_presence_add(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpPresence);
    auto p = decode_presence(r);
    CHECK(!p.removed);
    CHECK_EQ(p.account_id, 42u);
    CHECK_EQ(p.game_id,    0u);
    CHECK_EQ(p.status,     GameStatus::Lobby);
    CHECK_EQ(p.name,       std::string("alice"));

    Writer w; w.u8(OpPresence); encode_presence(w, p);
    CHECK_EQ(to_hex(frame(w.bytes())), hex);
}

static void test_presence_remove(const std::string& hex) {
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpPresence);
    auto p = decode_presence(r);
    CHECK(p.removed);
    CHECK_EQ(p.account_id, 42u);
    CHECK_EQ(p.game_id,    100u);
    CHECK_EQ(p.status,     GameStatus::Playing);
    CHECK_EQ(p.name,       std::string("kim"));
}

static void test_chat_request(const std::string& hex) {
    // Decode-only: see kSkipEncode comment.
    auto payload = unframe(from_hex(hex));
    Reader r(payload.data(), payload.size());
    CHECK_EQ(r.u8(), OpChat);
    auto channel = r.cstr(64);
    CHECK_EQ(channel, std::string("Lobby"));
    // Remainder is the (un-terminated) message bytes.
    std::string msg(reinterpret_cast<const char*>(payload.data() + r.offset()),
                    payload.size() - r.offset());
    CHECK_EQ(msg, std::string("hi!"));
}

static void test_setgame_request(const std::string& hex) {
    // Decode-only: trailing padding byte. SDK encodes 6 bytes.
    auto wire = from_hex(hex);
    Reader r(wire.data() + 1, wire.size() - 1); // skip length byte
    CHECK_EQ(r.u8(), OpSetGame);
    CHECK_EQ(r.u32_le(), 100u);
    CHECK_EQ(r.u8(), 1);
}

// ---- driver ------------------------------------------------------------

static int test_sha1() {
    g_current = "sha1";
    // Known answer: SHA1("abc") = a9993e364706816aba3e25717850c26c9cd0d89d
    auto h = sha1("abc", 3);
    static const uint8_t expect[20] = {
        0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a, 0xba, 0x3e,
        0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c, 0x9c, 0xd0, 0xd8, 0x9d
    };
    for (int i = 0; i < 20; ++i) CHECK_EQ(h[i], expect[i]);

    // Empty input.
    auto h0 = sha1("", 0);
    static const uint8_t expect0[20] = {
        0xda, 0x39, 0xa3, 0xee, 0x5e, 0x6b, 0x4b, 0x0d, 0x32, 0x55,
        0xbf, 0xef, 0x95, 0x60, 0x18, 0x90, 0xaf, 0xd8, 0x07, 0x09
    };
    for (int i = 0; i < 20; ++i) CHECK_EQ(h0[i], expect0[i]);

    // Long input that crosses a block boundary.
    std::string long_input(1000, 'a');
    auto h1 = sha1(long_input.data(), long_input.size());
    static const uint8_t expect1[20] = {
        0x29, 0x1e, 0x9a, 0x6c, 0x66, 0x99, 0x49, 0x49, 0xb5, 0x7b,
        0xa5, 0xe6, 0x50, 0x36, 0x1e, 0x98, 0xfc, 0x36, 0xb1, 0xba
    };
    for (int i = 0; i < 20; ++i) CHECK_EQ(h1[i], expect1[i]);
    return 0;
}

static void test_register_stats_roundtrip() {
    g_current = "register_stats_roundtrip";
    MatchStats s;
    s.weapons[0].fires        = 100;
    s.weapons[0].player_kills = 5;
    s.weapons[3].hits         = 7;
    s.kills                   = 3;
    s.heals_done              = 42;
    s.credits_earned          = 0xDEADBEEFu;

    auto enc = frame(encode_register_stats(/*game_id*/ 7, /*team*/ 1, /*acct*/ 9,
                                           /*ag*/ 2, /*won*/ true, /*xp*/ 1234, s));
    // 44 × u32 stats (12 weapon fields + 32 scalars) = 176 bytes.
    // Payload = opcode + game_id u32 + team u8 + acct u32 + agency u8
    //         + won u8 + xp u32 + 176 = 192. Wire = 1 (length byte) + 192 = 193.
    static constexpr size_t kStatsBytes   = 4u * (4 * 3 + 32);
    static constexpr size_t kPayloadBytes = 1 + 4 + 1 + 4 + 1 + 1 + 4 + kStatsBytes;
    CHECK_EQ(enc.size(), 1 + kPayloadBytes);
    CHECK_EQ(enc[0], static_cast<uint8_t>(kPayloadBytes));
    CHECK_EQ(enc[1], OpRegisterStats);
}

int main() {
    auto vectors = load_vectors(SILENCER_VECTORS_PATH);

    auto run = [&](const char* name, void (*fn)(const std::string&)) {
        auto it = vectors.find(name);
        if (it == vectors.end()) {
            std::fprintf(stderr, "FAIL: missing vector %s in %s\n", name, SILENCER_VECTORS_PATH);
            ++g_failures;
            return;
        }
        g_current = name;
        fn(it->second);
    };

    run("auth_request",                      test_auth_request);
    run("auth_reply_success",                test_auth_reply_success);
    run("auth_reply_failure",                test_auth_reply_failure);
    run("version_request",                   test_version_request);
    run("version_reply_ok",                  test_version_reply_ok);
    run("version_reply_reject_with_update",  test_version_reply_reject_with_update);
    run("version_reply_reject_bare",         test_version_reply_reject_bare);
    run("motd_chunk",                        test_motd_chunk);
    run("motd_terminator",                   test_motd_terminator);
    run("chat_request",                      test_chat_request);
    run("chat_push",                         test_chat_push);
    run("newgame_push",                      test_newgame_push);
    run("delgame_push",                      test_delgame_push);
    run("channel_push",                      test_channel_push);
    run("userinfo_request",                  test_userinfo_request);
    run("userinfo_reply",                    test_userinfo_reply);
    run("ping_push",                         test_ping);
    run("ping_ack",                          test_ping_ack);
    run("upgradestat_request",               test_upgradestat_request);
    run("upgradestat_reply",                 test_upgradestat_reply);
    run("presence_add",                      test_presence_add);
    run("presence_remove",                   test_presence_remove);
    run("setgame_request",                   test_setgame_request);

    test_sha1();
    test_register_stats_roundtrip();

    if (g_failures > 0) {
        std::fprintf(stderr, "\n%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("OK: all codec tests passed\n");
    return 0;
}
