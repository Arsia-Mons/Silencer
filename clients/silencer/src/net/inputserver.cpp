#include "inputserver.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define CLOSE_SOCK closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSE_SOCK ::close
#endif

namespace {
const uint8_t kProtoVersion   = 0x01;
const uint8_t kMsgAction      = 0x01;
const uint8_t kMsgScancode    = 0x02;
const uint8_t kMsgMouse       = 0x03;
const size_t  kActionPayload  = 12;
const size_t  kMousePayload   = 5;
// Bytes needed to cover all SDL scancodes as a bitmask (rounded up).
const size_t  kScancodeBytes  = (SDL_SCANCODE_COUNT + 7) / 8;

// Bit positions in the u32 keymask. MUST match clients/tui/src/input_client.ts.
enum : uint32_t {
	BIT_keymoveup        = 1u << 0,
	BIT_keymovedown      = 1u << 1,
	BIT_keymoveleft      = 1u << 2,
	BIT_keymoveright     = 1u << 3,
	BIT_keylookupleft    = 1u << 4,
	BIT_keylookupright   = 1u << 5,
	BIT_keylookdownleft  = 1u << 6,
	BIT_keylookdownright = 1u << 7,
	BIT_keynextinv       = 1u << 8,
	BIT_keynextcam       = 1u << 9,
	BIT_keyprevcam       = 1u << 10,
	BIT_keydetonate      = 1u << 11,
	BIT_keyjump          = 1u << 12,
	BIT_keyjetpack       = 1u << 13,
	BIT_keyactivate      = 1u << 14,
	BIT_keyuse           = 1u << 15,
	BIT_keyfire          = 1u << 16,
	BIT_keydisguise      = 1u << 17,
	BIT_keynextweapon    = 1u << 18,
	BIT_keyup            = 1u << 19,
	BIT_keydown          = 1u << 20,
	BIT_keyleft          = 1u << 21,
	BIT_keyright         = 1u << 22,
	BIT_keychat          = 1u << 23,
};

uint32_t read_u32_le(const uint8_t* p){
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
uint16_t read_u16_le(const uint8_t* p){
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

bool RecvAll(int fd, void* buf, size_t n){
	uint8_t* p = (uint8_t*)buf;
	while(n > 0){
		int r = (int)::recv(fd, (char*)p, (int)n, 0);
		if(r <= 0) return false;
		p += r;
		n -= (size_t)r;
	}
	return true;
}

void Decode(const uint8_t* payload, Input& out){
	uint32_t k = read_u32_le(payload);
	uint8_t  w = payload[4];
	out.keymoveup        = (k & BIT_keymoveup)        != 0;
	out.keymovedown      = (k & BIT_keymovedown)      != 0;
	out.keymoveleft      = (k & BIT_keymoveleft)      != 0;
	out.keymoveright     = (k & BIT_keymoveright)     != 0;
	out.keylookupleft    = (k & BIT_keylookupleft)    != 0;
	out.keylookupright   = (k & BIT_keylookupright)   != 0;
	out.keylookdownleft  = (k & BIT_keylookdownleft)  != 0;
	out.keylookdownright = (k & BIT_keylookdownright) != 0;
	out.keynextinv       = (k & BIT_keynextinv)       != 0;
	out.keynextcam       = (k & BIT_keynextcam)       != 0;
	out.keyprevcam       = (k & BIT_keyprevcam)       != 0;
	out.keydetonate      = (k & BIT_keydetonate)      != 0;
	out.keyjump          = (k & BIT_keyjump)          != 0;
	out.keyjetpack       = (k & BIT_keyjetpack)       != 0;
	out.keyactivate      = (k & BIT_keyactivate)      != 0;
	out.keyuse           = (k & BIT_keyuse)           != 0;
	out.keyfire          = (k & BIT_keyfire)          != 0;
	out.keydisguise      = (k & BIT_keydisguise)      != 0;
	out.keynextweapon    = (k & BIT_keynextweapon)    != 0;
	out.keyup            = (k & BIT_keyup)            != 0;
	out.keydown          = (k & BIT_keydown)          != 0;
	out.keyleft          = (k & BIT_keyleft)          != 0;
	out.keyright         = (k & BIT_keyright)         != 0;
	out.keychat          = (k & BIT_keychat)          != 0;
	out.keyweapon[0]     = (w & 0x01) != 0;
	out.keyweapon[1]     = (w & 0x02) != 0;
	out.keyweapon[2]     = (w & 0x04) != 0;
	out.keyweapon[3]     = (w & 0x08) != 0;
	out.mousedown        = (w & 0x10) != 0;
	out.mousex           = read_u16_le(payload + 5);
	out.mousey           = read_u16_le(payload + 7);
}
}  // namespace

InputServer::InputServer()
	: listenfd(-1), running(false), connfd(-1),
	  have_action(false), have_sc(false),
	  mouse_x(0), mouse_y(0), mouse_down(false), have_mouse(false) {
	memset(sc_state, 0, sizeof(sc_state));
}

InputServer::~InputServer(){ Stop(); }

bool InputServer::Start(int port){
	if(port <= 0) return false;
	if(running.load()) return false;
	listenfd = (int)::socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd < 0){
		fprintf(stderr, "[input] socket() failed\n");
		return false;
	}
	int yes = 1;
	::setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short)port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if(::bind(listenfd, (sockaddr*)&addr, sizeof(addr)) < 0){
		fprintf(stderr, "[input] bind() to 127.0.0.1:%d failed\n", port);
		CLOSE_SOCK(listenfd);
		listenfd = -1;
		return false;
	}
	if(::listen(listenfd, 1) < 0){
		fprintf(stderr, "[input] listen() failed\n");
		CLOSE_SOCK(listenfd);
		listenfd = -1;
		return false;
	}
	running = true;
	acceptthread = std::thread(&InputServer::AcceptLoop, this);
	fprintf(stderr, "[input] listening on 127.0.0.1:%d\n", port);
	return true;
}

