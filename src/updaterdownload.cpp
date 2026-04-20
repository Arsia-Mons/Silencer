#include "updaterdownload.h"
#include <curl/curl.h>
#include <cstdio>
#include <cstring>

bool UpdaterDownload::IsAllowed(const std::string &url) {
    const std::string https = "https://";
    if (url.compare(0, https.size(), https) == 0) return true;

    const std::string http = "http://";
    if (url.compare(0, http.size(), http) != 0) return false;

    // Loopback hosts only.
    std::string rest = url.substr(http.size());
    std::string host;
    if (!rest.empty() && rest[0] == '[') {
        // IPv6 literal: [::1] or [::1]:port
        size_t close = rest.find(']');
        host = (close == std::string::npos) ? rest : rest.substr(0, close + 1);
    } else {
        // Strip port or path: take everything up to ':' or '/'.
        size_t end = rest.find_first_of(":/");
        host = (end == std::string::npos) ? rest : rest.substr(0, end);
    }
    if (host == "localhost") return true;
    if (host == "127.0.0.1") return true;
    if (host == "[::1]") return true;
    return false;
}

namespace {

struct WriteCtx {
    FILE *fp;
};

size_t WriteCallback(void *buf, size_t sz, size_t nmemb, void *userdata) {
    WriteCtx *ctx = static_cast<WriteCtx*>(userdata);
    return fwrite(buf, sz, nmemb, ctx->fp);
}

struct ProgressCtx {
    bool (*cb)(void *, uint64_t, uint64_t);
    void *user;
};

int CurlProgress(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    ProgressCtx *ctx = static_cast<ProgressCtx*>(p);
    if (!ctx->cb) return 0;
    return ctx->cb(ctx->user, uint64_t(dlnow), uint64_t(dltotal)) ? 0 : 1;
}

} // namespace

UpdaterDownload::UpdaterDownload() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

UpdaterDownload::~UpdaterDownload() {
    curl_global_cleanup();
}

UpdaterDownload::Result UpdaterDownload::Fetch(
    const std::string &url, const std::string &outpath,
    bool (*progress_cb)(void *, uint64_t, uint64_t), void *user,
    int *http_status_out, std::string *err_out)
{
    if (!IsAllowed(url)) {
        if (err_out) *err_out = "scheme/host not allowed: " + url;
        fprintf(stderr, "[updater] download rejected (scheme): %s\n", url.c_str());
        return HTTP_ERROR;
    }

    FILE *fp = fopen(outpath.c_str(), "wb");
    if (!fp) {
        if (err_out) *err_out = "cannot open " + outpath;
        fprintf(stderr, "[updater] download fopen failed: %s\n", outpath.c_str());
        return IO_ERROR;
    }

    WriteCtx wctx{fp};
    ProgressCtx pctx{progress_cb, user};
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wctx);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, CurlProgress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &pctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "zsilencer-updater/1.0");

    CURLcode rc = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    if (http_status_out) *http_status_out = int(http);
    curl_easy_cleanup(curl);
    fclose(fp);

    if (rc == CURLE_ABORTED_BY_CALLBACK) {
        fprintf(stderr, "[updater] download aborted by user\n");
        return ABORTED;
    }
    if (rc != CURLE_OK) {
        if (err_out) *err_out = curl_easy_strerror(rc);
        fprintf(stderr, "[updater] download failed: curl=%d http=%ld url=%s msg=%s\n",
            (int)rc, http, url.c_str(), curl_easy_strerror(rc));
        return (http >= 400) ? HTTP_ERROR : NETWORK_ERROR;
    }
    fprintf(stderr, "[updater] download ok: %s → %s (http=%ld)\n", url.c_str(), outpath.c_str(), http);
    return OK;
}
