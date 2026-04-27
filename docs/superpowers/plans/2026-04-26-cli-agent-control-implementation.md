# CLI agent control — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the v1 control channel + `silencer-cli` wrapper specified in [`docs/superpowers/specs/2026-04-26-cli-agent-control-design.md`](../specs/2026-04-26-cli-agent-control-design.md), plus an agent skill and an end-to-end acceptance test.

**Architecture:** Long-running `silencer` exposes a JSON-lines TCP control channel on `127.0.0.1:<port>`. An accept thread parses one command per line, queues it (with a `std::promise<Reply>`) for the game thread, which drains the queue once per frame between input and tick. A stateless Bun + TS `silencer-cli` opens one socket per command. A `--headless` flag skips window/audio init for CI. JSON I/O uses the already-vendored `nlohmann/json`.

**Tech Stack:** C++14 / SDL3 / nlohmann/json (existing CMake dep) / stb_image_write (new vendor) / Bun + TypeScript / bash.

---

## Branching and worktree

All work happens in the existing worktree at `/Users/hv/.config/superpowers/worktrees/Silencer/hv-cli` on branch `hv/cli`. The branch already contains the spec commit `d31f982`. PR base is `main`.

## File structure

**New C++ files** (`clients/silencer/src/`):

- `controlserver.{h,cpp}` — accept thread, per-connection read/write loop, command queue, reply promises. Owns the listening `int sockfd` and the worker `std::thread` for accept + connection handlers. Pushes `ControlCommand` to `Game`.
- `controldispatch.{h,cpp}` — `op` → `Handler(Game&, const json&) -> Reply` table. Adding a new command is one entry plus one function. Game-thread only.

**No `controljson.{h,cpp}`** — the spec considered hand-rolling JSON, but `nlohmann/json` is already a CMake `FetchContent` dep (`clients/silencer/CMakeLists.txt:7-16`). Use it directly.

**New vendored header:** `clients/silencer/third_party/stb_image_write.h` (single-file, public-domain).

**Edited C++:**

- `main.cpp` — no edits. `Game::Load` is the existing arg-parsing site.
- `game.{h,cpp}` — add `controlserver`, `controlPort`, `headless`, `paused`, `stepFramesRemaining`, `stepWallclockDeadlineMs`, `pendingWaits` fields; add `DrainControlQueue()`, `PostFrameReplies()`, `StateName()`; gate `Tick`/`Present` on headless and paused state; integrate calls into `Loop`.
- `interface.{h,cpp}` — add `FindWidgetByLabel(World&, const char* label, Uint8 wantedType, Uint16* outId, ControlMatchError* err)`. Walks `objects` by ID, fetches each Object, dispatches by type to a label-extractor. Used by `click`/`set_text`/`select`.
- `renderer.{h,cpp}` — add `bool CapturePNG(const char* path, const SDL_Color* palette)` reading from `screenbuffer` indexed pixels through the palette, writing 24-bit RGB PNG via stb.
- `CMakeLists.txt` — add new sources (auto-globbed by `file(GLOB_RECURSE src/*.cpp)`), add `third_party/` to include dirs.

**New CLI component:** `clients/silencer-cli/` — `package.json`, `tsconfig.json`, `index.ts`, `CLAUDE.md`, `AGENTS.md` (symlink), `.gitignore`.

**New tests:** `tests/cli-agent/run.sh` + `tests/cli-agent/e2e/*.sh`. New doctest source `tests/control_widget_test.cpp` plus a wired-up `tests/CMakeLists.txt` entry.

**New skill:** `.claude/skills/using-silencer-cli/SKILL.md`.

**Updated docs:** `clients/silencer/CLAUDE.md`, `clients/cli/CLAUDE.md` (now points at the implemented `clients/silencer-cli/`).

---

## Acceptance gate

Before claiming done, the spec's acceptance test (spec line 217) must pass: a coding agent, with no human help, launches the daemon, navigates MAINMENU → OPTIONS → back, screenshots each, and PNGs render the expected screens. The E2E script in Task 27 codifies this.

---

## Task 1: Add `--control-port` and `--headless` flag plumbing

**Files:**
- Modify: `clients/silencer/src/game.h:114-172`
- Modify: `clients/silencer/src/game.cpp:122-218` (`Game::Load`)

- [ ] **Step 1: Add fields to `Game`**

In `clients/silencer/src/game.h` after the existing `replayfile` field, before `stage2spawned`:

```cpp
	// CLI agent control (v1).
	int controlPort;          // 0 = disabled
	bool headless;            // skip SDL_INIT_VIDEO + window
	bool paused;              // simulation gated; ticks resume during step budget
	int stepFramesRemaining;  // frames left in active step budget; -1 unbounded
	Uint64 stepWallclockDeadlineMs; // SDL_GetTicks deadline; 0 if no wallclock cap
```

- [ ] **Step 2: Initialise the new fields in the `Game` ctor**

Open `clients/silencer/src/game.cpp` and locate the `Game::Game()` constructor (search for `Game::Game(`). Add at the top of the body:

```cpp
	controlPort = 0;
	headless = false;
	paused = false;
	stepFramesRemaining = 0;
	stepWallclockDeadlineMs = 0;
```

- [ ] **Step 3: Parse the new flags in `Game::Load`**

In `Game::Load` (`clients/silencer/src/game.cpp:122-218`), inside the existing `do { ... } while((cmdline = strtok(0, " ")))` loop, after the `-r` branch and before the closing `}` of `do`, add:

```cpp
				else if(strncmp(cmdline, "--control-port", 14) == 0){
					char * portstr = strtok(NULL, " ");
					if(portstr){
						controlPort = atoi(portstr);
					}
				}
				else if(strcmp(cmdline, "--headless") == 0){
					headless = true;
				}
```

