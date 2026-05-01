#include "controlserver.h"
#include "controldispatch.h"
#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <deque>
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

bool ControlServer::Start(int p, std::function<void()> onShutdownDrain) {
	if(p <= 0){
		return false;
	}
	if(running.load()) return false;
	port = p;
	shutdownDrain = std::move(onShutdownDrain);
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

	// Force any handler blocked in recv() (inside ReadLine) to return. We mark
	// each entry -1 under the lock so the handler's own cleanup won't race-close
	// a recycled fd.
	{
		std::lock_guard<std::mutex> lk(conn_mu);
		for(int& fd : clientfds){
			if(fd >= 0){
				CLOSE_SOCK(fd);
				fd = -1;
			}
		}
	}

	// Drain queued commands and fulfill their promises so handlers blocked on
	// fut.get() can return.
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

	// Fulfill replies for commands held outside us (e.g. Game::pendingWaits).
	if(shutdownDrain) shutdownDrain();

	// Now safe to join: every handler can exit recv() (socket closed) or fut.get()
	// (promise fulfilled).
	std::vector<std::thread> threads_to_join;
	{
		std::lock_guard<std::mutex> lk(conn_mu);
		threads_to_join = std::move(connthreads);
		connthreads.clear();
		clientfds.clear();
	}
	for(auto& t : threads_to_join){
		if(t.joinable()) t.join();
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
		// Register cfd before launching the handler so Stop() will see it.
		// The handler removes its entry on exit.
		std::lock_guard<std::mutex> lk(conn_mu);
		clientfds.push_back(cfd);
		connthreads.emplace_back(&ControlServer::HandleConnection, this, cfd);
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
	// Reader/writer split: the reader thread (this one) parses lines, queues
	// commands, and pushes futures into `pending`. A per-connection writer
	// thread drains those futures in arrival order and emits replies as they
	// resolve. This eliminates head-of-line blocking — a slow MULTI_FRAME op
	// no longer blocks subsequent fast ops on the same connection. JS-side
	// ControlClient multiplexes replies by id, so wire ordering is irrelevant
	// to correctness even if futures resolve out of order.
	std::deque<std::shared_future<ControlReply>> pending;
	std::mutex pending_mu;
	std::condition_variable pending_cv;
	std::atomic<bool> reader_done{false};
	std::atomic<bool> write_failed{false};

	std::thread writer([&](){
		while(true){
			std::shared_future<ControlReply> fut;
			{
				std::unique_lock<std::mutex> lk(pending_mu);
				pending_cv.wait(lk, [&]{
					return write_failed.load() || !pending.empty() ||
					       (reader_done.load() && pending.empty());
				});
				if(write_failed.load()) return;
				if(pending.empty()){
					if(reader_done.load()) return;
					continue;
				}
				fut = pending.front();
				pending.pop_front();
			}
			ControlReply got = fut.get();
			if(!WriteAll(cfd, ReplyToLine(got))){
				write_failed.store(true);
				// Closing the socket nudges the reader's recv() out of its
				// blocking wait so it observes EOF and unwinds.
#ifdef _WIN32
				::shutdown(cfd, SD_BOTH);
#else
				::shutdown(cfd, SHUT_RDWR);
#endif
				return;
			}
		}
	});

	std::string line;
	while(running.load() && !write_failed.load() && ReadLine(cfd, line)){
		ControlCommand cmd;
		bool noreply = false;
		try {
			json j = json::parse(line);
			cmd.id = j.value("id", 0);
			cmd.op = j.value("op", "");
			if(j.contains("args") && j["args"].is_object()){
				cmd.args = j["args"];
			}
			noreply = j.value("noreply", false);
		} catch(const std::exception& e) {
			ControlReply rpl;
			rpl.id = 0;
			rpl.ok = false;
			rpl.code = "BAD_REQUEST";
			rpl.error = e.what();
			auto p = std::make_shared<std::promise<ControlReply>>();
			p->set_value(rpl);
			{
				std::lock_guard<std::mutex> lk(pending_mu);
				pending.push_back(p->get_future().share());
			}
			pending_cv.notify_one();
			break;
		}
		cmd.phase = ControlDispatch::PhaseFor(cmd.op);
		auto promise = std::make_shared<std::promise<ControlReply>>();
		auto fut = promise->get_future().share();
		cmd.reply = promise;
		// Queue for the dispatcher first so the future is guaranteed reachable
		// before the writer can pop it.
		{
			std::lock_guard<std::mutex> lk(queue_mu);
			if(cmd.phase == ControlCommand::POST_RENDER){
				postrender.push_back(std::move(cmd));
			} else {
				immediate.push_back(std::move(cmd));
			}
		}
		if(!noreply){
			std::lock_guard<std::mutex> lk(pending_mu);
			pending.push_back(fut);
			pending_cv.notify_one();
		}
		// noreply=true: future is fulfilled by the dispatcher and quietly
		// dropped. No wire chatter, no allocation pressure on the writer.
	}
	reader_done.store(true);
	pending_cv.notify_one();
	if(writer.joinable()) writer.join();

	// Take ownership of the close: Stop() may have already closed and -1'd our
	// entry; only close if the entry still names our fd.
	int my_cfd = -1;
	{
		std::lock_guard<std::mutex> lk(conn_mu);
		auto it = std::find(clientfds.begin(), clientfds.end(), cfd);
		if(it != clientfds.end()){
			my_cfd = *it;
			*it = -1;
		}
	}
	if(my_cfd >= 0) CLOSE_SOCK(my_cfd);
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
