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

struct StringBuf {
    std::string data;
    static const size_t kLimit = 1024 * 1024; // 1 MB cap for map list JSON
};

size_t StringWriteCallback(void * ptr, size_t sz, size_t nmemb, void * userdata) {
    StringBuf * buf = static_cast<StringBuf *>(userdata);
    size_t incoming = sz * nmemb;
    if (buf->data.size() + incoming > StringBuf::kLimit) {
        return 0;
    }
    buf->data.append(static_cast<const char *>(ptr), incoming);
    return incoming;
}

// Extract the string value of a JSON key from a simple (non-nested) object body.
std::string JsonStringField(const std::string & obj, const char * key) {
    std::string pat = std::string("\"") + key + "\":\"";
    size_t p = obj.find(pat);
    if (p == std::string::npos) return "";
    p += pat.size();
    size_t end = obj.find('"', p);
    if (end == std::string::npos) return "";
    return obj.substr(p, end - p);
}

// Decode a 40-char lowercase/uppercase hex string into 20 bytes.
bool HexDecode20(const std::string & hex, unsigned char out[20]) {
    if (hex.size() != 40) return false;
    for (int i = 0; i < 20; i++) {
        auto fromHex = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = fromHex(hex[i * 2]), lo = fromHex(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return true;
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

static StringBuf FetchMapListJSON(const char * apiURL) {
    std::string url = apiURL;
    url += "/api/maps";

    StringBuf buf;
    CURL * curl = curl_easy_init();
    if (!curl) return buf;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "zsilencer/" ZSILENCER_VERSION);

    CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        fprintf(stderr, "[mapfetch] failed to fetch map list from %s: curl=%d\n", apiURL, (int)rc);
        buf.data.clear();
    }
    return buf;
}

std::vector<std::pair<std::string, std::string>> FetchServerMapList(const char * apiURL) {
    std::vector<std::pair<std::string, std::string>> result;
    StringBuf buf = FetchMapListJSON(apiURL);
    const std::string & json = buf.data;
    size_t pos = 0;
    while (true) {
        size_t start = json.find('{', pos);
        if (start == std::string::npos) break;
        size_t end = json.find('}', start);
        if (end == std::string::npos) break;
        pos = end + 1;
        std::string obj     = json.substr(start, end - start + 1);
        std::string name    = JsonStringField(obj, "name");
        std::string sha1hex = JsonStringField(obj, "sha1");
        if (!name.empty() && sha1hex.size() == 40) {
            result.push_back({name, sha1hex});
        }
    }
    return result;
}

void FetchAndSyncServerMaps(const char * apiURL) {
    StringBuf buf = FetchMapListJSON(apiURL);
    const std::string & json = buf.data;
    size_t pos = 0;
    while (true) {
        size_t start = json.find('{', pos);
        if (start == std::string::npos) break;
        size_t end = json.find('}', start);
        if (end == std::string::npos) break;
        pos = end + 1;

        std::string obj    = json.substr(start, end - start + 1);
        std::string name   = JsonStringField(obj, "name");
        std::string sha1hex = JsonStringField(obj, "sha1");
        if (name.empty() || sha1hex.size() != 40) continue;

        // Skip if already present in level/download/.
        std::string dlpath = GetDataDir() + "level/download/" + name;
        SDL_RWops * existing = SDL_RWFromFile(dlpath.c_str(), "rb");
        if (existing) {
            SDL_RWclose(existing);
            continue;
        }

        unsigned char sha1bytes[20];
        if (!HexDecode20(sha1hex, sha1bytes)) continue;

        FetchMapFromServer(name.c_str(), sha1bytes, apiURL);
    }
}