(The leading `else` chains onto the existing `if(strncmp(... "-s" ...))` / `if(strncmp(... "-r" ...))` blocks; the existing code uses standalone `if`s — preserve that style by using bare `if` if that's what you find at the site. Match the local style.)

- [ ] **Step 4: Build and verify flags parse**

Run from the worktree root:

```bash
cd clients/silencer && cmake -B build -DSILENCER_BUILD_TESTS=ON && cmake --build build -j
```

Expected: clean build. Then verify flag parsing doesn't crash:

```bash
./build/silencer --headless --control-port 0 &
PID=$!
sleep 2
kill $PID 2>/dev/null
wait $PID 2>/dev/null
echo "exit: $?"
```

Expected: process started and was killed cleanly (exit non-zero from SIGTERM is fine — we just want no crash). The window will still appear; headless gating is wired in Task 11.

- [ ] **Step 5: Commit**

```bash
git add clients/silencer/src/game.h clients/silencer/src/game.cpp
git commit -m "feat(client): parse --control-port and --headless flags"
```

---

## Task 2: ControlServer skeleton — accept thread, queue, no-op dispatch

**Files:**
- Create: `clients/silencer/src/controlserver.h`
- Create: `clients/silencer/src/controlserver.cpp`

- [ ] **Step 1: Write the header**

`clients/silencer/src/controlserver.h`:

```cpp
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
	std::vector<std::thread> connthreads;
	std::mutex connthreads_mu;

	std::mutex queue_mu;
	std::vector<ControlCommand> immediate;
	std::vector<ControlCommand> postrender;
};

#endif
```

- [ ] **Step 2: Write the implementation**

`clients/silencer/src/controlserver.cpp`:

```cpp
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
	{
		std::lock_guard<std::mutex> lk(connthreads_mu);
		for(auto& t : connthreads){
			if(t.joinable()) t.join();
		}
		connthreads.clear();
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
		std::lock_guard<std::mutex> lk(connthreads_mu);
		connthreads.emplace_back(&ControlServer::HandleConnection, this, cfd);
	}
}

static bool ReadLine(int fd, std::string& out) {
	out.clear();
	char buf[1];
	while(true){
		int n = (int)::recv(fd, buf, 1, 0);
		if(n <= 0) return !out.empty();
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
```

- [ ] **Step 3: Build and confirm it compiles**

```bash
cd clients/silencer && cmake --build build -j
```

Expected: clean build. (`controldispatch.h` is created in Task 3; until then, comment out the `#include "controldispatch.h"` and `cmd.phase = ControlDispatch::PhaseFor(...)` line and hardcode `cmd.phase = ControlCommand::IMMEDIATE` to keep the build green between tasks. **Restore both in Task 3.**)

- [ ] **Step 4: Commit**

```bash
git add clients/silencer/src/controlserver.h clients/silencer/src/controlserver.cpp
git commit -m "feat(client): control server accept thread + command queue"
```

---

## Task 3: ControlDispatch skeleton — registry with `ping` only

**Files:**
- Create: `clients/silencer/src/controldispatch.h`
- Create: `clients/silencer/src/controldispatch.cpp`
- Modify: `clients/silencer/src/controlserver.cpp` (uncomment dispatch include from Task 2)

- [ ] **Step 1: Write the header**

`clients/silencer/src/controldispatch.h`:

```cpp
#ifndef CONTROLDISPATCH_H
#define CONTROLDISPATCH_H

#include "controlserver.h"

class Game;

namespace ControlDispatch {
	// Determine which queue an op belongs in. Called from the accept thread,
	// so this must be pure (no Game/World access).
	ControlCommand::Phase PhaseFor(const std::string& op);

	// Game-thread: handle one command, fulfill its reply promise.
	void HandleImmediate(Game& game, ControlCommand& cmd);
	void HandlePostRender(Game& game, ControlCommand& cmd);
}

#endif
```

- [ ] **Step 2: Write the implementation with `ping` only**

`clients/silencer/src/controldispatch.cpp`:

```cpp
#include "controldispatch.h"
#include "game.h"
#include <cstring>

namespace ControlDispatch {

ControlCommand::Phase PhaseFor(const std::string& op) {
	if(op == "screenshot") return ControlCommand::POST_RENDER;
	if(op == "wait_frames" || op == "wait_ms" ||
	   op == "wait_for_state" || op == "step") return ControlCommand::MULTI_FRAME;
	return ControlCommand::IMMEDIATE;
}

static ControlReply OkResult(int id, nlohmann::json r){
	ControlReply rpl;
	rpl.id = id;
	rpl.ok = true;
	rpl.result = std::move(r);
	return rpl;
}

static ControlReply Err(int id, const char* code, const std::string& msg){
	ControlReply rpl;
	rpl.id = id;
	rpl.ok = false;
	rpl.code = code;
	rpl.error = msg;
	return rpl;
}

void HandleImmediate(Game& game, ControlCommand& cmd) {
	if(cmd.op == "ping"){
		nlohmann::json r;
		r["version"] = SILENCER_VERSION;
		r["build"] = __DATE__ " " __TIME__;
		r["frame"] = game.GetFrameCount();
		r["paused"] = game.paused;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
	cmd.reply->set_value(Err(cmd.id, "UNKNOWN_OP", "unknown op: " + cmd.op));
}

void HandlePostRender(Game& game, ControlCommand& cmd) {
	cmd.reply->set_value(Err(cmd.id, "UNKNOWN_OP", "unknown post-render op: " + cmd.op));
}

} // namespace ControlDispatch
```

- [ ] **Step 3: Expose what the dispatcher needs from `Game`**

In `clients/silencer/src/game.h`, change `private:` access for `paused` (it's already a field added in Task 1) by **moving the new control-related fields** into a new `public:` block above the existing `private:`:

```cpp
public:
	// Exposed for ControlDispatch (game-thread only).
	int GetFrameCount() const { return frames; }
	bool paused;
	int stepFramesRemaining;
	Uint64 stepWallclockDeadlineMs;
	int controlPort;
	bool headless;
```

(Remove these same field declarations from wherever Task 1 placed them so they're not duplicated.)

- [ ] **Step 4: Restore the include and phase routing in `controlserver.cpp`**

Re-add `#include "controldispatch.h"` at the top and change the phase line back to:

```cpp
		cmd.phase = ControlDispatch::PhaseFor(cmd.op);
```

- [ ] **Step 5: Build**

```bash
cd clients/silencer && cmake --build build -j
```

Expected: clean build. ControlServer and ControlDispatch link, but no Game-side wiring drains the queue yet — connections will hang. That's fixed in Task 4.

- [ ] **Step 6: Commit**

```bash
git add clients/silencer/src/controldispatch.h clients/silencer/src/controldispatch.cpp clients/silencer/src/controlserver.cpp clients/silencer/src/game.h
git commit -m "feat(client): control dispatch table with ping op"
```

---

## Task 4: Wire ControlServer into `Game::Load` and `Game::Loop`

**Files:**
- Modify: `clients/silencer/src/game.h`
- Modify: `clients/silencer/src/game.cpp` (`Game::Load`, `Game::Loop`)

- [ ] **Step 1: Add the server member and helper decls to `Game`**

In `clients/silencer/src/game.h`, near the other includes add:

```cpp
#include "controlserver.h"
```

Then in the `private:` section add:

```cpp
	ControlServer controlserver;
	void DrainControlQueue();
	void PostFrameReplies();
```

- [ ] **Step 2: Start the server when `--control-port` is set**

In `clients/silencer/src/game.cpp`, at the very end of `Game::Load` (just before `return true;`), add:

```cpp
	if(controlPort > 0){
		if(!controlserver.Start(controlPort)){
			fprintf(stderr, "[control] failed to start; continuing without\n");
		}
	}
```

- [ ] **Step 3: Implement `DrainControlQueue` and `PostFrameReplies`**

Append to `clients/silencer/src/game.cpp`:

```cpp
#include "controldispatch.h"

void Game::DrainControlQueue(){
	if(controlPort <= 0) return;
	auto cmds = controlserver.DrainImmediate();
	for(auto& c : cmds){
		ControlDispatch::HandleImmediate(*this, c);
	}
}

void Game::PostFrameReplies(){
	if(controlPort <= 0) return;
	auto cmds = controlserver.DrainPostRender();
	for(auto& c : cmds){
		ControlDispatch::HandlePostRender(*this, c);
	}
}
```

- [ ] **Step 4: Hook into `Game::Loop`**

Open `clients/silencer/src/game.cpp` and find `bool Game::Loop(void){` (~line 268). Right at the top, after `if(stage2spawned){ return false; }`, add:

```cpp
	DrainControlQueue();
```

Find `Present();` near the bottom of `Loop` (~line 364) and immediately after it add:

```cpp
		PostFrameReplies();
```

(That `Present();` is inside the `if(!world.dedicatedserver.active)` branch — that's the right place; control is irrelevant in dedicated mode.)

- [ ] **Step 5: Build and smoke-test `ping`**

```bash
cd clients/silencer && cmake --build build -j
./build/silencer --control-port 5170 &
PID=$!
sleep 2
printf '{"id":1,"op":"ping"}\n' | nc 127.0.0.1 5170
kill $PID 2>/dev/null
wait $PID 2>/dev/null
```

Expected: a JSON line printed like `{"id":1,"ok":true,"result":{"build":"...","frame":N,"paused":false,"version":"00028"}}`.

- [ ] **Step 6: Commit**

```bash
git add clients/silencer/src/game.h clients/silencer/src/game.cpp
git commit -m "feat(client): drain control queue per-frame from Game::Loop"
```

---

## Task 5: `state` op + `StateName` helper

**Files:**
- Modify: `clients/silencer/src/game.h` — add `static const char* StateName(Uint8 s)`.
- Modify: `clients/silencer/src/game.cpp` — implement `StateName`.
- Modify: `clients/silencer/src/controldispatch.cpp` — add `state` handler.

- [ ] **Step 1: Add `StateName` declaration**

In `clients/silencer/src/game.h`, in the `public:` section near `GetFrameCount`:

```cpp
	static const char* StateName(Uint8 s);
	Uint8 GetState() const { return state; }
	Uint16 GetCurrentInterfaceId() const { return currentinterface; }
```

- [ ] **Step 2: Implement `StateName`**

Append to `clients/silencer/src/game.cpp`:

```cpp
const char* Game::StateName(Uint8 s){
	switch(s){
		case NONE: return "NONE";
		case FADEOUT: return "FADEOUT";
		case MAINMENU: return "MAINMENU";
		case LOBBYCONNECT: return "LOBBYCONNECT";
		case LOBBY: return "LOBBY";
		case UPDATING: return "UPDATING";
		case INGAME: return "INGAME";
		case MISSIONSUMMARY: return "MISSIONSUMMARY";
		case SINGLEPLAYERGAME: return "SINGLEPLAYERGAME";
		case OPTIONS: return "OPTIONS";
		case OPTIONSCONTROLS: return "OPTIONSCONTROLS";
		case OPTIONSDISPLAY: return "OPTIONSDISPLAY";
		case OPTIONSAUDIO: return "OPTIONSAUDIO";
		case HOSTGAME: return "HOSTGAME";
		case JOINGAME: return "JOINGAME";
		case REPLAYGAME: return "REPLAYGAME";
		case TESTGAME: return "TESTGAME";
		default: return "UNKNOWN";
	}
}
```

- [ ] **Step 3: Add the `state` handler**

In `clients/silencer/src/controldispatch.cpp`, inside `HandleImmediate` before the `unknown op` fallback:

```cpp
	if(cmd.op == "state"){
		nlohmann::json r;
		r["state"] = Game::StateName(game.GetState());
		r["current_interface_id"] = game.GetCurrentInterfaceId();
		r["frame"] = game.GetFrameCount();
		r["paused"] = game.paused;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
```

- [ ] **Step 4: Build and verify**

```bash
cd clients/silencer && cmake --build build -j
./build/silencer --control-port 5170 &
PID=$!; sleep 3
printf '{"id":1,"op":"state"}\n' | nc 127.0.0.1 5170
kill $PID 2>/dev/null; wait $PID 2>/dev/null
```

Expected: a line containing `"state":"MAINMENU"` (the post-load default, after the FADEOUT transition).

- [ ] **Step 5: Commit**

```bash
git add clients/silencer/src/game.h clients/silencer/src/game.cpp clients/silencer/src/controldispatch.cpp
git commit -m "feat(client): state op + StateName helper"
```

---

## Task 6: `Interface::FindWidgetByLabel` + widget enumeration

**Files:**
- Modify: `clients/silencer/src/interface.h`
- Modify: `clients/silencer/src/interface.cpp`
- Create: `tests/control_widget_test.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Declare the helper**

In `clients/silencer/src/interface.h`, inside `class Interface : public Object`:

```cpp
public:
	enum WidgetMatch { MATCH_OK, MATCH_NOT_FOUND, MATCH_AMBIGUOUS };
	// Walks `objects`. Compares case-insensitively against widget label/text.
	// `wantedTypes` is a bitmask of (1 << ObjectTypes::BUTTON) etc; 0 = any.
	// Returns the matched object id in `*outId` on MATCH_OK.
	WidgetMatch FindWidgetByLabel(class World& world, const char* labelOrId,
		Uint32 wantedTypes, Uint16* outId) const;
```

(You'll see existing `public:` block at the top — append this declaration there. Do **not** introduce a second public section.)

- [ ] **Step 2: Implement the helper**

In `clients/silencer/src/interface.cpp`, append:

```cpp
#include "button.h"
#include "toggle.h"
#include "textbox.h"
#include "selectbox.h"
#include "objecttypes.h"
#include "world.h"
#include <cctype>
#include <cstdlib>
#include <cstring>

static bool IEq(const char* a, const char* b){
	if(!a || !b) return false;
	while(*a && *b){
		if(std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) return false;
		++a; ++b;
	}
	return *a == 0 && *b == 0;
}

static const char* LabelOf(Object* o){
	if(!o) return nullptr;
	switch(o->type){
		case ObjectTypes::BUTTON: return ((Button*)o)->text;
		case ObjectTypes::TOGGLE: return ((Toggle*)o)->text;
		default: return nullptr;
	}
}

Interface::WidgetMatch Interface::FindWidgetByLabel(World& world,
	const char* labelOrId, Uint32 wantedTypes, Uint16* outId) const {
	if(!labelOrId || !*labelOrId) return MATCH_NOT_FOUND;
	// Numeric ID path: caller passed a literal Uint16.
	char* endp = nullptr;
	long asnum = std::strtol(labelOrId, &endp, 10);
	if(endp && *endp == 0 && asnum > 0 && asnum <= 0xFFFF){
		Object* o = world.GetObjectFromId((Uint16)asnum);
		if(o && (wantedTypes == 0 || (wantedTypes & (1u << o->type)))){
			*outId = (Uint16)asnum;
			return MATCH_OK;
		}
		return MATCH_NOT_FOUND;
	}
	int hits = 0;
	Uint16 firstHit = 0;
	for(Uint16 oid : objects){
		Object* o = world.GetObjectFromId(oid);
		if(!o) continue;
		if(wantedTypes != 0 && !(wantedTypes & (1u << o->type))) continue;
		const char* label = LabelOf(o);
		if(label && IEq(label, labelOrId)){
			if(hits == 0) firstHit = oid;
			++hits;
		}
	}
	if(hits == 0) return MATCH_NOT_FOUND;
	if(hits > 1)  return MATCH_AMBIGUOUS;
	*outId = firstHit;
	return MATCH_OK;
}
```

- [ ] **Step 3: Write a doctest unit test**

`tests/control_widget_test.cpp`:

```cpp
#include "doctest.h"
// Pure-logic test for the label matcher. We test IEq via a small shim by
// duplicating the logic here — the helper itself is a static inside
// interface.cpp and not exported. If this drifts, fail fast.
#include <cctype>
static bool IEq(const char* a, const char* b){
	if(!a || !b) return false;
	while(*a && *b){
		if(std::tolower((unsigned char)*a) != std::tolower((unsigned char)*b)) return false;
		++a; ++b;
	}
	return *a == 0 && *b == 0;
}
TEST_CASE("widget label compare is case-insensitive"){
	CHECK(IEq("Connect", "connect"));
	CHECK(IEq("OPTIONS", "Options"));
	CHECK_FALSE(IEq("Options", "Optionz"));
	CHECK_FALSE(IEq("Options", "Options "));
}
```

- [ ] **Step 4: Wire the test source**

In `tests/CMakeLists.txt`, add `control_widget_test.cpp` to `TEST_SRC`:

```cmake
set(TEST_SRC
    smoke_test.cpp
    updater_sha256_test.cpp
    updater_download_test.cpp
    updater_zip_test.cpp
    updater_sm_test.cpp
    control_widget_test.cpp
    ${CMAKE_SOURCE_DIR}/src/updatersha256.cpp
    ${CMAKE_SOURCE_DIR}/src/updaterdownload.cpp
    ${CMAKE_SOURCE_DIR}/src/updaterzip.cpp
    ${CMAKE_SOURCE_DIR}/src/updater.cpp
)
```

- [ ] **Step 5: Run tests**

```bash
cd clients/silencer && cmake -B build -DSILENCER_BUILD_TESTS=ON && cmake --build build -j
./build/tests/silencer_tests --test-case='widget label compare is case-insensitive'
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add clients/silencer/src/interface.h clients/silencer/src/interface.cpp tests/control_widget_test.cpp tests/CMakeLists.txt
git commit -m "feat(client): Interface::FindWidgetByLabel + case-insensitive doctest"
```

---

## Task 7: `inspect` op (widget enumeration for current interface)

**Files:**
- Modify: `clients/silencer/src/controldispatch.cpp`

- [ ] **Step 1: Add the handler**

In `clients/silencer/src/controldispatch.cpp` add at the top:

```cpp
#include "interface.h"
#include "world.h"
#include "button.h"
#include "toggle.h"
#include "textbox.h"
#include "selectbox.h"
#include "objecttypes.h"
```

…and inside `HandleImmediate` before the unknown-op fallback:

```cpp
	if(cmd.op == "inspect"){
		Uint16 ifid = cmd.args.value("interface_id", 0);
		if(ifid == 0) ifid = game.GetCurrentInterfaceId();
		Interface* iface = (Interface*)game.GetWorld().GetObjectFromId(ifid);
		if(!iface || iface->type != ObjectTypes::INTERFACE){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE", "no current interface"));
			return;
		}
		nlohmann::json widgets = nlohmann::json::array();
		for(Uint16 oid : iface->objects){
			Object* o = game.GetWorld().GetObjectFromId(oid);
			if(!o) continue;
			nlohmann::json w;
			w["id"] = oid;
			w["x"] = o->x; w["y"] = o->y;
			switch(o->type){
				case ObjectTypes::BUTTON: {
					Button* b = (Button*)o;
					w["kind"] = "button";
					w["label"] = b->text;
					w["w"] = b->width; w["h"] = b->height;
					w["enabled"] = !iface->disabled;
					break;
				}
				case ObjectTypes::TOGGLE: {
					Toggle* t = (Toggle*)o;
					w["kind"] = "toggle";
					w["label"] = t->text;
					w["w"] = t->width; w["h"] = t->height;
					w["enabled"] = !iface->disabled;
					w["selected"] = t->selected;
					break;
				}
				case ObjectTypes::TEXTBOX: {
					TextBox* tb = (TextBox*)o;
					w["kind"] = "textbox";
					w["w"] = tb->width; w["h"] = tb->height;
					break;
				}
				case ObjectTypes::SELECTBOX: {
					SelectBox* sb = (SelectBox*)o;
					w["kind"] = "selectbox";
					w["w"] = sb->width; w["h"] = sb->height;
					w["selected_index"] = sb->selecteditem;
					break;
				}
				default:
					w["kind"] = "other";
					w["object_type"] = o->type;
					break;
			}
			widgets.push_back(std::move(w));
		}
		nlohmann::json r;
		r["widgets"] = widgets;
		r["interface_id"] = ifid;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
```

- [ ] **Step 2: Expose `World&` accessor on Game**

In `clients/silencer/src/game.h`, in the public block:

```cpp
	class World& GetWorld() { return world; }
```

- [ ] **Step 3: Build and verify**

```bash
cd clients/silencer && cmake --build build -j
./build/silencer --control-port 5170 &
PID=$!; sleep 3
printf '{"id":1,"op":"inspect"}\n' | nc 127.0.0.1 5170 | head -c 800; echo
kill $PID 2>/dev/null; wait $PID 2>/dev/null
```

Expected: a JSON line whose `result.widgets` contains entries with `"kind":"button"` and labels matching the main menu (e.g. `"NEW GAME"`, `"OPTIONS"`).

- [ ] **Step 4: Commit**

```bash
git add clients/silencer/src/controldispatch.cpp clients/silencer/src/game.h
git commit -m "feat(client): inspect op enumerates widgets on current interface"
```

---

## Task 8: `world_state` op (best-effort summary)

**Files:**
- Modify: `clients/silencer/src/controldispatch.cpp`

- [ ] **Step 1: Add the handler**

In `clients/silencer/src/controldispatch.cpp`, add includes if not already present:

```cpp
#include "peer.h"
#include "player.h"
```

Inside `HandleImmediate`:

```cpp
	if(cmd.op == "world_state"){
		World& w = game.GetWorld();
		nlohmann::json r;
		r["map"] = w.gameinfo.mapname;
		r["peers"] = (int)w.peercount;
		nlohmann::json players = nlohmann::json::array();
		int objcount = 0;
		for(auto* o : w.objectlist){
			++objcount;
			if(o && o->type == ObjectTypes::PLAYER){
				Player* p = (Player*)o;
				nlohmann::json pj;
				pj["id"] = p->id;
				pj["name"] = p->name;
				pj["agency"] = p->agency;
				pj["hp"] = p->hp;
				pj["x"] = p->x;
				pj["y"] = p->y;
				players.push_back(std::move(pj));
			}
		}
		r["players"] = players;
		r["objects_count"] = objcount;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
```

(If any field name above doesn't match the actual `Player`/`World` member after you read those headers — `Player::name`, `Player::agency`, `Player::hp` — adapt to the real names. `Player` derives from `Bipedal` → `Physical` → `Hittable` → `Object`; positional fields and HP live up the chain. Read `clients/silencer/src/player.h` to confirm.)

- [ ] **Step 2: Build and verify**

```bash
cd clients/silencer && cmake --build build -j
./build/silencer --control-port 5170 &
PID=$!; sleep 3
printf '{"id":1,"op":"world_state"}\n' | nc 127.0.0.1 5170
kill $PID 2>/dev/null; wait $PID 2>/dev/null
```

Expected: JSON with `"peers":0`, `"players":[]`, and a non-zero `objects_count` (UI objects exist on the main menu).

- [ ] **Step 3: Commit**

```bash
git add clients/silencer/src/controldispatch.cpp
git commit -m "feat(client): world_state op for best-effort summary"
```

---

## Task 9: `click` op

**Files:**
- Modify: `clients/silencer/src/controldispatch.cpp`

- [ ] **Step 1: Add the handler**

In `HandleImmediate`:

```cpp
	if(cmd.op == "click"){
		std::string target;
		if(cmd.args.contains("label")) target = cmd.args["label"].get<std::string>();
		else if(cmd.args.contains("id")) target = std::to_string(cmd.args["id"].get<int>());
		if(target.empty()){
			cmd.reply->set_value(Err(cmd.id, "BAD_REQUEST", "click needs label or id"));
			return;
		}
		Uint16 ifid = game.GetCurrentInterfaceId();
		Interface* iface = (Interface*)game.GetWorld().GetObjectFromId(ifid);
		if(!iface){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE", "no current interface"));
			return;
		}
		Uint32 mask = (1u << ObjectTypes::BUTTON) | (1u << ObjectTypes::TOGGLE);
		Uint16 wid = 0;
		auto m = iface->FindWidgetByLabel(game.GetWorld(), target.c_str(), mask, &wid);
		if(m == Interface::MATCH_NOT_FOUND){
			cmd.reply->set_value(Err(cmd.id, "WIDGET_NOT_FOUND",
				"no widget matches \"" + target + "\""));
			return;
		}
		if(m == Interface::MATCH_AMBIGUOUS){
			cmd.reply->set_value(Err(cmd.id, "WIDGET_AMBIGUOUS",
				"multiple widgets match \"" + target + "\""));
			return;
		}
		Object* o = game.GetWorld().GetObjectFromId(wid);
		if(o->type == ObjectTypes::BUTTON){
			Button* b = (Button*)o;
			b->Activate(); // existing behavior used by the main menu
			b->clicked = true;
		} else if(o->type == ObjectTypes::TOGGLE){
			Toggle* t = (Toggle*)o;
			t->selected = !t->selected;
		}
		nlohmann::json r;
		r["widget_id"] = wid;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
```

- [ ] **Step 2: Build and verify the click triggers a state change**

```bash
cd clients/silencer && cmake --build build -j
./build/silencer --control-port 5170 &
PID=$!; sleep 3
printf '{"id":1,"op":"click","args":{"label":"OPTIONS"}}\n' | nc 127.0.0.1 5170
sleep 1
printf '{"id":2,"op":"state"}\n' | nc 127.0.0.1 5170
kill $PID 2>/dev/null; wait $PID 2>/dev/null
```

Expected: first reply ok, second reply contains `"state":"OPTIONS"`. If the first works but state doesn't transition, walk `Game::ProcessMainMenuInterface` to confirm the button.clicked / button.state path used today by mouse handling — match exactly what the SDL click handler does.

- [ ] **Step 3: Commit**

```bash
git add clients/silencer/src/controldispatch.cpp
git commit -m "feat(client): click op with case-insensitive label match"
```

---

## Task 10: `set_text`, `select`, `back`

**Files:**
- Modify: `clients/silencer/src/controldispatch.cpp`

- [ ] **Step 1: Expose `Game::GoBack`**

`Game::GoBack` already exists at `clients/silencer/src/game.cpp:3934`. Move its declaration to `public:` in `game.h` (find the `bool GoBack(void);` line and move it).

- [ ] **Step 2: Add handlers for `set_text`, `select`, `back`**

```cpp
	if(cmd.op == "set_text"){
		std::string target = cmd.args.value("label", std::string());
		std::string text   = cmd.args.value("text", std::string());
		Uint16 ifid = game.GetCurrentInterfaceId();
		Interface* iface = (Interface*)game.GetWorld().GetObjectFromId(ifid);
		if(!iface){ cmd.reply->set_value(Err(cmd.id, "WRONG_STATE", "no interface")); return; }
		Uint32 mask = (1u << ObjectTypes::TEXTBOX);
		Uint16 wid = 0;
		auto m = iface->FindWidgetByLabel(game.GetWorld(), target.c_str(), mask, &wid);
		if(m != Interface::MATCH_OK){
			cmd.reply->set_value(Err(cmd.id, m == Interface::MATCH_NOT_FOUND
				? "WIDGET_NOT_FOUND" : "WIDGET_AMBIGUOUS", target));
			return;
		}
		TextBox* tb = (TextBox*)game.GetWorld().GetObjectFromId(wid);
		tb->text.clear();
		tb->AddText(text.c_str());
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}

	if(cmd.op == "select"){
		std::string target = cmd.args.value("label", std::string());
		int idx = cmd.args.value("index", -1);
		std::string text = cmd.args.value("text", std::string());
		Uint16 ifid = game.GetCurrentInterfaceId();
		Interface* iface = (Interface*)game.GetWorld().GetObjectFromId(ifid);
		if(!iface){ cmd.reply->set_value(Err(cmd.id, "WRONG_STATE", "no interface")); return; }
		Uint32 mask = (1u << ObjectTypes::SELECTBOX);
		Uint16 wid = 0;
		auto m = iface->FindWidgetByLabel(game.GetWorld(), target.c_str(), mask, &wid);
		if(m != Interface::MATCH_OK){
			cmd.reply->set_value(Err(cmd.id, "WIDGET_NOT_FOUND", target));
			return;
		}
		SelectBox* sb = (SelectBox*)game.GetWorld().GetObjectFromId(wid);
		if(idx < 0 && !text.empty()){
			for(unsigned int i = 0; i < sb->items.size(); ++i){
				if(sb->items[i] && IEq(sb->items[i], text.c_str())){ idx = (int)i; break; }
			}
		}
		if(idx < 0 || (unsigned)idx >= sb->items.size()){
			cmd.reply->set_value(Err(cmd.id, "WIDGET_NOT_FOUND", "no such item"));
			return;
		}
		sb->selecteditem = idx;
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}

	if(cmd.op == "back"){
		bool went = game.GoBack();
		nlohmann::json r; r["went_back"] = went;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
```

(`IEq` is the static helper from `interface.cpp`; copy it locally in `controldispatch.cpp` as a static, since it's not exported.)

- [ ] **Step 3: Build and verify `back`**

```bash
cd clients/silencer && cmake --build build -j
./build/silencer --control-port 5170 &
PID=$!; sleep 3
printf '{"id":1,"op":"click","args":{"label":"OPTIONS"}}\n' | nc 127.0.0.1 5170
sleep 1
printf '{"id":2,"op":"back"}\n' | nc 127.0.0.1 5170
sleep 1
printf '{"id":3,"op":"state"}\n' | nc 127.0.0.1 5170
kill $PID 2>/dev/null; wait $PID 2>/dev/null
```

Expected: third reply has `"state":"MAINMENU"`.

- [ ] **Step 4: Commit**

```bash
git add clients/silencer/src/controldispatch.cpp clients/silencer/src/game.h
git commit -m "feat(client): set_text, select, back ops"
```

---

## Task 11: Vendor `stb_image_write.h` and add `Renderer::CapturePNG`

**Files:**
- Create: `clients/silencer/third_party/stb_image_write.h`
- Modify: `clients/silencer/CMakeLists.txt`
- Modify: `clients/silencer/src/renderer.h`
- Modify: `clients/silencer/src/renderer.cpp`

- [ ] **Step 1: Vendor stb_image_write**

Pin to a known commit. Run from the worktree root:

```bash
mkdir -p clients/silencer/third_party
curl -fsSL "https://raw.githubusercontent.com/nothings/stb/5736b15f7ea0ffb08dd38af21067c314d6a3aae9/stb_image_write.h" \
  -o clients/silencer/third_party/stb_image_write.h
sha256sum clients/silencer/third_party/stb_image_write.h
```

Record the sha in the commit message. The file is single-header public domain (~1700 lines).

- [ ] **Step 2: Add include path**

In `clients/silencer/CMakeLists.txt` after `target_include_directories(${SILENCER_TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated")`, append:

```cmake
target_include_directories(${SILENCER_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
```

- [ ] **Step 3: Add `Renderer::CapturePNG` declaration**

In `clients/silencer/src/renderer.h`, in the public section of `class Renderer`:

```cpp
	bool CapturePNG(const class Surface & buf, const SDL_Color * palette, const char * path);
```

- [ ] **Step 4: Implement `CapturePNG`**

In `clients/silencer/src/renderer.cpp`, at the very top:

```cpp
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
```

Append at the bottom of the file:

```cpp
bool Renderer::CapturePNG(const Surface& buf, const SDL_Color* palette, const char* path){
	std::vector<unsigned char> rgb(buf.w * buf.h * 3);
	for(int i = 0; i < buf.w * buf.h; ++i){
		Uint8 idx = buf.pixels[i];
		rgb[i*3+0] = palette[idx].r;
		rgb[i*3+1] = palette[idx].g;
		rgb[i*3+2] = palette[idx].b;
	}
	int rc = stbi_write_png(path, buf.w, buf.h, 3, rgb.data(), buf.w * 3);
	return rc != 0;
}
```

- [ ] **Step 5: Build**

```bash
cd clients/silencer && cmake --build build -j
```

Expected: clean build.

- [ ] **Step 6: Commit**

```bash
git add clients/silencer/third_party/stb_image_write.h clients/silencer/CMakeLists.txt clients/silencer/src/renderer.h clients/silencer/src/renderer.cpp
git commit -m "feat(client): vendor stb_image_write + Renderer::CapturePNG"
```

---

## Task 12: `screenshot` op (post-render)

**Files:**
- Modify: `clients/silencer/src/controldispatch.cpp`
- Modify: `clients/silencer/src/game.h` (expose `screenbuffer` and palette)

- [ ] **Step 1: Expose what dispatch needs**

In `clients/silencer/src/game.h`, public:

```cpp
	const Surface& GetScreenBuffer() const { return screenbuffer; }
	const SDL_Color* GetPaletteColors() const { return palettecolors; }
	Renderer& GetRenderer() { return renderer; }
```

- [ ] **Step 2: Add the handler**

In `controldispatch.cpp`, add:

```cpp
#include <cstdio>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir((p), 0755)
#endif
```

…and inside `HandlePostRender`:

```cpp
	if(cmd.op == "screenshot"){
		std::string out = cmd.args.value("out", std::string());
		if(out.empty()){
			char buf[256];
		#ifdef _WIN32
			const char* tmp = getenv("TEMP"); if(!tmp) tmp = ".";
			snprintf(buf, sizeof(buf), "%s\\silencer-%d.png", tmp, game.GetFrameCount());
		#else
			snprintf(buf, sizeof(buf), "/tmp/silencer-%d.png", game.GetFrameCount());
		#endif
			out = buf;
		}
		bool ok = game.GetRenderer().CapturePNG(game.GetScreenBuffer(),
			game.GetPaletteColors(), out.c_str());
		if(!ok){
			cmd.reply->set_value(Err(cmd.id, "INTERNAL", "stbi_write_png failed: " + out));
			return;
		}
		nlohmann::json r; r["path"] = out;
		cmd.reply->set_value(OkResult(cmd.id, r));
		return;
	}
```

- [ ] **Step 3: Build and verify**

```bash
cd clients/silencer && cmake --build build -j
./build/silencer --control-port 5170 &
PID=$!; sleep 3
rm -f /tmp/silencer-shot.png
printf '{"id":1,"op":"screenshot","args":{"out":"/tmp/silencer-shot.png"}}\n' | nc 127.0.0.1 5170
ls -l /tmp/silencer-shot.png
kill $PID 2>/dev/null; wait $PID 2>/dev/null
```

Expected: PNG exists, non-zero size, opens in Preview/eog.

- [ ] **Step 4: Commit**

```bash
git add clients/silencer/src/controldispatch.cpp clients/silencer/src/game.h
git commit -m "feat(client): screenshot op (post-render PNG capture)"
```

---

## Task 13: `quit` op

**Files:**
- Modify: `clients/silencer/src/controldispatch.cpp`
- Modify: `clients/silencer/src/game.h` (expose a quit flag)
- Modify: `clients/silencer/src/game.cpp` — `Loop` returns false when quit flag set.

- [ ] **Step 1: Add quit flag**

In `game.h` public section:

```cpp
	bool quitRequested = false;
```

In `Game::Loop`, at the very top after the existing `stage2spawned` check:

```cpp
	if(quitRequested) return false;
```

- [ ] **Step 2: Add handler**

In `controldispatch.cpp` `HandleImmediate`:

```cpp
	if(cmd.op == "quit"){
		game.quitRequested = true;
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
```

- [ ] **Step 3: Build and verify**

```bash
cd clients/silencer && cmake --build build -j
./build/silencer --control-port 5170 &
PID=$!; sleep 3
printf '{"id":1,"op":"quit"}\n' | nc 127.0.0.1 5170
wait $PID
echo "exit: $?"
```

Expected: process exited cleanly (rc 0).

- [ ] **Step 4: Commit**

```bash
git add clients/silencer/src/controldispatch.cpp clients/silencer/src/game.h clients/silencer/src/game.cpp
git commit -m "feat(client): quit op for clean shutdown"
```

---

## Task 14: `pause`, `resume`, multiplayer guard

**Files:**
- Modify: `clients/silencer/src/controldispatch.cpp`
- Modify: `clients/silencer/src/game.cpp` — gate `Tick` body on `paused` w/ step budget.

- [ ] **Step 1: Add WRONG_STATE check helper**

In `controldispatch.cpp`:

```cpp
static bool InMultiplayerLive(Game& game){
	// Authority connected with peers, or peer connected to authority.
	World& w = game.GetWorld();
	return (w.peercount > 1) && (w.gameplaystate == World::INGAME);
}
```

- [ ] **Step 2: Add `pause` and `resume` handlers**

```cpp
	if(cmd.op == "pause"){
		if(InMultiplayerLive(game)){
			cmd.reply->set_value(Err(cmd.id, "WRONG_STATE", "pause not supported in live multiplayer"));
			return;
		}
		game.paused = true;
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
	if(cmd.op == "resume"){
		game.paused = false;
		game.stepFramesRemaining = 0;
		game.stepWallclockDeadlineMs = 0;
		cmd.reply->set_value(OkResult(cmd.id, nlohmann::json::object()));
		return;
	}
```

- [ ] **Step 3: Gate the simulation step**

In `clients/silencer/src/game.cpp`, find the `while(lasttick <= tickcheck && tickcheck - lasttick > wait){` loop in `Game::Loop`. Right inside the loop body, before the existing `world.DoNetwork(); UpdateInputState(...)` calls, add:

```cpp
		if(paused){
			bool budgetFrames = stepFramesRemaining > 0 || stepFramesRemaining < 0;
			bool budgetMs = stepWallclockDeadlineMs > 0 && SDL_GetTicks() < stepWallclockDeadlineMs;
			if(!budgetFrames && !budgetMs){
				lasttick = tickcheck; // freeze the catch-up clock
				break;
			}
			if(stepFramesRemaining > 0) --stepFramesRemaining;
			if(stepWallclockDeadlineMs > 0 && SDL_GetTicks() >= stepWallclockDeadlineMs){
				stepWallclockDeadlineMs = 0;
			}
		}
```

This freezes the simulation while still presenting frames (so screenshots still work) and lets `step` consume the budget.

- [ ] **Step 4: Build and verify**

```bash
cd clients/silencer && cmake --build build -j
./build/silencer --control-port 5170 &
PID=$!; sleep 3
printf '{"id":1,"op":"pause"}\n'  | nc 127.0.0.1 5170
printf '{"id":2,"op":"ping"}\n'   | nc 127.0.0.1 5170
sleep 1
printf '{"id":3,"op":"ping"}\n'   | nc 127.0.0.1 5170
printf '{"id":4,"op":"resume"}\n' | nc 127.0.0.1 5170
kill $PID 2>/dev/null; wait $PID 2>/dev/null
```

Expected: id 2 and id 3 frame counters are equal or differ by very little (sim clock frozen). Cosmetic frames (`Present` rate) may still tick — that's fine.

- [ ] **Step 5: Commit**

```bash
git add clients/silencer/src/controldispatch.cpp clients/silencer/src/game.cpp
git commit -m "feat(client): pause/resume with multiplayer guard"
```

---

## Task 15: Multi-frame waits (`wait_frames`, `wait_ms`, `wait_for_state`, `step`)

**Files:**
- Modify: `clients/silencer/src/controlserver.h` — add `pendingWaits` plumbing.
- Modify: `clients/silencer/src/controlserver.cpp`
- Modify: `clients/silencer/src/controldispatch.cpp`
- Modify: `clients/silencer/src/game.cpp` (`DrainControlQueue` + tick wakeups)

- [ ] **Step 1: Move pending-wait list onto `Game`**

In `clients/silencer/src/game.h` public:

```cpp
	struct PendingWait {
		ControlCommand cmd;
		Uint64 deadline_ms = 0;   // 0 = no wallclock deadline
		int frames_left = -1;     // <0 = no frame deadline
		std::string wait_state;   // for wait_for_state
	};
	std::vector<PendingWait> pendingWaits;
```

(Forward-declare `ControlCommand` via `#include "controlserver.h"` already in `game.h`.)

- [ ] **Step 2: Route MULTI_FRAME ops in `DrainControlQueue`**

Replace the `DrainControlQueue` body with:

```cpp
void Game::DrainControlQueue(){
	if(controlPort <= 0) return;
	auto cmds = controlserver.DrainImmediate();
	for(auto& c : cmds){
		if(c.phase == ControlCommand::MULTI_FRAME){
			ControlDispatch::EnqueueWait(*this, std::move(c));
		} else {
			ControlDispatch::HandleImmediate(*this, c);
		}
	}
	ControlDispatch::TickWaits(*this);
}
```

- [ ] **Step 3: Implement `EnqueueWait` and `TickWaits`**

In `controldispatch.h`:

```cpp
	void EnqueueWait(Game& game, ControlCommand cmd);
	void TickWaits(Game& game);
```

In `controldispatch.cpp`:

```cpp
void EnqueueWait(Game& game, ControlCommand cmd){
	Game::PendingWait w;
	w.cmd = std::move(cmd);
	if(w.cmd.op == "wait_frames"){
		w.frames_left = w.cmd.args.value("n", 1);
	} else if(w.cmd.op == "wait_ms"){
		int ms = w.cmd.args.value("n", 0);
		w.deadline_ms = SDL_GetTicks() + (Uint64)ms;
	} else if(w.cmd.op == "wait_for_state"){
		w.wait_state = w.cmd.args.value("state", std::string());
		int t = w.cmd.args.value("timeout_ms", 5000);
		w.deadline_ms = SDL_GetTicks() + (Uint64)t;
	} else if(w.cmd.op == "step"){
		int frames = w.cmd.args.value("frames", 0);
		int ms     = w.cmd.args.value("ms", 0);
		if(frames > 0){
			game.stepFramesRemaining = frames;
			w.frames_left = frames;
		} else if(ms > 0){
			game.stepWallclockDeadlineMs = SDL_GetTicks() + (Uint64)ms;
			w.deadline_ms = game.stepWallclockDeadlineMs;
		} else {
			w.cmd.reply->set_value(Err(w.cmd.id, "BAD_REQUEST", "step needs frames>0 or ms>0"));
			return;
		}
		// step assumes the caller wanted the sim to advance and re-pause.
		game.paused = true;
	}
	game.pendingWaits.push_back(std::move(w));
}

void TickWaits(Game& game){
	Uint64 now = SDL_GetTicks();
	auto& v = game.pendingWaits;
	for(auto it = v.begin(); it != v.end();){
		bool done = false;
		auto& w = *it;
		if(w.cmd.op == "wait_frames" || w.cmd.op == "step"){
			if(w.frames_left > 0) --w.frames_left;
			if(w.frames_left == 0) done = true;
			if(w.deadline_ms > 0 && now >= w.deadline_ms) done = true;
		} else if(w.cmd.op == "wait_ms"){
			if(now >= w.deadline_ms) done = true;
		} else if(w.cmd.op == "wait_for_state"){
			if(w.wait_state == Game::StateName(game.GetState())){
				w.cmd.reply->set_value(OkResult(w.cmd.id, nlohmann::json::object()));
				it = v.erase(it); continue;
			}
			if(now >= w.deadline_ms){
				w.cmd.reply->set_value(Err(w.cmd.id, "TIMEOUT",
					"state did not become " + w.wait_state));
				it = v.erase(it); continue;
			}
		}
		if(done){
			if(w.cmd.op == "step"){
				game.paused = true;  // step span ended; re-pause
				game.stepFramesRemaining = 0;
				game.stepWallclockDeadlineMs = 0;
			}
			w.cmd.reply->set_value(OkResult(w.cmd.id, nlohmann::json::object()));
			it = v.erase(it);
		} else {
			++it;
		}
	}
}
```

(Add `#include <SDL3/SDL.h>` near the top of `controldispatch.cpp` if `SDL_GetTicks` isn't already pulled in.)

- [ ] **Step 4: Build and verify**

```bash
cd clients/silencer && cmake --build build -j
./build/silencer --control-port 5170 &
PID=$!; sleep 3
START=$(date +%s%N)
printf '{"id":1,"op":"wait_ms","args":{"n":300}}\n' | nc 127.0.0.1 5170
END=$(date +%s%N)
echo "elapsed (ns): $((END-START))"
kill $PID 2>/dev/null; wait $PID 2>/dev/null
```

Expected: elapsed ≥ 300_000_000 ns (≥ 300 ms). State wait scenarios are exercised in the E2E suite.

- [ ] **Step 5: Commit**

```bash
git add clients/silencer/src/game.h clients/silencer/src/game.cpp clients/silencer/src/controldispatch.h clients/silencer/src/controldispatch.cpp
git commit -m "feat(client): multi-frame waits + step budget"
```

---

## Task 16: `--headless` mode

**Files:**
- Modify: `clients/silencer/src/game.cpp`

- [ ] **Step 1: Skip window/audio when `headless`**

In `Game::Load`, locate the `if(!world.dedicatedserver.active)` block (~line 181) that calls `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)`. Wrap the contents like so:

```cpp
	if(!world.dedicatedserver.active){
		Uint32 flags = headless ? 0 : (SDL_INIT_VIDEO | SDL_INIT_AUDIO);
		if(!SDL_Init(flags)){
			printf("Could not initialize SDL %s\n", SDL_GetError());
			return false;
		}
		if(!headless){
			if(!MIX_Init()){ /* existing body */ }
			// existing audio + window + render-device init below — gated on !headless
		}
	}
```

(Read the full original block before editing — the existing init has many statements; gate everything from `MIX_Init()` through `SetColors(...)` on `!headless`. The palette load in particular *must* still happen because resources rely on it; keep `renderer.palette.SetPalette(0)` inside `!headless` for now since it's tied to assets the renderer owns. Confirm at build time which subset is mandatory.)

- [ ] **Step 2: Skip render-device path in `Present`/`Loop`**

In `Game::Present`, the existing `if(renderdevice)` guard already covers the headless-no-device case — no change needed.

In `Game::Loop`, find the `if(!world.dedicatedserver.active){ ... Present(); }` branch. Surround the `Present()` call with:

```cpp
		if(!headless) Present();
```

This means screenshots still work (they read from `screenbuffer`, which `renderer.Draw` populates regardless of presentation).

- [ ] **Step 3: Build and verify**

```bash
cd clients/silencer && cmake --build build -j
./build/silencer --headless --control-port 5170 &
PID=$!; sleep 3
printf '{"id":1,"op":"state"}\n' | nc 127.0.0.1 5170
kill $PID 2>/dev/null; wait $PID 2>/dev/null
```

Expected: no window appears. State reply still ok.

- [ ] **Step 4: Verify screenshot still works headless**

```bash
./build/silencer --headless --control-port 5170 &
PID=$!; sleep 3
rm -f /tmp/silencer-headless.png
printf '{"id":1,"op":"screenshot","args":{"out":"/tmp/silencer-headless.png"}}\n' | nc 127.0.0.1 5170
ls -l /tmp/silencer-headless.png
kill $PID 2>/dev/null; wait $PID 2>/dev/null
```

Expected: PNG written, non-zero size. (Whether it shows real menu graphics depends on whether the renderer ran without a render device — note any caveat in the resulting commit message and the skill doc.)

- [ ] **Step 5: Commit**

```bash
git add clients/silencer/src/game.cpp
git commit -m "feat(client): --headless skips window/audio init"
```

---

## Task 17: `silencer-cli` Bun + TS wrapper — scaffolding

**Files:**
- Create: `clients/silencer-cli/package.json`
- Create: `clients/silencer-cli/tsconfig.json`
- Create: `clients/silencer-cli/index.ts`
- Create: `clients/silencer-cli/CLAUDE.md`
- Create: `clients/silencer-cli/AGENTS.md` (symlink to `CLAUDE.md`)
- Create: `clients/silencer-cli/.gitignore`

- [ ] **Step 1: package.json**

```json
{
  "name": "silencer-cli",
  "private": true,
  "type": "module",
  "bin": {
    "silencer-cli": "./index.ts"
  },
  "scripts": {
    "fmt": "oxfmt --write ."
  },
  "devDependencies": {
    "typescript": "^5.4.0"
  }
}
```

- [ ] **Step 2: tsconfig.json**

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "ESNext",
    "moduleResolution": "bundler",
    "strict": true,
    "esModuleInterop": true,
    "skipLibCheck": true,
    "types": ["bun"]
  },
  "include": ["index.ts"]
}
```

- [ ] **Step 3: .gitignore**

```
node_modules/
bun.lockb
```

- [ ] **Step 4: index.ts**

```ts
#!/usr/bin/env bun

import { connect } from "node:net";

type Reply = {
  id: number;
  ok: boolean;
  result?: unknown;
  error?: string;
  code?: string;
};

function usage(): never {
  console.error(
    `usage: silencer-cli [--host H] [--port P] <op> [--key value ...]\n` +
      `       silencer-cli ping\n` +
      `       silencer-cli state\n` +
      `       silencer-cli inspect [--interface-id N]\n` +
      `       silencer-cli world_state\n` +
      `       silencer-cli click --label "OPTIONS"\n` +
      `       silencer-cli set_text --label TEXT_ID --text "hi"\n` +
      `       silencer-cli select --label LISTBOX --index 0\n` +
      `       silencer-cli back\n` +
      `       silencer-cli screenshot [--out /path/x.png]\n` +
      `       silencer-cli wait_for_state --state OPTIONS [--timeout-ms 5000]\n` +
      `       silencer-cli wait_frames --n 30\n` +
      `       silencer-cli wait_ms --n 500\n` +
      `       silencer-cli pause | resume\n` +
      `       silencer-cli step --frames 10 | --ms 200\n` +
      `       silencer-cli quit\n` +
      `\n` +
      `Env: SILENCER_CONTROL_HOST (default 127.0.0.1)\n` +
      `     SILENCER_CONTROL_PORT (default 5170)`,
  );
  process.exit(2);
}

function parseArgs(argv: string[]): { host: string; port: number; op: string; args: Record<string, unknown> } {
  let host = process.env.SILENCER_CONTROL_HOST ?? "127.0.0.1";
  let port = Number.parseInt(process.env.SILENCER_CONTROL_PORT ?? "5170", 10);
  const args: Record<string, unknown> = {};
  let op: string | null = null;
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i]!;
    if (a === "--host") {
      host = argv[++i] ?? usage();
    } else if (a === "--port") {
      port = Number.parseInt(argv[++i] ?? usage(), 10);
    } else if (a.startsWith("--")) {
      const key = a.slice(2).replace(/-/g, "_");
      const next = argv[i + 1];
      if (next === undefined || next.startsWith("--")) {
        args[key] = true;
      } else {
        const num = Number(next);
        args[key] = Number.isFinite(num) && next.match(/^-?\d+(\.\d+)?$/) ? num : next;
        i++;
      }
    } else if (op === null) {
      op = a;
    } else {
      // positional after op → treat as label/text shorthand for click/set_text/select
      if (op === "click" && args["label"] === undefined) args["label"] = a;
      else if ((op === "set_text" || op === "select") && args["label"] === undefined) args["label"] = a;
      else if (op === "set_text" && args["text"] === undefined) args["text"] = a;
      else if (op === "select" && args["index"] === undefined) {
        const num = Number(a);
        args[Number.isInteger(num) ? "index" : "text"] = Number.isInteger(num) ? num : a;
      }
    }
  }
  if (!op) usage();
  return { host, port, op, args };
}

async function main() {
  const { host, port, op, args } = parseArgs(process.argv.slice(2));
  const id = Math.floor(Math.random() * 1_000_000) + 1;
  const payload = JSON.stringify({ id, op, args }) + "\n";

  const sock = connect({ host, port });
  await new Promise<void>((res, rej) => {
    sock.once("connect", () => res());
    sock.once("error", rej);
  });
  sock.write(payload);

  let buf = "";
  for await (const chunk of sock as AsyncIterable<Buffer>) {
    buf += chunk.toString("utf8");
    const nl = buf.indexOf("\n");
    if (nl >= 0) {
      const line = buf.slice(0, nl);
      sock.end();
      const reply = JSON.parse(line) as Reply;
      if (reply.ok) {
        process.stdout.write(JSON.stringify(reply.result ?? {}) + "\n");
        process.exit(0);
      } else {
        process.stderr.write(`[${reply.code ?? "ERR"}] ${reply.error ?? ""}\n`);
        process.exit(1);
      }
    }
  }
  process.stderr.write("[TRANSPORT] connection closed without reply\n");
  process.exit(2);
}

main().catch((e) => {
  process.stderr.write(`[TRANSPORT] ${e instanceof Error ? e.message : String(e)}\n`);
  process.exit(2);
});
```

- [ ] **Step 5: Make it executable**

```bash
chmod +x clients/silencer-cli/index.ts
```

- [ ] **Step 6: CLAUDE.md**

`clients/silencer-cli/CLAUDE.md`:

```markdown
# clients/silencer-cli — agent control wrapper

Stateless Bun + TypeScript CLI that talks JSON-lines TCP to a running
`silencer` started with `--control-port <P>`. One command per
invocation; the game stays up across many calls.

## Run

```bash
bun ./index.ts ping
bun ./index.ts --port 5170 click --label OPTIONS
bun ./index.ts wait_for_state --state OPTIONS --timeout-ms 3000
bun ./index.ts screenshot --out /tmp/x.png
```

Or install into PATH locally:

```bash
bun link
silencer-cli ping
```

## Env

- `SILENCER_CONTROL_HOST` (default `127.0.0.1`)
- `SILENCER_CONTROL_PORT` (default `5170`)

## Exit codes

- `0` — `ok:true`; result JSON to stdout.
- `1` — `ok:false`; `[CODE] error` to stderr.
- `2` — transport failure (connect refused / closed prematurely / etc).

## Wire protocol

See `docs/superpowers/specs/2026-04-26-cli-agent-control-design.md`.
```

- [ ] **Step 7: AGENTS.md symlink**

```bash
cd clients/silencer-cli && ln -s CLAUDE.md AGENTS.md && cd -
```

(Windows fallback per repo convention is a one-line stub — but this repo's other components use symlinks; match.)

- [ ] **Step 8: Smoke test**

```bash
./build/silencer --headless --control-port 5170 &
PID=$!; sleep 3
bun clients/silencer-cli/index.ts ping
kill $PID 2>/dev/null; wait $PID 2>/dev/null
```

Expected: stdout has the ping result JSON, exit 0.

- [ ] **Step 9: Commit**

```bash
git add clients/silencer-cli/
git commit -m "feat(silencer-cli): Bun+TS wrapper for the control channel"
```

---

## Task 18: E2E test harness

**Files:**
- Create: `tests/cli-agent/run.sh`
- Create: `tests/cli-agent/e2e/00_ping.sh`
- Create: `tests/cli-agent/e2e/10_navigate.sh`
- Create: `tests/cli-agent/e2e/20_screenshot.sh`
- Create: `tests/cli-agent/e2e/lib.sh`

- [ ] **Step 1: lib.sh**

```bash
#!/usr/bin/env bash
# Sourced by every E2E scenario.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
SILENCER_BIN="${SILENCER_BIN:-$REPO_ROOT/clients/silencer/build/silencer}"
CLI="bun $REPO_ROOT/clients/silencer-cli/index.ts"

pick_port() {
  # Random ephemeral.
  python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1]);s.close()'
}

start_silencer() {
  local port="$1"
  "$SILENCER_BIN" --headless --control-port "$port" >"/tmp/silencer-e2e-$port.log" 2>&1 &
  echo $!
}

wait_alive() {
  local port="$1"
  for i in $(seq 1 60); do
    if $CLI --port "$port" ping >/dev/null 2>&1; then return 0; fi
    sleep 0.5
  done
  echo "silencer on $port never came up" >&2
  return 1
}

stop_silencer() {
  local pid="$1" port="${2:-}"
  if [ -n "$port" ]; then
    $CLI --port "$port" quit >/dev/null 2>&1 || true
  fi
  sleep 0.3
  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
}
```

- [ ] **Step 2: 00_ping.sh**

```bash
#!/usr/bin/env bash
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

OUT=$($CLI --port "$PORT" ping)
echo "$OUT" | grep -q '"version"'
echo "PASS 00_ping"
```

- [ ] **Step 3: 10_navigate.sh**

Implements the spec's acceptance scenario:

```bash
#!/usr/bin/env bash
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

# MAINMENU -> OPTIONS
$CLI --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 10000
$CLI --port "$PORT" click --label OPTIONS
$CLI --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000
# OPTIONS -> back -> MAINMENU
$CLI --port "$PORT" back
$CLI --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 5000
echo "PASS 10_navigate"
```

- [ ] **Step 4: 20_screenshot.sh**

```bash
#!/usr/bin/env bash
set -euo pipefail
. "$(dirname "$0")/lib.sh"

PORT=$(pick_port)
PID=$(start_silencer "$PORT")
trap "stop_silencer $PID $PORT" EXIT
wait_alive "$PORT"

OUT_DIR="$(mktemp -d)"
$CLI --port "$PORT" wait_for_state --state MAINMENU --timeout-ms 10000
$CLI --port "$PORT" screenshot --out "$OUT_DIR/main.png"
test -s "$OUT_DIR/main.png"

$CLI --port "$PORT" click --label OPTIONS
$CLI --port "$PORT" wait_for_state --state OPTIONS --timeout-ms 5000
$CLI --port "$PORT" screenshot --out "$OUT_DIR/options.png"
test -s "$OUT_DIR/options.png"

# Frame headers should match (PNG magic).
head -c 8 "$OUT_DIR/main.png" | xxd | head -1 | grep -q '8950 4e47'
head -c 8 "$OUT_DIR/options.png" | xxd | head -1 | grep -q '8950 4e47'
echo "PASS 20_screenshot ($OUT_DIR)"
```

- [ ] **Step 5: run.sh — top-level driver**

`tests/cli-agent/run.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FAIL=0
for s in "$DIR"/e2e/[0-9]*_*.sh; do
  echo "=== $s ==="
  if bash "$s"; then
    echo "  ok"
  else
    echo "  FAIL"
    FAIL=$((FAIL+1))
  fi
done
if [ "$FAIL" -ne 0 ]; then
  echo "$FAIL scenario(s) failed" >&2
  exit 1
fi
echo "all green"
```

- [ ] **Step 6: chmod and run**

```bash
chmod +x tests/cli-agent/run.sh tests/cli-agent/e2e/*.sh
tests/cli-agent/run.sh
```

Expected: `all green`.

- [ ] **Step 7: Commit**

```bash
git add tests/cli-agent/
git commit -m "test(cli-agent): E2E harness driving the daemon end to end"
```

---

## Task 19: Skill — `using-silencer-cli`

**Files:**
- Create: `.claude/skills/using-silencer-cli/SKILL.md`

- [ ] **Step 1: Write the skill**

`.claude/skills/using-silencer-cli/SKILL.md`:

```markdown
---
name: using-silencer-cli
description: Use when you need to drive the Silencer game from a terminal — verifying UI changes, navigating menus, taking screenshots, reading game state — without a human at the keyboard. The CLI talks JSON-lines TCP to a long-running silencer client.
---

# Using silencer-cli

`silencer-cli` is a stateless Bun+TS command that talks to a long-running
`silencer` client over `127.0.0.1:<port>`. The game stays up between
invocations — that's what makes navigation possible.

## Setup (every session)

1. Build the client once:

   ```bash
   cd clients/silencer && cmake -B build -DSILENCER_BUILD_TESTS=ON \
     && cmake --build build -j
   ```

2. Pick a port and start the daemon (preferably `--headless` for CI/SSH):

   ```bash
   PORT=5170
   ./clients/silencer/build/silencer --headless --control-port "$PORT" \
     >/tmp/silencer.log 2>&1 &
   ```

3. Wait until it's up:

   ```bash
   for _ in $(seq 1 60); do
     bun clients/silencer-cli/index.ts --port $PORT ping >/dev/null 2>&1 \
       && break
     sleep 0.5
   done
   ```

## Driving the UI

Find the current screen and its widgets first — never guess labels:

```bash
bun clients/silencer-cli/index.ts --port $PORT state
bun clients/silencer-cli/index.ts --port $PORT inspect
```

Then click. Labels are case-insensitive, exact match:

```bash
bun clients/silencer-cli/index.ts --port $PORT click --label OPTIONS
bun clients/silencer-cli/index.ts --port $PORT wait_for_state \
  --state OPTIONS --timeout-ms 5000
```

Going back:

```bash
bun clients/silencer-cli/index.ts --port $PORT back
```

## Screenshots

```bash
bun clients/silencer-cli/index.ts --port $PORT screenshot --out /tmp/x.png
```

`/tmp/x.png` is a 24-bit RGB PNG of the offscreen `screenbuffer` — the
exact frame the player would see. There is a one-frame latency
(`screenshot` is a post-render op), so insert a `wait_frames --n 1` if
you've just clicked something and want the result.

## Pause / step

Pause is supported in MAINMENU, OPTIONS, replay, and SP/test contexts.
In live multiplayer, `pause` returns `WRONG_STATE` — that's by design.

```bash
bun clients/silencer-cli/index.ts --port $PORT pause
bun clients/silencer-cli/index.ts --port $PORT step --frames 30
bun clients/silencer-cli/index.ts --port $PORT resume
```

## Exit codes

- `0` — `ok:true`. Result JSON on stdout.
- `1` — `ok:false`. `[CODE] error` on stderr. Codes:
  `BAD_REQUEST`, `UNKNOWN_OP`, `WIDGET_NOT_FOUND`, `WIDGET_AMBIGUOUS`,
  `WRONG_STATE`, `TIMEOUT`, `INTERNAL`.
- `2` — transport failure (game not running, port wrong, etc).

Branch on these. Don't grep error strings — the codes are the contract.

## Common patterns

### Check that a button exists before clicking

```bash
bun clients/silencer-cli/index.ts --port $PORT inspect \
  | jq -r '.widgets[] | select(.kind=="button") | .label'
```

### Round-trip a navigation and capture a screenshot

```bash
bun clients/silencer-cli/index.ts --port $PORT click --label OPTIONS
bun clients/silencer-cli/index.ts --port $PORT wait_for_state \
  --state OPTIONS --timeout-ms 5000
bun clients/silencer-cli/index.ts --port $PORT wait_frames --n 1
bun clients/silencer-cli/index.ts --port $PORT screenshot \
  --out /tmp/options.png
bun clients/silencer-cli/index.ts --port $PORT back
```

### Shut down cleanly

```bash
bun clients/silencer-cli/index.ts --port $PORT quit
```

## Troubleshooting

- `[TRANSPORT] ECONNREFUSED` → daemon not yet listening; sleep + retry,
  or check `/tmp/silencer.log`.
- `[WIDGET_NOT_FOUND]` → run `inspect` and copy the exact label.
- `[WIDGET_AMBIGUOUS]` → pass `--label NUMERIC_ID` (the widget's `id`
  field from `inspect`) instead of the label string.
- `[WRONG_STATE]` for `pause`/`step` → expected in live multiplayer;
  this is a v1 limitation.

## Where this lives

- C++ side: `clients/silencer/src/controlserver.{h,cpp}`,
  `controldispatch.{h,cpp}`. Game-thread integration in
  `clients/silencer/src/game.cpp` (`Game::Loop`).
- CLI: `clients/silencer-cli/`.
- Spec: `docs/superpowers/specs/2026-04-26-cli-agent-control-design.md`.
- E2E: `tests/cli-agent/run.sh`.
```

- [ ] **Step 2: Commit**

```bash
git add .claude/skills/using-silencer-cli/SKILL.md
git commit -m "docs(skill): using-silencer-cli for agent-driven game control"
```

---

## Task 20: Component CLAUDE.md updates

**Files:**
- Modify: `clients/silencer/CLAUDE.md`
- Modify: `clients/cli/CLAUDE.md` (point at the now-real `clients/silencer-cli/`)

- [ ] **Step 1: silencer CLAUDE.md — add a "CLI agent control" section**

In `clients/silencer/CLAUDE.md`, before the "Gotchas" section, append:

```markdown
## CLI agent control

`silencer --control-port <P> [--headless]` exposes a JSON-lines TCP
control channel on `127.0.0.1:<P>`. Implementation lives in
`src/controlserver.{h,cpp}` (accept thread + queue) and
`src/controldispatch.{h,cpp}` (op handlers, game-thread only). The
queue drains once per frame in `Game::Loop` between input and tick.
PNG capture uses `third_party/stb_image_write.h`.

Wire format, ops, and acceptance scenarios:
[../../docs/superpowers/specs/2026-04-26-cli-agent-control-design.md](../../docs/superpowers/specs/2026-04-26-cli-agent-control-design.md).

Driver and skill: [../silencer-cli/](../silencer-cli/) and
`.claude/skills/using-silencer-cli/`.
```

- [ ] **Step 2: clients/cli/CLAUDE.md — redirect**

Replace the placeholder with:

```markdown
# clients/cli — superseded

The agent-driving CLI lives at `../silencer-cli/`. This directory is
preserved as a redirect for older docs that linked here. Safe to
delete in a follow-up sweep.
```

- [ ] **Step 3: AGENTS.md symlinks already exist — verify**

```bash
ls -l clients/silencer/AGENTS.md clients/cli/AGENTS.md
```

If either is missing, recreate with `ln -s CLAUDE.md AGENTS.md` from inside the dir.

- [ ] **Step 4: Commit**

```bash
git add clients/silencer/CLAUDE.md clients/cli/CLAUDE.md
git commit -m "docs(claude-md): document CLI agent control surface"
```

---

## Task 21: Final verification + PR

**Files:** none (just running the gauntlet).

- [ ] **Step 1: Build clean from scratch**

```bash
cd clients/silencer
rm -rf build
cmake -B build -DSILENCER_BUILD_TESTS=ON
cmake --build build -j
```

Expected: clean build.

- [ ] **Step 2: Run unit tests**

```bash
./clients/silencer/build/tests/silencer_tests
```

Expected: all PASS.

- [ ] **Step 3: Run the E2E suite**

```bash
tests/cli-agent/run.sh
```

Expected: `all green`. This is the spec's acceptance gate.

- [ ] **Step 4: Push and open the PR**

```bash
git push -u origin hv/cli
gh pr create --title "feat(client): CLI agent control v1" --body "$(cat <<'EOF'
## Summary
- Implements the v1 control channel + `silencer-cli` wrapper from
  `docs/superpowers/specs/2026-04-26-cli-agent-control-design.md`.
- Adds `--control-port` and `--headless` flags to the client.
- New `clients/silencer-cli/` Bun+TS driver.
- New `.claude/skills/using-silencer-cli/` skill so agents can drive
  the game end-to-end.
- New `tests/cli-agent/` E2E harness — covers the spec's acceptance
  scenario (MAINMENU ↔ OPTIONS, screenshots).

## Test plan
- [ ] `cmake -B build -DSILENCER_BUILD_TESTS=ON && cmake --build build -j`
- [ ] `./build/tests/silencer_tests` — unit tests pass.
- [ ] `tests/cli-agent/run.sh` — E2E green.
- [ ] Manual: `silencer --headless --control-port 5170` then
      `bun clients/silencer-cli/index.ts ping`.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

Expected: PR URL printed. Done.

---

## Self-review checklist (run after writing the plan)

- [x] **Spec coverage:** every section of the spec has a task.
  - Architecture (spec 26-52) → Tasks 2, 3, 4.
  - Loop integration (spec 54-93) → Tasks 4, 14, 15.
  - Command surface introspection → Tasks 1, 5, 7, 8.
  - UI actions → Tasks 9, 10.
  - Effects (screenshot, quit) → Tasks 11, 12, 13.
  - Time/pacing → Tasks 14, 15.
  - Multiplayer pause caveat → Task 14 step 1.
  - Wire format → Task 2 (`ReplyToLine`), Task 17 (`silencer-cli`).
  - Code layout → Tasks 2, 3, 11, 17, 20 (CLAUDE.md).
  - Testing (unit + E2E + acceptance) → Tasks 6, 18, 21.
- [x] **Placeholder scan:** no TBD/TODO; every code step has a code block; every command has expected output.
- [x] **Type consistency:** `ControlReply`/`ControlCommand` field names stable across tasks; `Game::PendingWait` defined where used (Task 15) before reference; handler signatures consistent.
- [x] **Acceptance gate** (spec line 217) baked into Task 18 + 21.
