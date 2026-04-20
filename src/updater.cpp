#include "updater.h"
#include "updaterdownload.h"
#include "updatersha256.h"
#include "updaterzip.h"

#include <cstdio>
#include <cstring>

bool Updater::VerifyFile(const std::string &path, const uint8_t expected[32]) {
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    SHA256 h;
    uint8_t buf[8192];
    for (;;) {
        size_t n = fread(buf, 1, sizeof(buf), fp);
        if (n == 0) break;
        h.Update(buf, n);
    }
    fclose(fp);
    uint8_t out[32];
    h.Final(out);
    return memcmp(out, expected, 32) == 0;
}

Updater::Updater()
    : state(IDLE), bytes_got(0), bytes_total(0), cancel_flag(false), retries(0) {}

Updater::~Updater() {
    cancel_flag = true;
    if (worker.joinable()) worker.join();
}

void Updater::PresentUpdate(const std::string &u, const uint8_t s[32]) {
    std::lock_guard<std::mutex> lk(mu);
    url = u;
    for (int i = 0; i < 32; i++) sha[i] = s[i];
    state = PROMPTING;
    error.clear();
    fprintf(stderr, "[updater] PresentUpdate: url=%s\n", url.c_str());
}

void Updater::Consent() {
    if (worker.joinable()) worker.join();  // drain any previous run
    {
        std::lock_guard<std::mutex> lk(mu);
        if (state != PROMPTING && state != FAILED) return;
        state = DOWNLOADING;
        bytes_got = 0;
        bytes_total = 0;
        cancel_flag = false;  // reset under the lock, before worker starts
        error.clear();
    }
    worker = std::thread(&Updater::Run, this);
}

void Updater::Cancel() {
    cancel_flag = true;
    fprintf(stderr, "[updater] Cancel requested\n");
}

void Updater::Retry() {
    int n;
    {
        std::lock_guard<std::mutex> lk(mu);
        if (state != FAILED) return;
        n = ++retries;
    }
    // Note: the retry cap (plan says 3) is enforced by the UI layer.
    fprintf(stderr, "[updater] Retry #%d\n", n);
    Consent();
}

Updater::State Updater::GetState() {
    std::lock_guard<std::mutex> lk(mu);
    return state;
}

float Updater::GetProgress() {
    uint64_t tot = bytes_total.load();
    if (tot == 0) return 0.0f;
    double p = double(bytes_got.load()) / double(tot);
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    return float(p);
}

std::string Updater::GetErrorMessage() {
    std::lock_guard<std::mutex> lk(mu);
    return error;
}

int Updater::GetRetryCount() {
    std::lock_guard<std::mutex> lk(mu);
    return retries;
}

std::string Updater::GetDownloadURL() {
    std::lock_guard<std::mutex> lk(mu);
    return url;
}

void UpdaterSetProgress(Updater &u, uint64_t got, uint64_t total) {
    u.bytes_got = got;
    u.bytes_total = total;
}

void UpdaterCheckCancel(Updater &u, bool *out) {
    *out = u.cancel_flag.load();
}

static bool ProgressTrampoline(void *ctx, uint64_t got, uint64_t total) {
    Updater *u = static_cast<Updater*>(ctx);
    UpdaterSetProgress(*u, got, total);
    bool cancelled = false;
    UpdaterCheckCancel(*u, &cancelled);
    return !cancelled;
}

void Updater::Run() {
    // 1. Download to a temp path.
    const char *tmpdir =
#ifdef _WIN32
        getenv("TEMP");
#else
        "/tmp";
#endif
    if (!tmpdir) tmpdir = ".";
    std::string zippath = std::string(tmpdir) + "/zsilencer-update.zip";

    UpdaterDownload dl;
    int http = 0;
    std::string err;
    UpdaterDownload::Result dr = dl.Fetch(url, zippath, ProgressTrampoline, this, &http, &err);
    if (cancel_flag) {
        std::lock_guard<std::mutex> lk(mu);
        state = FAILED;
        error = "Cancelled";
        return;
    }
    if (dr != UpdaterDownload::OK) {
        std::lock_guard<std::mutex> lk(mu);
        state = FAILED;
        error = err.empty() ? "Network error" : err;
        fprintf(stderr, "[updater] download failed: %s\n", error.c_str());
        return;
    }

    // 2. Verify.
    {
        std::lock_guard<std::mutex> lk(mu);
        state = VERIFYING;
    }
    if (!VerifyFile(zippath, sha.data())) {
        std::lock_guard<std::mutex> lk(mu);
        state = FAILED;
        error = "Downloaded file corrupted (sha256 mismatch)";
        fprintf(stderr, "[updater] %s\n", error.c_str());
        return;
    }

    // 3. Hand off to stage-2. Caller (main loop) sees state=STAGING and
    //    performs the exec + exit. We don't fork from the worker thread —
    //    that needs to happen after SDL is torn down.
    {
        std::lock_guard<std::mutex> lk(mu);
        state = STAGING;
    }
    fprintf(stderr, "[updater] download + verify ok, handing off to stage-2\n");
}
