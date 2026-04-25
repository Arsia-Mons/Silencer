#include "mapfetch.h"
#include "sha1.h"
#include "shared.h"
#include "os.h"
#include <SDL.h>
#include <curl/curl.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

namespace {

struct MemBuf {
    std::vector<unsigned char> data;
    // Refuse to buffer more than the engine's hard limit to avoid a giant
    // allocation on a malicious or misconfigured server.
    static const size_t kLimit = 65535;
};

size_t WriteCallback(void * ptr, size_t sz, size_t nmemb, void * userdata) {
    MemBuf * buf = static_cast<MemBuf *>(userdata);
    size_t incoming = sz * nmemb;
    if (buf->data.size() + incoming > MemBuf::kLimit) {
        // Signal abort by returning a value != incoming.
        fprintf(stderr, "[mapfetch] response too large\n");
        return 0;
    }
    const unsigned char * p = static_cast<const unsigned char *>(ptr);
    buf->data.insert(buf->data.end(), p, p + incoming);
    return incoming;
}

} // namespace

std::string FetchMapFromServer(const char * mapname,
                               const unsigned char * sha1hash,
                               const char * apiURL)
{
    // Build URL: {apiURL}/api/maps/by-sha1/{sha1hex}
    char sha1hex[41];
    for (int i = 0; i < 20; i++) {
        sprintf(&sha1hex[i * 2], "%.2x", sha1hash[i]);
    }
    sha1hex[40] = '\0';

    std::string url = apiURL;
    url += "/api/maps/by-sha1/";
    url += sha1hex;

    MemBuf buf;
    buf.data.reserve(65536);

    CURL * curl = curl_easy_init();
    if (!curl) return "";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "zsilencer/" ZSILENCER_VERSION);

    CURLcode rc = curl_easy_perform(curl);
    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
    curl_easy_cleanup(curl);

    if (rc == CURLE_ABORTED_BY_CALLBACK) {
        // WriteCallback refused oversized response.
        return "";
    }
    if (rc != CURLE_OK) {
        // 404 is normal (map not published yet); log only unexpected errors.
        if (httpStatus != 404) {
            fprintf(stderr, "[mapfetch] failed to fetch %s: curl=%d http=%ld\n",
                    mapname, (int)rc, httpStatus);
        }
        return "";
    }
    if (buf.data.empty()) {
        return "";
    }

    // Verify SHA-1 before touching the filesystem.
    unsigned char computed[20];
    sha1::calc(buf.data.data(), (int)buf.data.size(), computed);
    if (memcmp(computed, sha1hash, 20) != 0) {
        fprintf(stderr, "[mapfetch] SHA-1 mismatch for %s — discarding\n", mapname);
        return "";
    }

    // Save to <DataDir>/level/download/<mapname>.
    CDDataDir();
    std::string dir = GetDataDir() + "level/download";
    CreateDirectory(dir.c_str());
    std::string path = dir + "/" + mapname;

    SDL_RWops * file = SDL_RWFromFile(path.c_str(), "wb");
    if (!file) {
        fprintf(stderr, "[mapfetch] cannot write %s\n", path.c_str());
        return "";
    }
    SDL_RWwrite(file, buf.data.data(), 1, buf.data.size());
    SDL_RWclose(file);

    fprintf(stderr, "[mapfetch] downloaded %s (%zu bytes) from %s\n",
            mapname, buf.data.size(), apiURL);
    return path;
}
