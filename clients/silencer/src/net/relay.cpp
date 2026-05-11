#include "relay.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#include "sha1.h"
#include "../world/serializer.h"

// Hand-rolled HTTP→WebSocket upgrade + binary-frame writer (RFC 6455
// subset). Mirrors services/lobby/wsutil.go on the Go side.

namespace {

constexpr int kRelayBacklog       = 32;
constexpr size_t kMaxPendingBytes = 8 * 1024 * 1024; // per-client outbox cap

// World::MSG_* enum values mirrored from src/world/world.h. Hand-copied
// to avoid pulling the entire engine TU into the relay; if the enum
// order ever changes upstream, the build will not catch it — keep these
// in sync.
constexpr Uint8 kMsgConnect    = 0;
constexpr Uint8 kMsgPeerlist   = 3;
constexpr Uint8 kMsgDisconnect = 4;
constexpr Uint8 kMsgPing       = 5;
constexpr Uint8 kMsgGameinfo   = 7;

constexpr int kPingIntervalMs        = 1000;  // safely under any reasonable peertimeout
constexpr int kConnectRetryIntervalMs = 500;  // until AUTHORITY admits us
// Periodic MSG_PEERLIST request so newly-attached browser WS clients
// see a fresh peerlist within this window — the AUTHORITY only
// broadcasts it on peer admit/disconnect, which races our WS attach.
constexpr int kPeerlistRefreshIntervalMs = 500;

const char *kMagicGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// Windows Winsock uses WSAEWOULDBLOCK (10035), not POSIX EWOULDBLOCK (140).
// shared.h aliases errno → WSAGetLastError() on Win, so compare against the
// Winsock value there.
#ifdef _WIN32
constexpr int kWouldBlock = WSAEWOULDBLOCK;
typedef int relay_ssize_t;
#else
constexpr int kWouldBlock = EWOULDBLOCK;
typedef ssize_t relay_ssize_t;
#endif

// Base64 of the SHA-1 of (key + magic). Tiny inlined encoder because
// the rest of the codebase doesn't need one anywhere else.
std::string Base64(const unsigned char *data, size_t len) {
	static const char *tbl =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	out.reserve(((len + 2) / 3) * 4);
	for (size_t i = 0; i < len; i += 3) {
		unsigned int v = data[i] << 16;
		if (i + 1 < len) v |= data[i + 1] << 8;
		if (i + 2 < len) v |= data[i + 2];
		out.push_back(tbl[(v >> 18) & 0x3F]);
		out.push_back(tbl[(v >> 12) & 0x3F]);
		out.push_back(i + 1 < len ? tbl[(v >> 6) & 0x3F] : '=');
		out.push_back(i + 2 < len ? tbl[v & 0x3F]       : '=');
	}
	return out;
}

bool ReadLine(int sock, std::string &out, size_t cap) {
	out.clear();
	char c;
	while (out.size() < cap) {
		relay_ssize_t n = recv(sock, &c, 1, 0);
		if (n <= 0) return false;
		if (c == '\n') {
			if (!out.empty() && out.back() == '\r') out.pop_back();
			return true;
		}
		out.push_back(c);
	}
	return false; // line too long
}

bool SendAll(int sock, const void *data, size_t len) {
	const char *p = (const char *)data;
	while (len > 0) {
		relay_ssize_t n = send(sock, p, (int)len, 0);
		if (n <= 0) {
			if (n < 0 && (errno == kWouldBlock)) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}
			return false;
		}
		p   += n;
		len -= (size_t)n;
	}
	return true;
}

unsigned long long NowMs() {
	using namespace std::chrono;
	return (unsigned long long)duration_cast<milliseconds>(
	    steady_clock::now().time_since_epoch()).count();
}

} // namespace

Relay::Relay()  = default;
Relay::~Relay() {
	if (listenSock >= 0) closesocket(listenSock);
	if (udpSock >= 0) closesocket(udpSock);
	std::lock_guard<std::mutex> g(clientsMu);
	for (auto *c : clients) {
		if (c->sock >= 0) closesocket(c->sock);
		delete c;
	}
}