void InputServer::Stop(){
	if(!running.exchange(false)) return;
	if(listenfd >= 0){
		CLOSE_SOCK(listenfd);
		listenfd = -1;
	}
	if(acceptthread.joinable()) acceptthread.join();

	int my_connfd = -1;
	{
		std::lock_guard<std::mutex> lk(conn_mu);
		my_connfd = connfd;
		connfd = -1;
	}
	if(my_connfd >= 0) CLOSE_SOCK(my_connfd);

	std::thread t;
	{
		std::lock_guard<std::mutex> lk(conn_mu);
		t = std::move(connthread);
	}
	if(t.joinable()) t.join();
}

void InputServer::AcceptLoop(){
	while(running.load()){
		sockaddr_in caddr{};
		socklen_t clen = sizeof(caddr);
		int fd = (int)::accept(listenfd, (sockaddr*)&caddr, &clen);
		if(fd < 0){
			if(!running.load()) break;
			continue;
		}
		// Single-client channel — drop any prior connection.
		int prev = -1;
		std::thread prev_thread;
		{
			std::lock_guard<std::mutex> lk(conn_mu);
			prev = connfd;
			prev_thread = std::move(connthread);
			connfd = fd;
		}
		if(prev >= 0) CLOSE_SOCK(prev);
		if(prev_thread.joinable()) prev_thread.join();

		int one = 1;
		::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
		std::lock_guard<std::mutex> lk(conn_mu);
		connthread = std::thread(&InputServer::HandleConnection, this, fd);
	}
}

void InputServer::HandleConnection(int fd){
	uint8_t version = 0;
	if(!RecvAll(fd, &version, 1) || version != kProtoVersion){
		fprintf(stderr, "[input] handshake failed (version=%u)\n", version);
		int my_fd = -1;
		{
			std::lock_guard<std::mutex> lk(conn_mu);
			if(connfd == fd){
				my_fd = connfd;
				connfd = -1;
			}
		}
		if(my_fd >= 0) CLOSE_SOCK(my_fd);
		return;
	}
	uint8_t hdr[3];
	uint8_t payload[256];
	while(running.load() && RecvAll(fd, hdr, 3)){
		uint8_t type = hdr[0];
		uint16_t len = read_u16_le(hdr + 1);
		if(len > sizeof(payload)){
			fprintf(stderr, "[input] oversized payload %u\n", len);
			break;
		}
		if(len > 0 && !RecvAll(fd, payload, len)) break;
		if(type == kMsgAction && len == kActionPayload){
			Input next;
			Decode(payload, next);
			std::lock_guard<std::mutex> lk(action_mu);
			action_snap = next;
			have_action = true;
			continue;
		}
		if(type == kMsgScancode && len == kScancodeBytes){
			// Decode the scancode bitmask into the keystate-shaped Uint8 array
			// the engine consumes (1 byte per scancode for SDL parity).
			std::lock_guard<std::mutex> lk(sc_mu);
			for(int sc = 0; sc < SDL_SCANCODE_COUNT; ++sc){
				int byte = sc >> 3;
				int bit  = sc & 7;
				sc_state[sc] = (payload[byte] >> bit) & 1;
			}
			have_sc = true;
			continue;
		}
		if(type == kMsgMouse && len == kMousePayload){
			std::lock_guard<std::mutex> lk(mouse_mu);
			mouse_x    = read_u16_le(payload);
			mouse_y    = read_u16_le(payload + 2);
			mouse_down = (payload[4] & 0x01) != 0;
			have_mouse = true;
			continue;
		}
		// Unknown / mismatched types silently dropped — forward-compat.
	}
	int my_fd = -1;
	{
		std::lock_guard<std::mutex> lk(conn_mu);
		if(connfd == fd){
			my_fd = connfd;
			connfd = -1;
		}
	}
	if(my_fd >= 0) CLOSE_SOCK(my_fd);
}

bool InputServer::LatestAction(Input& out){
	std::lock_guard<std::mutex> lk(action_mu);
	if(!have_action) return false;
	out = action_snap;
	return true;
}

bool InputServer::LatestScancodes(Uint8* out){
	std::lock_guard<std::mutex> lk(sc_mu);
	if(!have_sc) return false;
	memcpy(out, sc_state, sizeof(sc_state));
	return true;
}

bool InputServer::LatestMouse(Uint16& x, Uint16& y, bool& down){
	std::lock_guard<std::mutex> lk(mouse_mu);
	if(!have_mouse) return false;
	x    = mouse_x;
	y    = mouse_y;
	down = mouse_down;
	return true;
}
