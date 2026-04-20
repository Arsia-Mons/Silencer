#ifndef UPDATERDOWNLOAD_H
#define UPDATERDOWNLOAD_H

#include <cstddef>
#include <cstdint>
#include <string>

// Blocking HTTPS (and loopback-only HTTP) downloader used by the updater.
// Not threaded internally — callers drive it from a worker thread.
class UpdaterDownload {
public:
    enum Result { OK = 0, NETWORK_ERROR, HTTP_ERROR, ABORTED, IO_ERROR };

    // Scheme validation: https anywhere, http only when the host is loopback.
    // Pure function: safe to call from tests without a network.
    static bool IsAllowed(const std::string &url);

    UpdaterDownload();
    ~UpdaterDownload();

    // Download url → outpath (truncating). Progress callback receives
    // (bytes_so_far, total_bytes_hint) where total may be 0 if the server
    // didn't send Content-Length. Return true from progress_cb to continue,
    // false to abort.
    Result Fetch(const std::string &url,
                 const std::string &outpath,
                 bool (*progress_cb)(void *ctx, uint64_t got, uint64_t total),
                 void *ctx,
                 int *http_status_out,
                 std::string *err_out);
};

#endif