int Relay::Run(const char *peerHost, unsigned short peerPort,
               Uint32 gameIdIn, unsigned short wsPort) {
	gameId = gameIdIn;

	// --- AUTHORITY peer address ------------------------------------------
	peerAddr.sin_family      = AF_INET;
	peerAddr.sin_port        = htons(peerPort);
	peerAddr.sin_addr.s_addr = inet_addr(peerHost);
	if (peerAddr.sin_addr.s_addr == INADDR_NONE) {
		fprintf(stderr, "[relay] bad peer host '%s' (must be dotted-decimal IPv4)\n",
		        peerHost);
		return 1;
	}

	// --- UDP socket to AUTHORITY -----------------------------------------
	udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (udpSock < 0) {
		fprintf(stderr, "[relay] udp socket: %s\n", strerror(errno));
		return 1;
	}
	{
		unsigned long iomode = 1;
		ioctl(udpSock, FIONBIO, &iomode);
	}
	{
		sockaddr_in bindAddr{};
		bindAddr.sin_family      = AF_INET;
		bindAddr.sin_addr.s_addr = INADDR_ANY;
		bindAddr.sin_port        = 0;
		if (bind(udpSock, (sockaddr *)&bindAddr, sizeof(bindAddr)) != 0) {
			fprintf(stderr, "[relay] udp bind: %s\n", strerror(errno));
			return 1;
		}
	}

	// --- WS listen socket ------------------------------------------------
	listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock < 0) {
		fprintf(stderr, "[relay] tcp socket: %s\n", strerror(errno));
		return 1;
	}
	int yes = 1;
	setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

	sockaddr_in addr{};
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port        = htons(wsPort);
	if (bind(listenSock, (sockaddr *)&addr, sizeof(addr)) != 0) {
		fprintf(stderr, "[relay] tcp bind :%u: %s\n", wsPort, strerror(errno));
		return 1;
	}
	if (listen(listenSock, kRelayBacklog) != 0) {
		fprintf(stderr, "[relay] listen: %s\n", strerror(errno));
		return 1;
	}
	fprintf(stderr, "[relay] game=%u listening on ws://0.0.0.0:%u/ (peer=%s:%u)\n",
	        (unsigned)gameId, (unsigned)wsPort, peerHost, (unsigned)peerPort);

	// --- Initial spectator handshake -------------------------------------
	SendConnectRequest();
	unsigned long long lastPing = NowMs();
	unsigned long long lastConnectRetry = NowMs();
	unsigned long long lastPeerlistRefresh = NowMs();

	// --- Main loop -------------------------------------------------------
	unsigned char udpBuf[16384];
	while (true) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(listenSock, &rfds);
		FD_SET(udpSock, &rfds);
		int maxFd = listenSock > udpSock ? listenSock : udpSock;
		timeval tv{0, 50 * 1000};
		int sel = select(maxFd + 1, &rfds, nullptr, nullptr, &tv);
		if (sel > 0) {
			if (FD_ISSET(listenSock, &rfds)) AcceptOne(listenSock);
			if (FD_ISSET(udpSock, &rfds)) {
				sockaddr_in src{};
				socklen_t srclen = sizeof(src);
				while (true) {
					relay_ssize_t n = recvfrom(udpSock, (char *)udpBuf, sizeof(udpBuf),
					                           0, (sockaddr *)&src, &srclen);
					if (n <= 0) break;
					// Only forward bytes from the AUTHORITY peer. (Stray traffic
					// from elsewhere shouldn't happen on a freshly bound ephemeral
					// port, but defense-in-depth: dropping unexpected senders
					// keeps the WS stream noise-free.)
					if (src.sin_addr.s_addr != peerAddr.sin_addr.s_addr ||
					    src.sin_port != peerAddr.sin_port) {
						continue;
					}
					if (!admitted) {
						admitted = true;
						fprintf(stderr, "[relay] AUTHORITY admitted us (first packet, %d bytes)\n",
						        (int)n);
					}
					if (n >= 1 && udpBuf[0] == kMsgDisconnect) {
						fprintf(stderr, "[relay] received MSG_DISCONNECT — shutting down\n");
						return 0;
					}
					// Stash bootstrap messages so newly-attaching WS clients
					// can be replayed them.
					if (n >= 1) {
						switch (udpBuf[0]) {
						case kMsgConnect:  lastConnect.assign(udpBuf, udpBuf + n);  break;
						case kMsgGameinfo: lastGameinfo.assign(udpBuf, udpBuf + n); break;
						case kMsgPeerlist: lastPeerlist.assign(udpBuf, udpBuf + n); break;
						default: break;
						}
					}
					Broadcast(udpBuf, (size_t)n);
				}
			}
		}

		unsigned long long now = NowMs();
		if (!admitted && now - lastConnectRetry >= (unsigned long long)kConnectRetryIntervalMs) {
			lastConnectRetry = now;
			SendConnectRequest();
		}
		if (admitted && now - lastPing >= (unsigned long long)kPingIntervalMs) {
			lastPing = now;
			SendPing();
		}
		if (admitted && now - lastPeerlistRefresh >= (unsigned long long)kPeerlistRefreshIntervalMs) {
			lastPeerlistRefresh = now;
			SendPeerlistRequest();
		}
	}
}

