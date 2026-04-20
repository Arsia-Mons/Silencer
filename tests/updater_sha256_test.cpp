#include "doctest.h"
#include "updatersha256.h"
#include <cstring>
#include <string>

static std::string hexify(const uint8_t digest[32]) {
    static const char hex[] = "0123456789abcdef";
    std::string s(64, '0');
    for (int i = 0; i < 32; i++) {
        s[i*2]     = hex[(digest[i] >> 4) & 0xF];
        s[i*2 + 1] = hex[digest[i]        & 0xF];
    }
    return s;
}

TEST_CASE("sha256 empty string") {
    // https://csrc.nist.gov/projects/cryptographic-standards-and-guidelines/example-values
    SHA256 h;
    h.Update(nullptr, 0);
    uint8_t out[32];
    h.Final(out);
    CHECK(hexify(out) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE("sha256 'abc'") {
    SHA256 h;
    h.Update("abc", 3);
    uint8_t out[32];
    h.Final(out);
    CHECK(hexify(out) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE("sha256 long stream (FIPS 180-4 vector B.3)") {
    // 1,000,000 repetitions of 'a'.
    SHA256 h;
    std::string chunk(1000, 'a');
    for (int i = 0; i < 1000; i++) h.Update(chunk.data(), chunk.size());
    uint8_t out[32];
    h.Final(out);
    CHECK(hexify(out) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}
