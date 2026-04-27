#include "controlserver.h"
#include "controldispatch.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define CLOSE_SOCK closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSE_SOCK ::close
#endif

using nlohmann::json;

ControlServer::ControlServer() : listenfd(-1), port(0), running(false) {}

ControlServer::~ControlServer() {
	Stop();
}

bool ControlServer::Start(int p) {
	if(p <= 0){
		return false;
	}
	if(running.load()) return false;
	port = p;
	listenfd = (int)::socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd < 0){
		fprintf(stderr, "[control] socket() failed\n");
		return false;
	}
	int yes = 1;
	::setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons((unsigned short)port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 only
	if(::bind(listenfd, (sockaddr*)&addr, sizeof(addr)) < 0){
		fprintf(stderr, "[control] bind() to 127.0.0.1:%d failed\n", port);
		CLOSE_SOCK(listenfd);
		listenfd = -1;
		return false;
	}
	if(::listen(listenfd, 16) < 0){
		fprintf(stderr, "[control] listen() failed\n");
		CLOSE_SOCK(listenfd);
		listenfd = -1;
		return false;
	}
	running = true;
	acceptthread = std::thread(&ControlServer::AcceptLoop, this);
	fprintf(stderr, "[control] listening on 127.0.0.1:%d\n", port);
	return true;
}

void ControlServer::Stop() {
	if(!running.exchange(false)) return;
	if(listenfd >= 0){
		CLOSE_SOCK(listenfd); // unblocks accept()
		listenfd = -1;
	}
	if(acceptthread.joinable()) acceptthread.join();
	// Drain pending commands and fulfill their promises so the per-connection
	// threads can exit. They're detached, so we don't need to join them.
	std::vector<ControlCommand> stragglers;
	{
		std::lock_guard<std::mutex> lk(queue_mu);
		stragglers = std::move(immediate);
		auto pr = std::move(postrender);
		stragglers.insert(stragglers.end(),
			std::make_move_iterator(pr.begin()),
			std::make_move_iterator(pr.end()));
		immediate.clear();
		postrender.clear();
	}
	for(auto& c : stragglers){
		if(c.reply){
			ControlReply rpl;
			rpl.id = c.id;
			rpl.ok = false;
			rpl.code = "INTERNAL";
			rpl.error = "server stopping";
			c.reply->set_value(rpl);
		}
	}
}

void ControlServer::AcceptLoop() {
	while(running.load()){
		sockaddr_in caddr{};
		socklen_t clen = sizeof(caddr);
		int cfd = (int)::accept(listenfd, (sockaddr*)&caddr, &clen);
		if(cfd < 0){
			if(!running.load()) break;
			continue;
		}
		std::thread(&ControlServer::HandleConnection, this, cfd).detach();
	}
}

static bool ReadLine(int fd, std::string& out) {
	out.clear();
	char buf[1];
	while(true){
		int n = (int)::recv(fd, buf, 1, 0);
		if(n <= 0) return false;
		if(buf[0] == '\n') return true;
		if(buf[0] != '\r') out.push_back(buf[0]);
		if(out.size() > (1 << 20)) return false; // 1 MiB line cap
	}
}

static bool WriteAll(int fd, const std::string& s) {
	const char* p = s.data();
	size_t left = s.size();
	while(left){
		int n = (int)::send(fd, p, (int)left, 0);
		if(n <= 0) return false;
		p += n; left -= (size_t)n;
	}
	return true;
}

static std::string ReplyToLine(const ControlReply& r) {
	json j;
	j["id"] = r.id;
	j["ok"] = r.ok;
	if(r.ok){
		j["result"] = r.result;
	} else {
		j["error"] = r.error;
		j["code"] = r.code;
	}
	std::string s = j.dump();
	s.push_back('\n');
	return s;
}

void ControlServer::HandleConnection(int cfd) {
	std::string line;
	while(running.load() && ReadLine(cfd, line)){
		ControlCommand cmd;
		ControlReply rpl;
		try {
			json j = json::parse(line);
			cmd.id = j.value("id", 0);
			cmd.op = j.value("op", "");
			if(j.contains("args") && j["args"].is_object()){
				cmd.args = j["args"];
			}
		} catch(const std::exception& e) {
			rpl.id = 0;
			rpl.ok = false;
			rpl.code = "BAD_REQUEST";
			rpl.error = e.what();
			WriteAll(cfd, ReplyToLine(rpl));
			break;
		}
		cmd.phase = ControlDispatch::PhaseFor(cmd.op);
		auto promise = std::make_shared<std::promise<ControlReply>>();
		auto fut = promise->get_future();
		cmd.reply = promise;
		{
			std::lock_guard<std::mutex> lk(queue_mu);
			if(cmd.phase == ControlCommand::POST_RENDER){
				postrender.push_back(std::move(cmd));
			} else {
				immediate.push_back(std::move(cmd));
			}
		}
		ControlReply got = fut.get();
		if(!WriteAll(cfd, ReplyToLine(got))) break;
	}
	CLOSE_SOCK(cfd);
}

std::vector<ControlCommand> ControlServer::DrainImmediate() {
	std::lock_guard<std::mutex> lk(queue_mu);
	auto out = std::move(immediate);
	immediate.clear();
	return out;
}

std::vector<ControlCommand> ControlServer::DrainPostRender() {
	std::lock_guard<std::mutex> lk(queue_mu);
	auto out = std::move(postrender);
	postrender.clear();
	return out;
}
