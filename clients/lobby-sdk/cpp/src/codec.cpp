#include "silencer/lobby/codec.h"

#include <cstring>

namespace silencer {
namespace lobby {

// ---- Reader ------------------------------------------------------------

uint8_t Reader::u8() {
    if (off_ >= size_) throw CodecError("u8: short read");
    return data_[off_++];
}

uint16_t Reader::u16_le() {
    if (off_ + 2 > size_) throw CodecError("u16: short read");
    uint16_t v = static_cast<uint16_t>(data_[off_]) |
                 (static_cast<uint16_t>(data_[off_ + 1]) << 8);
    off_ += 2;
    return v;
}

uint32_t Reader::u32_le() {
    if (off_ + 4 > size_) throw CodecError("u32: short read");
    uint32_t v = static_cast<uint32_t>(data_[off_])        |
                 (static_cast<uint32_t>(data_[off_ + 1]) <<  8) |
                 (static_cast<uint32_t>(data_[off_ + 2]) << 16) |
                 (static_cast<uint32_t>(data_[off_ + 3]) << 24);
    off_ += 4;
    return v;
}

void Reader::read_into(uint8_t* dst, size_t n) {
    if (off_ + n > size_) throw CodecError("bytes: short read");
    std::memcpy(dst, data_ + off_, n);
    off_ += n;
}

std::string Reader::cstr(size_t max_len) {
    size_t end = off_;
    size_t limit = off_ + max_len;
    if (limit > size_) limit = size_;
    while (end < limit && data_[end] != 0) ++end;
    if (end >= limit) throw CodecError("cstr: unterminated");
    std::string s(reinterpret_cast<const char*>(data_ + off_), end - off_);
    off_ = end + 1;
    return s;
}

std::string Reader::lenstr() {
    uint8_t n = u8();
    if (off_ + n > size_) throw CodecError("lenstr: short read");
    std::string s(reinterpret_cast<const char*>(data_ + off_), n);
    off_ += n;
    return s;
}

// ---- Writer ------------------------------------------------------------

void Writer::u8(uint8_t v) { buf_.push_back(v); }
void Writer::u16_le(uint16_t v) {
    buf_.push_back(static_cast<uint8_t>(v & 0xFF));
    buf_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
void Writer::u32_le(uint32_t v) {
    buf_.push_back(static_cast<uint8_t>( v        & 0xFF));
    buf_.push_back(static_cast<uint8_t>((v >>  8) & 0xFF));
    buf_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}
void Writer::write(const uint8_t* src, size_t n) {
    buf_.insert(buf_.end(), src, src + n);
}
void Writer::cstr(const std::string& s) {
    buf_.insert(buf_.end(), s.begin(), s.end());
    buf_.push_back(0);
}
void Writer::lenstr(const std::string& s) {
    size_t n = s.size();
    if (n > 255) n = 255;
    buf_.push_back(static_cast<uint8_t>(n));
    buf_.insert(buf_.end(), s.begin(), s.begin() + n);
}

// ---- Framing -----------------------------------------------------------

std::vector<uint8_t> frame_encode(const std::vector<uint8_t>& payload) {
    if (payload.empty() || payload.size() > kMaxFramePayload) {
        throw CodecError("frame_encode: bad payload size");
    }
    std::vector<uint8_t> out;
    out.reserve(1 + payload.size());
    out.push_back(static_cast<uint8_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

bool frame_try_decode(const uint8_t* buf, size_t avail,
                      std::vector<uint8_t>& out_payload, size_t& consumed) {
    if (avail < 1) return false;
    uint8_t n = buf[0];
    if (n == 0) throw CodecError("frame_try_decode: zero-length frame");
    if (avail < static_cast<size_t>(1 + n)) return false;
    out_payload.assign(buf + 1, buf + 1 + n);
    consumed = 1 + n;
    return true;
}

// ---- LobbyGame ---------------------------------------------------------

void encode_lobby_game(Writer& w, const LobbyGame& g) {
    w.u32_le(g.id);
    w.u32_le(g.account_id);
    w.lenstr(g.name);
    w.lenstr(g.password);
    w.lenstr(g.hostname);
    w.lenstr(g.map_name);
    w.write(g.map_hash.data(), g.map_hash.size());
    w.u8(g.players);
    w.u8(g.state);
    w.u8(static_cast<uint8_t>(g.security_level));
    w.u8(g.min_level);
    w.u8(g.max_level);
    w.u8(g.max_players);
    w.u8(g.max_teams);
    w.u8(g.extra);
    w.u16_le(g.port);
}

void decode_lobby_game(Reader& r, LobbyGame& g) {
    g.id         = r.u32_le();
    g.account_id = r.u32_le();
    g.name       = r.lenstr();
    g.password   = r.lenstr();
    g.hostname   = r.lenstr();
    g.map_name   = r.lenstr();
    r.read_into(g.map_hash.data(), g.map_hash.size());
    g.players        = r.u8();
    g.state          = r.u8();
    g.security_level = static_cast<SecurityLevel>(r.u8());
    g.min_level      = r.u8();
    g.max_level      = r.u8();
    g.max_players    = r.u8();
    g.max_teams      = r.u8();
    g.extra          = r.u8();
    g.port           = r.u16_le();
}

// ---- Per-opcode encoders ------------------------------------------------

std::vector<uint8_t> encode_version_request(const std::string& version, Platform p) {
    Writer w;
    w.u8(OpVersion);
    w.cstr(version);
    w.u8(static_cast<uint8_t>(p));
    return std::move(w).take();
}

std::vector<uint8_t> encode_auth_request(const std::string& username,
                                         const std::array<uint8_t, 20>& password_sha1) {
    Writer w;
    w.u8(OpAuth);
    w.cstr(username);
    w.write(password_sha1.data(), password_sha1.size());
    return std::move(w).take();
}

std::vector<uint8_t> encode_chat(const std::string& channel, const std::string& msg) {
    Writer w;
    w.u8(OpChat);
    w.cstr(channel);
    w.cstr(msg);
    return std::move(w).take();
}

std::vector<uint8_t> encode_join_channel(const std::string& current_channel,
                                         const std::string& new_channel) {
    return encode_chat(current_channel, "/join " + new_channel);
}

std::vector<uint8_t> encode_new_game(const LobbyGame& g) {
    Writer w;
    w.u8(OpNewGame);
    encode_lobby_game(w, g);
    return std::move(w).take();
}

std::vector<uint8_t> encode_user_info_request(uint32_t account_id) {
    Writer w;
    w.u8(OpUserInfo);
    w.u32_le(account_id);
    return std::move(w).take();
}

std::vector<uint8_t> encode_ping_ack() {
    Writer w;
    w.u8(OpPing);
    w.u8(1);
    return std::move(w).take();
}

std::vector<uint8_t> encode_upgrade_stat(uint8_t agency_idx, uint8_t stat_id) {
    Writer w;
    w.u8(OpUpgradeStat);
    w.u8(agency_idx);
    w.u8(stat_id);
    return std::move(w).take();
}

std::vector<uint8_t> encode_set_game(uint32_t game_id, GameStatus status) {
    Writer w;
    w.u8(OpSetGame);
    w.u32_le(game_id);
    w.u8(static_cast<uint8_t>(status));
    return std::move(w).take();
}

std::vector<uint8_t> encode_register_stats(uint32_t game_id, uint8_t team_number,
                                           uint32_t account_id, uint8_t stats_agency,
                                           bool won, uint32_t xp,
                                           const MatchStats& s) {
    Writer w;
    w.u8(OpRegisterStats);
    w.u32_le(game_id);
    w.u8(team_number);
    w.u32_le(account_id);
    w.u8(stats_agency);
    w.u8(won ? 1 : 0);
    w.u32_le(xp);
    for (int i = 0; i < 4; ++i) {
        w.u32_le(s.weapons[i].fires);
        w.u32_le(s.weapons[i].hits);
        w.u32_le(s.weapons[i].player_kills);
    }
    w.u32_le(s.civilians_killed);
    w.u32_le(s.guards_killed);
    w.u32_le(s.robots_killed);
    w.u32_le(s.defense_killed);
    w.u32_le(s.secrets_picked_up);
    w.u32_le(s.secrets_returned);
    w.u32_le(s.secrets_stolen);
    w.u32_le(s.secrets_dropped);
    w.u32_le(s.powerups_picked_up);
    w.u32_le(s.deaths);
    w.u32_le(s.kills);
    w.u32_le(s.suicides);
    w.u32_le(s.poisons);
    w.u32_le(s.tracts_planted);
    w.u32_le(s.grenades_thrown);
    w.u32_le(s.neutrons_thrown);
    w.u32_le(s.emps_thrown);
    w.u32_le(s.shaped_thrown);
    w.u32_le(s.plasmas_thrown);
    w.u32_le(s.flares_thrown);
    w.u32_le(s.poison_flares_thrown);
    w.u32_le(s.health_packs_used);
    w.u32_le(s.fixed_cannons_placed);
    w.u32_le(s.fixed_cannons_destroyed);
    w.u32_le(s.dets_planted);
    w.u32_le(s.cameras_planted);
    w.u32_le(s.viruses_used);
    w.u32_le(s.files_hacked);
    w.u32_le(s.files_returned);
    w.u32_le(s.credits_earned);
    w.u32_le(s.credits_spent);
    w.u32_le(s.heals_done);
    return std::move(w).take();
}

// ---- Body decoders ------------------------------------------------------

VersionResult decode_version_reply(Reader& r) {
    VersionResult v;
    v.ok = r.u8() != 0;
    if (!v.ok && r.remaining() >= 2 + 32) {
        uint16_t url_len = r.u16_le();
        if (url_len > 0 && url_len < 512 && r.remaining() >= static_cast<size_t>(url_len) + 32) {
            std::string url(url_len, '\0');
            r.read_into(reinterpret_cast<uint8_t*>(&url[0]), url_len);
            v.update_url = std::move(url);
            r.read_into(v.sha256.data(), v.sha256.size());
        }
    }
    return v;
}

AuthResult decode_auth_reply(Reader& r) {
    AuthResult a;
    a.ok = r.u8() != 0;
    if (a.ok) {
        a.account_id = r.u32_le();
    } else {
        a.error = r.cstr(256);
    }
    return a;
}

ChatMessage decode_chat_push(Reader& r) {
    ChatMessage m;
    m.channel    = r.cstr(64);
    m.text       = r.cstr(kMaxFramePayload);
    m.color      = r.u8();
    m.brightness = r.u8();
    return m;
}

NewGameEvent decode_new_game(Reader& r) {
    NewGameEvent ev;
    ev.status = r.u8();
    decode_lobby_game(r, ev.game);
    return ev;
}

uint32_t decode_del_game(Reader& r) { return r.u32_le(); }

std::string decode_channel(Reader& r) { return r.cstr(64); }

UserInfo decode_user_info(Reader& r) {
    UserInfo u;
    u.account_id = r.u32_le();
    for (int i = 0; i < 5; ++i) {
        AgencyStats& a = u.agencies[i];
        a.wins             = r.u16_le();
        a.losses           = r.u16_le();
        a.xp_to_next_level = r.u16_le();
        a.level            = r.u8();
        a.endurance        = r.u8();
        a.shield           = r.u8();
        a.jetpack          = r.u8();
        a.tech_slots       = r.u8();
        a.hacking          = r.u8();
        a.contacts         = r.u8();
    }
    u.name = r.lenstr();
    return u;
}

PresenceUpdate decode_presence(Reader& r) {
    PresenceUpdate p;
    uint8_t action = r.u8();
    p.removed     = (action == 1);
    p.account_id  = r.u32_le();
    p.game_id     = r.u32_le();
    p.status      = static_cast<GameStatus>(r.u8());
    p.name        = r.lenstr();
    return p;
}

MotdChunk decode_motd(Reader& r, size_t payload_size) {
    // payload_size includes the opcode byte; if the body is just a
    // single 0x00 byte, that's the terminator.
    MotdChunk c;
    if (payload_size <= 2 && r.remaining() >= 1) {
        uint8_t b = r.u8();
        c.terminator = (b == 0);
        if (!c.terminator) c.text.push_back(static_cast<char>(b));
        return c;
    }
    c.terminator = false;
    c.text       = r.cstr(kMaxFramePayload);
    return c;
}

// ---- Symmetric encoders for inbound shapes (used in tests) -------------

void encode_user_info_body(Writer& w, const UserInfo& u) {
    w.u32_le(u.account_id);
    for (int i = 0; i < 5; ++i) {
        const AgencyStats& a = u.agencies[i];
        w.u16_le(a.wins);
        w.u16_le(a.losses);
        w.u16_le(a.xp_to_next_level);
        w.u8(a.level);
        w.u8(a.endurance);
        w.u8(a.shield);
        w.u8(a.jetpack);
        w.u8(a.tech_slots);
        w.u8(a.hacking);
        w.u8(a.contacts);
    }
    w.lenstr(u.name);
}

void encode_auth_reply(Writer& w, const AuthResult& a) {
    w.u8(a.ok ? 1 : 0);
    if (a.ok) {
        w.u32_le(a.account_id);
    } else {
        w.cstr(a.error);
    }
}

void encode_version_reply(Writer& w, const VersionResult& v) {
    w.u8(v.ok ? 1 : 0);
    if (!v.ok && !v.update_url.empty()) {
        w.u16_le(static_cast<uint16_t>(v.update_url.size()));
        w.write(reinterpret_cast<const uint8_t*>(v.update_url.data()),
                v.update_url.size());
        w.write(v.sha256.data(), v.sha256.size());
    }
}

void encode_chat_push(Writer& w, const ChatMessage& m) {
    w.cstr(m.channel);
    w.cstr(m.text);
    w.u8(m.color);
    w.u8(m.brightness);
}

void encode_motd_chunk(Writer& w, const MotdChunk& c) {
    if (c.terminator) {
        w.u8(0);
    } else {
        w.cstr(c.text);
    }
}

void encode_presence(Writer& w, const PresenceUpdate& p) {
    w.u8(p.removed ? 1 : 0);
    w.u32_le(p.account_id);
    w.u32_le(p.game_id);
    w.u8(static_cast<uint8_t>(p.status));
    w.lenstr(p.name);
}

} // namespace lobby
} // namespace silencer
