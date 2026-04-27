#ifndef SILENCER_LOBBY_CODEC_H
#define SILENCER_LOBBY_CODEC_H

#include "types.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace silencer {
namespace lobby {

// Thrown by decode helpers when a frame is shorter than expected or
// otherwise malformed. The Client catches these and reports a
// protocol error via the on_error callback rather than aborting.
class CodecError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Maximum payload (opcode + body) of a single frame. The leading
// length byte is excluded.
static constexpr size_t kMaxFramePayload = 255;

// Cursor over a frame payload (opcode byte + body). All reads are
// bounded; insufficient bytes throw CodecError.
class Reader {
public:
    Reader(const uint8_t* data, size_t size) : data_(data), size_(size), off_(0) {}

    uint8_t  u8();
    uint16_t u16_le();
    uint32_t u32_le();
    void     read_into(uint8_t* dst, size_t n);
    // Reads up to (and consumes) the next 0x00 byte.
    std::string cstr(size_t max_len);
    // Reads u8 length then exactly that many bytes.
    std::string lenstr();

    size_t remaining() const { return size_ - off_; }
    size_t offset() const { return off_; }

private:
    const uint8_t* data_;
    size_t         size_;
    size_t         off_;
};

class Writer {
public:
    void u8(uint8_t v);
    void u16_le(uint16_t v);
    void u32_le(uint32_t v);
    void write(const uint8_t* src, size_t n);
    void cstr(const std::string& s);
    void lenstr(const std::string& s);

    const std::vector<uint8_t>& bytes() const { return buf_; }
    std::vector<uint8_t>&&      take() { return std::move(buf_); }

private:
    std::vector<uint8_t> buf_;
};

// Frame envelope: prepends [len u8] to the payload. Throws if the
// payload is empty or larger than kMaxFramePayload.
std::vector<uint8_t> frame_encode(const std::vector<uint8_t>& payload);

// Decodes a frame from a stream-style byte source. Returns true and
// fills `out_payload` (opcode + body, without the leading length byte)
// when a complete frame is available; returns false otherwise. On
// success, `consumed` is set to the number of bytes consumed from the
// front of the buffer.
bool frame_try_decode(const uint8_t* buf, size_t avail,
                      std::vector<uint8_t>& out_payload, size_t& consumed);

// Per-opcode payload encoders. Each returns the full payload
// (opcode byte + body), suitable for passing to frame_encode().

std::vector<uint8_t> encode_version_request(const std::string& version, Platform p);
std::vector<uint8_t> encode_auth_request(const std::string& username,
                                         const std::array<uint8_t, 20>& password_sha1);
std::vector<uint8_t> encode_chat(const std::string& channel, const std::string& msg);
std::vector<uint8_t> encode_join_channel(const std::string& current_channel,
                                         const std::string& new_channel);
std::vector<uint8_t> encode_new_game(const LobbyGame& g);
std::vector<uint8_t> encode_user_info_request(uint32_t account_id);
std::vector<uint8_t> encode_ping_ack();
std::vector<uint8_t> encode_upgrade_stat(uint8_t agency_idx, uint8_t stat_id);
std::vector<uint8_t> encode_set_game(uint32_t game_id, GameStatus status);
std::vector<uint8_t> encode_register_stats(uint32_t game_id, uint8_t team_number,
                                           uint32_t account_id, uint8_t stats_agency,
                                           bool won, uint32_t xp,
                                           const MatchStats& stats);

// Body decoders. These accept a Reader positioned just after the
// opcode byte.

VersionResult  decode_version_reply(Reader& r);
AuthResult     decode_auth_reply(Reader& r);
ChatMessage    decode_chat_push(Reader& r);
NewGameEvent   decode_new_game(Reader& r);
uint32_t       decode_del_game(Reader& r);
std::string    decode_channel(Reader& r);
UserInfo       decode_user_info(Reader& r);
PresenceUpdate decode_presence(Reader& r);
// MOTD is special: the full payload is the chunk text, terminator
// is signalled by a payload that is exactly the opcode + a single
// 0x00 byte (i.e. an empty cstr).
struct MotdChunk { std::string text; bool terminator; };
MotdChunk      decode_motd(Reader& r, size_t payload_size);

// Game struct codecs (used by encode_new_game / decode_new_game).
void encode_lobby_game(Writer& w, const LobbyGame& g);
void decode_lobby_game(Reader& r, LobbyGame& g);

// Encodes the body of a UserInfo reply (without opcode byte). Exposed
// for round-trip testing of inbound shapes.
void encode_user_info_body(Writer& w, const UserInfo& u);
void encode_auth_reply(Writer& w, const AuthResult& a);
void encode_version_reply(Writer& w, const VersionResult& v);
void encode_chat_push(Writer& w, const ChatMessage& m);
void encode_motd_chunk(Writer& w, const MotdChunk& c);
void encode_presence(Writer& w, const PresenceUpdate& p);

} // namespace lobby
} // namespace silencer

#endif
