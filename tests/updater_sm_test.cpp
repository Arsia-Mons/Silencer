#include "doctest.h"
#include "updater.h"
#include <cstdio>
#include <string>
#include <sys/stat.h>

// Helper: precreate a file with known content so we can test the VERIFYING step
// without a real download.
static std::string writeTempFile(const char *content) {
    char tmpl[] = "/tmp/zupd_sm_XXXXXX";
    int fd = mkstemp(tmpl);
    REQUIRE(fd >= 0);
    std::string path = tmpl;
    FILE *fp = fdopen(fd, "wb");
    fwrite(content, 1, 3, fp);  // write "abc"
    fclose(fp);
    return path;
}

TEST_CASE("VerifyOnly catches sha mismatch") {
    // sha256 of "abc" is ba7816bf...
    std::string path = writeTempFile("abc");
    uint8_t correct[32] = {
        0xba,0x78,0x16,0xbf, 0x8f,0x01,0xcf,0xea,
        0x41,0x41,0x40,0xde, 0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3, 0x96,0x17,0x7a,0x9c,
        0xb4,0x10,0xff,0x61, 0xf2,0x00,0x15,0xad
    };
    uint8_t wrong[32] = {0xff};

    CHECK(Updater::VerifyFile(path, correct));
    CHECK_FALSE(Updater::VerifyFile(path, wrong));
}

// PumpStage2 (driven every frame from Game::Loop) must stay a no-op until the
// worker actually reaches STAGING — otherwise it would spawn stage-2 mid-flight
// or in steady state. The STAGING→spawn path itself runs a real process and is
// covered by manual/e2e verification, not here.
TEST_CASE("PumpStage2 does nothing before STAGING") {
    Updater u;
    u.PumpStage2();  // fresh updater is IDLE
    CHECK(u.GetState() == Updater::IDLE);
    CHECK_FALSE(u.IsStage2Spawned());

    u.ForceState(Updater::DOWNLOADING);
    u.PumpStage2();
    CHECK(u.GetState() == Updater::DOWNLOADING);
    CHECK_FALSE(u.IsStage2Spawned());
}
