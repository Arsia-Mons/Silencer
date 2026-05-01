#include "tuibackend.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
const Uint8 kMsgPalette = 0x01;
const Uint8 kMsgFrame   = 0x02;

void put_u32_le(Uint8 *p, Uint32 v){
	p[0] = (Uint8)(v & 0xFF);
	p[1] = (Uint8)((v >> 8) & 0xFF);
	p[2] = (Uint8)((v >> 16) & 0xFF);
	p[3] = (Uint8)((v >> 24) & 0xFF);
}

void put_u16_le(Uint8 *p, Uint16 v){
	p[0] = (Uint8)(v & 0xFF);
	p[1] = (Uint8)((v >> 8) & 0xFF);
}
}

TUIBackend::TUIBackend(){}

TUIBackend::~TUIBackend(){ Shutdown(); }

bool TUIBackend::Init(SDL_Window * /*window*/){
	const char *host = std::getenv("SILENCER_TUI_FRAME_HOST");
	const char *port_s = std::getenv("SILENCER_TUI_FRAME_PORT");
	if(!host || !port_s){
		fprintf(stderr, "[tui] SILENCER_TUI_FRAME_HOST / _PORT not set\n");
		return false;
	}
	int port = atoi(port_s);
	if(port <= 0){
		fprintf(stderr, "[tui] invalid frame port %s\n", port_s);
		return false;
	}

	sock = (int)socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0){
		fprintf(stderr, "[tui] socket() failed\n");
		return false;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short)port);
	if(inet_pton(AF_INET, host, &addr.sin_addr) != 1){
		fprintf(stderr, "[tui] bad host %s\n", host);
		return false;
	}
	if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0){
		fprintf(stderr, "[tui] connect() to %s:%d failed\n", host, port);
		return false;
	}

	int one = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));

	fprintf(stderr, "[tui] connected to frame host %s:%d\n", host, port);
	return true;
}

void TUIBackend::Shutdown(){
	if(sock >= 0){
#ifdef _WIN32
		closesocket(sock);
#else
		close(sock);
#endif
		sock = -1;
	}
}

bool TUIBackend::WriteAll(const void *data, size_t len){
	const char *p = (const char *)data;
	size_t left = len;
	while(left > 0){
#ifdef _WIN32
		int n = send(sock, p, (int)left, 0);
#else
		ssize_t n = send(sock, p, left, 0);
#endif
		if(n <= 0){
			// Frontend disconnected. Tear the socket down so subsequent
			// Present() calls bail at the sock < 0 guard, and IsAlive()
			// flips to false so the game loop exits instead of spinning.
			Shutdown();
			return false;
		}
		p += n;
		left -= (size_t)n;
	}
	return true;
}

void TUIBackend::SetPalette(const SDL_Color *colors, int count){
	if(count > 256) count = 256;
	for(int i = 0; i < count; i++){
		palette_colors[i] = colors[i];
		palette_colors[i].a = 255;
	}
	palette_dirty = true;
}

void TUIBackend::UploadFrame(const Uint8 *indexed_pixels, int w, int h){
	pending_pixels = indexed_pixels;
	pending_w = w;
	pending_h = h;
	frame_dirty = true;
}

void TUIBackend::Present(){
	if(sock < 0) return;

	if(palette_dirty){
		Uint8 hdr[5];
		hdr[0] = kMsgPalette;
		put_u32_le(hdr + 1, 256 * 4);
		Uint8 payload[256 * 4];
		for(int i = 0; i < 256; i++){
			payload[i*4 + 0] = palette_colors[i].r;
			payload[i*4 + 1] = palette_colors[i].g;
			payload[i*4 + 2] = palette_colors[i].b;
			payload[i*4 + 3] = 255;
		}
		if(!WriteAll(hdr, sizeof(hdr))) return;
		if(!WriteAll(payload, sizeof(payload))) return;
		palette_dirty = false;
	}

	if(frame_dirty && pending_pixels){
		size_t pixels = (size_t)pending_w * (size_t)pending_h;
		Uint8 hdr[5];
		hdr[0] = kMsgFrame;
		put_u32_le(hdr + 1, (Uint32)(4 + pixels));
		Uint8 dims[4];
		put_u16_le(dims + 0, (Uint16)pending_w);
		put_u16_le(dims + 2, (Uint16)pending_h);
		if(!WriteAll(hdr, sizeof(hdr))) return;
		if(!WriteAll(dims, sizeof(dims))) return;
		if(!WriteAll(pending_pixels, pixels)) return;
		frame_dirty = false;
	}
}
