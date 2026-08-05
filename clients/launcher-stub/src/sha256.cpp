// Streaming SHA-256 (FIPS 180-4). Vendored so the stub depends on nothing;
// the e2e cross-checks the digest against the platform sha256 tool.

#include "stub.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

struct Sha256 {
  uint32_t h[8];
  uint64_t len = 0;
  uint8_t buf[64];
  size_t buflen = 0;

  Sha256() {
    static const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                     0xa54ff53a, 0x510e527f, 0x9b05688c,
                                     0x1f83d9ab, 0x5be0cd19};
    memcpy(h, init, sizeof h);
  }

  static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

  void block(const uint8_t *p) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
        0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
        0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
        0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
      w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 |
             (uint32_t)p[i * 4 + 2] << 8 | (uint32_t)p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
      uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
      uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      uint32_t ch = (e & f) ^ (~e & g);
      uint32_t t1 = hh + S1 + ch + K[i] + w[i];
      uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t t2 = S0 + mj;
      hh = g; g = f; f = e; e = d + t1;
      d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }

  void update(const uint8_t *p, size_t n) {
    len += n;
    while (n) {
      size_t take = 64 - buflen;
      if (take > n)
        take = n;
      memcpy(buf + buflen, p, take);
      buflen += take;
      p += take;
      n -= take;
      if (buflen == 64) {
        block(buf);
        buflen = 0;
      }
    }
  }

  void finish(uint8_t out[32]) {
    uint64_t bits = len * 8;
    uint8_t pad = 0x80, z = 0;
    update(&pad, 1);
    while (buflen != 56)
      update(&z, 1);
    uint8_t l[8];
    for (int i = 0; i < 8; i++)
      l[i] = (uint8_t)(bits >> (56 - 8 * i));
    update(l, 8);
    for (int i = 0; i < 8; i++)
      for (int j = 0; j < 4; j++)
        out[i * 4 + j] = (uint8_t)(h[i] >> (24 - 8 * j));
  }
};

} // namespace

std::string stub::sha256_file_hex(const std::string &path) {
  std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
  if (!in)
    return "";
  Sha256 s;
  std::vector<char> buf(64 * 1024);
  while (in) {
    in.read(buf.data(), (std::streamsize)buf.size());
    std::streamsize n = in.gcount();
    if (n > 0)
      s.update((const uint8_t *)buf.data(), (size_t)n);
  }
  uint8_t d[32];
  s.finish(d);
  char hex[65];
  for (int i = 0; i < 32; i++)
    snprintf(hex + i * 2, 3, "%02x", d[i]);
  return std::string(hex, 64);
}