void Relay::SendPeerlistRequest() {
	// Single-byte MSG_PEERLIST request — AUTHORITY responds with the
	// current peer list. The reply gets forwarded verbatim to all
	// currently-connected WS clients, so browser tabs that attached
	// after AUTHORITY's admit-time broadcast still bootstrap their
	// peer list (which they need before MSG_SNAPSHOT processing
	// unblocks on the World::CONNECTING → CONNECTED transition).
	Uint8 code = kMsgPeerlist;
	sendto(udpSock, (const char *)&code, 1, 0,
	       (sockaddr *)&peerAddr, sizeof(peerAddr));
}

void Relay::SendConnectRequest() {
	// Wire layout matches World::Connect (src/world/world.cpp):
	// MSG_CONNECT + agency u8 + accountid u32 + passwordsize u8 +
	// (password bytes) + observer bit.
	Serializer data;
	Uint8 code = kMsgConnect;
	data.Put(code);
	Uint8 agency = 0;
	data.Put(agency);
	Uint32 accountid = 0; // anonymous spectator
	data.Put(accountid);
	Uint8 passwordsize = 0;
	data.Put(passwordsize);
	data.PutBit(true); // observer

	sendto(udpSock, data.data, (int)Serializer::BitsToBytes(data.offset), 0,
	       (sockaddr *)&peerAddr, sizeof(peerAddr));
}

void Relay::SendPing() {
	// Mirrors World::SendPing: MSG_PING + pingid u32. AUTHORITY replies
	// MSG_PONG; we don't track ping time so we drop the pong.
	Serializer data;
	Uint8 code = kMsgPing;
	data.Put(code);
	Uint32 pingid = (Uint32)NowMs();
	data.Put(pingid);

	sendto(udpSock, data.data, (int)Serializer::BitsToBytes(data.offset), 0,
	       (sockaddr *)&peerAddr, sizeof(peerAddr));
}

bool Relay::AcceptOne(int sock) {
	sockaddr_in peer{};
	socklen_t plen = sizeof(peer);
	int c = accept(sock, (sockaddr *)&peer, &plen);
	if (c < 0) return false;
	if (!DoHandshake(c)) {
		closesocket(c);
		return false;
	}
	// Non-blocking after handshake — broadcast writes must not stall on
	// a slow client.
	unsigned long iomode = 1;
	ioctl(c, FIONBIO, &iomode);

	auto *wc = new WSClient{c, {}, false};
	std::lock_guard<std::mutex> g(clientsMu);
	clients.push_back(wc);
	fprintf(stderr, "[relay] client connected (now %zu)\n", clients.size());

	// Replay the AUTHORITY's bootstrap messages so the new client doesn't
	// have to wait for the next peer admit/disconnect to bootstrap its
	// World state.
	if (!lastConnect.empty()) {
		SendFrameTo(*wc, lastConnect.data(), lastConnect.size());
	}
	if (!lastGameinfo.empty()) {
		SendFrameTo(*wc, lastGameinfo.data(), lastGameinfo.size());
	}
	if (!lastPeerlist.empty()) {
		SendFrameTo(*wc, lastPeerlist.data(), lastPeerlist.size());
	}
	return true;
}

