#include "doctest.h"
#include "updaterdownload.h"

TEST_CASE("https is always accepted") {
    CHECK(UpdaterDownload::IsAllowed("https://example.com/a.zip"));
}

TEST_CASE("http is rejected for non-loopback") {
    CHECK_FALSE(UpdaterDownload::IsAllowed("http://example.com/a.zip"));
    CHECK_FALSE(UpdaterDownload::IsAllowed("http://8.8.8.8/a.zip"));
}

TEST_CASE("http on loopback is accepted") {
    CHECK(UpdaterDownload::IsAllowed("http://127.0.0.1/a.zip"));
    CHECK(UpdaterDownload::IsAllowed("http://127.0.0.1:8000/a.zip"));
    CHECK(UpdaterDownload::IsAllowed("http://[::1]/a.zip"));
    CHECK(UpdaterDownload::IsAllowed("http://localhost:8000/a.zip"));
}

TEST_CASE("garbage schemes rejected") {
    CHECK_FALSE(UpdaterDownload::IsAllowed("file:///etc/passwd"));
    CHECK_FALSE(UpdaterDownload::IsAllowed("ftp://example.com/a.zip"));
    CHECK_FALSE(UpdaterDownload::IsAllowed(""));
}
