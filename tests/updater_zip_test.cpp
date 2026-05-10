#include "doctest.h"
#include "updaterzip.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// Path to mini.zip — set at configure time via a compile definition.
#ifndef ZIP_FIXTURE_DIR
#error "ZIP_FIXTURE_DIR not defined"
#endif

TEST_CASE("extract mini.zip into a fresh temp dir") {
    std::string zip = std::string(ZIP_FIXTURE_DIR) + "/mini.zip";
    char tmpl[] = "/tmp/zupd_test_XXXXXX";
    char *dir = mkdtemp(tmpl);
    REQUIRE(dir != nullptr);

    UpdaterZip::Result r = UpdaterZip::Extract(zip, dir);
    CHECK(r == UpdaterZip::OK);

    std::string f = std::string(dir) + "/hello.txt";
    FILE *fp = fopen(f.c_str(), "rb");
    REQUIRE(fp != nullptr);
    char buf[8] = {0};
    size_t n = fread(buf, 1, 8, fp);
    fclose(fp);
    CHECK(n == 3);
    CHECK(strcmp(buf, "hi\n") == 0);
}

TEST_CASE("missing zip returns error") {
    CHECK(UpdaterZip::Extract("/nonexistent.zip", "/tmp") != UpdaterZip::OK);
}
