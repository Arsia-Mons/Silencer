#ifndef CONTROLSERVER_H
#define CONTROLSERVER_H

#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "nlohmann/json.hpp"

class Game;

struct ControlReply {
	int id = 0;
	bool ok = false;
	nlohmann::json result = nlohmann::json::object();
	std::string error;
	std::string code; // empty when ok
};

struct ControlCommand {
	int id = 0;
	std::string op;
	nlohmann::json args = nlohmann::json::object();
	std::shared_ptr<std::promise<ControlReply>> reply;
	// "post-render" (screenshot) and "wait" commands tag themselves so the
	// dispatcher can route them through the right queue.
	enum Phase { IMMEDIATE, POST_RENDER, MULTI_FRAME } phase = IMMEDIATE;
};

class ControlServer {
public:
	ControlServer();
	~ControlServer();
	// Returns false if the listen socket can't bind. Logs and continues.
	bool Start(int port);
	void Stop();

	// Game-thread: pop all queued IMMEDIATE commands.
	std::vector<ControlCommand> DrainImmediate();
	// Game-thread: pop all queued POST_RENDER commands.
	std::vector<ControlCommand> DrainPostRender();

private:
	void AcceptLoop();
	void HandleConnection(int clientfd);

	int listenfd;
	int port;
	std::atomic<bool> running;
	std::thread acceptthread;

	std::mutex queue_mu;
	std::vector<ControlCommand> immediate;
	std::vector<ControlCommand> postrender;
};

#endif
