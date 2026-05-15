#include "btdebug.h"
#include "../shared.h"
#include "../sha1.h"
#include <vector>
#include <string>
#include <cstring>

// ── Static state ──────────────────────────────────────────────────────────────
static SOCKET                                        s_listen  = (SOCKET)-1;
static std::vector<SOCKET>                           s_clients; // handshake done
static std::vector<std::pair<SOCKET, std::string>>   s_pending; // awaiting WS handshake
static bool                                          s_inited  = false;

// ── Helpers ───────────────────────────────────────────────────────────────────
static std::string base64Encode(const unsigned char* src, size_t len) {
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned b  = (unsigned char)src[i] << 16;
        if (i+1 < len) b |= (unsigned char)src[i+1] << 8;
        if (i+2 < len) b |= (unsigned char)src[i+2];
        out += t[(b >> 18) & 0x3F];
        out += t[(b >> 12) & 0x3F];
        out += i+1 < len ? t[(b >>  6) & 0x3F] : '=';
        out += i+2 < len ? t[ b        & 0x3F] : '=';
    }
    return out;
}

static bool wsHandshake(SOCKET fd, const std::string& req) {
    const char* needle = "Sec-WebSocket-Key: ";
    auto pos = req.find(needle);
    if (pos == std::string::npos) return false;
    pos += strlen(needle);
    auto eol = req.find("\r\n", pos);
    if (eol == std::string::npos) return false;
    std::string key = req.substr(pos, eol - pos);
    // trim trailing whitespace
    while (!key.empty() && (key.back() == ' ' || key.back() == '\r')) key.pop_back();

    std::string combined = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned char hash[20];
    sha1::calc(combined.c_str(), (int)combined.size(), hash);
    std::string accept = base64Encode(hash, 20);

    std::string resp =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    send(fd, resp.c_str(), (int)resp.size(), 0);
    return true;
}

static void wsSendText(SOCKET fd, const std::string& payload) {
    size_t len = payload.size();
    unsigned char hdr[10];
    int hlen;
    hdr[0] = 0x81; // FIN + text opcode
    if (len < 126) {
        hdr[1] = (unsigned char)len;
        hlen = 2;
    } else if (len < 65536) {
        hdr[1] = 126;
        hdr[2] = (len >> 8) & 0xFF;
        hdr[3] =  len       & 0xFF;
        hlen = 4;
    } else {
        hdr[1] = 127;
        for (int i = 0; i < 8; ++i)
            hdr[2+i] = (len >> (56 - 8*i)) & 0xFF;
        hlen = 10;
    }
    send(fd, (const char*)hdr, hlen, 0);
    send(fd, payload.c_str(), (int)len, 0);
}

static void setNonBlocking(SOCKET fd) {
    unsigned long mode = 1;
    ioctl(fd, FIONBIO, &mode);
}

// ── Lazy init ─────────────────────────────────────────────────────────────────
static void lazyInit() {
    if (s_inited) return;
    s_inited = true;

    s_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_listen == (SOCKET)-1) return;

    int yes = 1;
    setsockopt(s_listen, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
    setNonBlocking(s_listen);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // localhost only
    addr.sin_port        = htons(9339);

    if (bind(s_listen, (sockaddr*)&addr, sizeof(addr)) != 0 ||
        listen(s_listen, 4) != 0) {
        closesocket(s_listen);
        s_listen = (SOCKET)-1;
    }
}

// ── Pump: accept + service pending handshakes ─────────────────────────────────
static void pumpConnections() {
    // Accept
    SOCKET newfd;
    while ((newfd = accept(s_listen, nullptr, nullptr)) != (SOCKET)-1) {
        setNonBlocking(newfd);
        s_pending.push_back({newfd, ""});
    }

    // Service pending handshakes
    std::vector<std::pair<SOCKET, std::string>> still;
    for (size_t i = 0; i < s_pending.size(); ++i) {
        SOCKET fd = s_pending[i].first;
        std::string& buf = s_pending[i].second;
        char tmp[2048];
        int n = recv(fd, tmp, (int)sizeof(tmp) - 1, 0);
        if (n > 0) {
            buf.append(tmp, n);
            if (buf.find("\r\n\r\n") != std::string::npos) {
                if (wsHandshake(fd, buf))
                    s_clients.push_back(fd);
                else
                    closesocket(fd);
                continue;
            }
        } else if (n == 0) {
            closesocket(fd);
            continue;
        }
        still.push_back({fd, std::move(buf)});
    }
    s_pending = std::move(still);
}

// ── Public API ────────────────────────────────────────────────────────────────
void BTDebug::broadcast(const std::string& actorType,
                        unsigned           actorId,
                        const std::unordered_map<std::string, json>& blackboard) {
    lazyInit();
    if (s_listen == (SOCKET)-1) return;

    pumpConnections();
    if (s_clients.empty()) return;

    json out;
    out["type"] = actorType;
    out["id"]   = actorId;
    json bb     = json::object();
    for (auto& kv : blackboard) bb[kv.first] = kv.second;
    out["blackboard"] = std::move(bb);
    std::string payload = out.dump();

    std::vector<SOCKET> alive;
    for (auto fd : s_clients) {
        // Drain any client frames (pings, close frames) without blocking
        char discard[256];
        recv(fd, discard, sizeof(discard), 0);

        int sent = send(fd, "", 0, 0); // probe
        (void)sent;
        wsSendText(fd, payload);
        alive.push_back(fd);
    }
    s_clients = std::move(alive);
}

void BTDebug::shutdown() {
    for (size_t i = 0; i < s_pending.size(); ++i) closesocket(s_pending[i].first);
    for (auto fd : s_clients)        closesocket(fd);
    s_pending.clear();
    s_clients.clear();
    if (s_listen != (SOCKET)-1) { closesocket(s_listen); s_listen = (SOCKET)-1; }
    s_inited = false;
}
