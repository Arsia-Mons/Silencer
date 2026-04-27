#ifndef UPDATERSHA256_H
#define UPDATERSHA256_H

#include <cstddef>
#include <cstdint>

// Streaming SHA-256. Usage:
//   SHA256 h;
//   h.Update(buf, n); ...
//   uint8_t digest[32]; h.Final(digest);
class SHA256 {
public:
    SHA256();
    void Update(const void *data, size_t len);
    void Final(uint8_t out[32]);

private:
    void Transform(const uint8_t block[64]);
    uint32_t state[8];
    uint64_t bitcount;
    uint8_t  buffer[64];
    size_t   buffer_len;
};

#endif
