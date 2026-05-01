// Standalone SHA-1 implementation (RFC 3174). Public domain reference:
// https://datatracker.ietf.org/doc/html/rfc3174
//
// Output matches the reference C client's sha1::calc() byte-for-byte;
// verified by the codec test suite against the lobby auth handshake.

#include "silencer/lobby/client.h"

#include <cstdint>
#include <cstring>

namespace silencer {
namespace lobby {

namespace {

inline uint32_t rotl(uint32_t v, unsigned n) { return (v << n) | (v >> (32 - n)); }

void process_block(const uint8_t block[64], uint32_t h[5]) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4    ]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) <<  8) |
               (static_cast<uint32_t>(block[i * 4 + 3])      );
    }
    for (int i = 16; i < 80; ++i) {
        w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20)      { f = (b & c) | (~b & d);          k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;                   k = 0xCA62C1D6; }
        uint32_t t = rotl(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rotl(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

} // namespace

std::array<uint8_t, 20> sha1(const void* data, size_t len) {
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t bit_len = static_cast<uint64_t>(len) * 8;

    while (len >= 64) {
        process_block(p, h);
        p   += 64;
        len -= 64;
    }

    uint8_t block[128] = {0};
    std::memcpy(block, p, len);
    block[len] = 0x80;
    size_t pad_to = (len < 56) ? 64 : 128;
    for (int i = 0; i < 8; ++i) {
        block[pad_to - 1 - i] = static_cast<uint8_t>((bit_len >> (i * 8)) & 0xFF);
    }
    process_block(block, h);
    if (pad_to == 128) process_block(block + 64, h);

    std::array<uint8_t, 20> out{};
    for (int i = 0; i < 5; ++i) {
        out[i * 4    ] = static_cast<uint8_t>((h[i] >> 24) & 0xFF);
        out[i * 4 + 1] = static_cast<uint8_t>((h[i] >> 16) & 0xFF);
        out[i * 4 + 2] = static_cast<uint8_t>((h[i] >>  8) & 0xFF);
        out[i * 4 + 3] = static_cast<uint8_t>( h[i]        & 0xFF);
    }
    return out;
}

} // namespace lobby
} // namespace silencer
