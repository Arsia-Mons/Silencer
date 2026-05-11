#include "relay.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

// Stage 2 relay implementation. Hand-rolled HTTP→WebSocket upgrade +
// binary frame writer; no third-party dep. Mirrors the Go-side
// services/lobby/wsutil.go logic (same RFC 6455 subset).

#include "sha1.h"

namespace {

constexpr int kRelayBacklog       = 32;
constexpr size_t kMaxPendingBytes = 8 * 1024 * 1024; // 8 MB per-client outbox cap

const char *kMagicGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

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
		ssize_t n = recv(sock, &c, 1, 0);
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
		ssize_t n = send(sock, p, len, 0);
		if (n <= 0) {
			if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
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

} // namespace

Relay::Relay()  = default;
Relay::~Relay() {
	if (listenSock >= 0) closesocket(listenSock);
	std::lock_guard<std::mutex> g(clientsMu);
	for (auto *c : clients) {
		if (c->sock >= 0) closesocket(c->sock);
		delete c;
	}
}

int Relay::Run(const char *lobbyAddr, unsigned short lobbyPort,
               Uint32 gameId, unsigned short wsPort) {
	(void)lobbyAddr; (void)lobbyPort;

	// --- Listen socket ---------------------------------------------------
	listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock < 0) {
		fprintf(stderr, "[relay] socket: %s\n", strerror(errno));
		return 1;
	}
	int yes = 1;
	setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

	sockaddr_in addr{};
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port        = htons(wsPort);
	if (bind(listenSock, (sockaddr *)&addr, sizeof(addr)) != 0) {
		fprintf(stderr, "[relay] bind :%u: %s\n", wsPort, strerror(errno));
		return 1;
	}
	if (listen(listenSock, kRelayBacklog) != 0) {
		fprintf(stderr, "[relay] listen: %s\n", strerror(errno));
		return 1;
	}
	fprintf(stderr, "[relay] game=%u listening on ws://0.0.0.0:%u/\n",
	        (unsigned)gameId, (unsigned)wsPort);

	// --- Accept + heartbeat loop -----------------------------------------
	// Stage 2 stub: until PR #148 Phase 3 lands the spectator-peer UDP
	// path, we just emit a synthetic 4-byte heartbeat every 250ms so
	// browsers can confirm the WS pipe works end-to-end.
	auto lastBeat = std::chrono::steady_clock::now();
	while (true) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(listenSock, &rfds);
		timeval tv{0, 50 * 1000};
		int maxFd = listenSock;
		if (select(maxFd + 1, &rfds, nullptr, nullptr, &tv) > 0) {
			if (FD_ISSET(listenSock, &rfds)) AcceptOne(listenSock);
		}

		auto now = std::chrono::steady_clock::now();
		if (now - lastBeat >= std::chrono::milliseconds(250)) {
			lastBeat = now;
			static unsigned int seq = 0;
			seq++;
			unsigned char beat[8] = {
			    'B', 'E', 'A', 'T',
			    (unsigned char)(seq), (unsigned char)(seq >> 8),
			    (unsigned char)(seq >> 16), (unsigned char)(seq >> 24),
			};
			Broadcast(beat, sizeof(beat));
		}
	}
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
	return true;
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
		ssize_t n = send(c.sock, (const char *)c.sendBuf.data(), c.sendBuf.size(), 0);
		if (n > 0) {
			c.sendBuf.erase(c.sendBuf.begin(), c.sendBuf.begin() + n);
		} else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
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