void Relay::SendFrameTo(WSClient &c, const unsigned char *bytes, size_t len) {
	std::vector<unsigned char> frame;
	frame.reserve(len + 10);
	frame.push_back(0x82); // FIN | binary
	if (len < 126) {
		frame.push_back((unsigned char)len);
	} else if (len <= 0xFFFF) {
		frame.push_back(126);
		frame.push_back((unsigned char)(len >> 8));
		frame.push_back((unsigned char)len);
	} else {
		frame.push_back(127);
		for (int i = 7; i >= 0; i--) frame.push_back((unsigned char)(len >> (i * 8)));
	}
	frame.insert(frame.end(), bytes, bytes + len);
	c.sendBuf.insert(c.sendBuf.end(), frame.begin(), frame.end());
	DrainOutbox(c);
}

bool Relay::DoHandshake(int sock) {
	// Read request line + headers, collect Sec-WebSocket-Key.
	std::string line;
	if (!ReadLine(sock, line, 4096)) return false;
	// `GET /<path> HTTP/1.1` — we accept any path.
	if (line.compare(0, 4, "GET ") != 0) return false;

	std::string key;
	int wsVersion = 0;
	for (;;) {
		if (!ReadLine(sock, line, 4096)) return false;
		if (line.empty()) break;
		auto colon = line.find(':');
		if (colon == std::string::npos) continue;
		std::string name = line.substr(0, colon);
		std::string val  = line.substr(colon + 1);
		// trim leading spaces
		while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(0, 1);
		std::transform(name.begin(), name.end(), name.begin(),
		               [](char ch) { return (char)std::tolower((unsigned char)ch); });
		if (name == "sec-websocket-key")     key       = val;
		if (name == "sec-websocket-version") wsVersion = std::atoi(val.c_str());
	}
	if (key.empty()) return false;
	if (wsVersion != 13) return false;

	std::string combined = key + kMagicGuid;
	unsigned char digest[20];
	sha1::calc(combined.data(), (int)combined.size(), digest);
	std::string accept = Base64(digest, sizeof(digest));

	std::string resp =
	    "HTTP/1.1 101 Switching Protocols\r\n"
	    "Upgrade: websocket\r\n"
	    "Connection: Upgrade\r\n"
	    "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
	return SendAll(sock, resp.data(), resp.size());
}

void Relay::Broadcast(const unsigned char *bytes, size_t len) {
	// Build a single binary frame (opcode 0x2, unmasked, payload len).
	std::vector<unsigned char> frame;
	frame.reserve(len + 10);
	frame.push_back(0x82); // FIN | binary
	if (len < 126) {
		frame.push_back((unsigned char)len);
	} else if (len <= 0xFFFF) {
		frame.push_back(126);
		frame.push_back((unsigned char)(len >> 8));
		frame.push_back((unsigned char)len);
	} else {
		frame.push_back(127);
		for (int i = 7; i >= 0; i--) frame.push_back((unsigned char)(len >> (i * 8)));
	}
	frame.insert(frame.end(), bytes, bytes + len);

	std::lock_guard<std::mutex> g(clientsMu);
	for (auto *c : clients) {
		if (c->dead) continue;
		// Soft cap on per-client backlog; drop frames for stragglers
		// rather than stall the broadcast.
		if (c->sendBuf.size() + frame.size() > kMaxPendingBytes) {
			fprintf(stderr, "[relay] dropping frame for slow client (backlog %zu)\n",
			        c->sendBuf.size());
			continue;
		}
		c->sendBuf.insert(c->sendBuf.end(), frame.begin(), frame.end());
		DrainOutbox(*c);
	}
	// Reap dead clients.
	clients.erase(std::remove_if(clients.begin(), clients.end(),
	                              [this](WSClient *c) {
		                              if (c->dead) {
			                              CloseClient(*c);
			                              delete c;
			                              return true;
		                              }
		                              return false;
	                              }),
	              clients.end());
}

void Relay::DrainOutbox(WSClient &c) {
	while (!c.sendBuf.empty()) {
		relay_ssize_t n = send(c.sock, (const char *)c.sendBuf.data(),
		                       (int)c.sendBuf.size(), 0);
		if (n > 0) {
			c.sendBuf.erase(c.sendBuf.begin(), c.sendBuf.begin() + n);
		} else if (n < 0 && errno == kWouldBlock) {
			return; // try again next broadcast
		} else {
			c.dead = true;
			return;
		}
	}
}

void Relay::CloseClient(WSClient &c) {
	if (c.sock >= 0) {
		shutdown(c.sock, SHUT_RDWR);
		closesocket(c.sock);
		c.sock = -1;
	}
}
