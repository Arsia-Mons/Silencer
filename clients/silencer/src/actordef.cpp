#include "actordef.h"
#include "json.hpp"
#include <fstream>
#include <stdexcept>
#include <cstdio>
#include <curl/curl.h>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#endif

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// AnimSequence
// ---------------------------------------------------------------------------

int AnimSequence::TotalDuration() const {
	int total = 0;
	for (const auto& f : frames) total += f.duration;
	return total;
}

const FrameDef* AnimSequence::Resolve(int state_i) const {
	if (frames.empty() || state_i < 0) return nullptr;
	if (loop) {
		int total = TotalDuration();
		if (total <= 0) return nullptr;
		state_i = state_i % total;
	}
	int acc = 0;
	for (const auto& f : frames) {
		acc += f.duration;
		if (state_i < acc) return &f;
	}
	return nullptr; // past end of non-looping sequence
}

// ---------------------------------------------------------------------------
// JSON parsing helpers
// ---------------------------------------------------------------------------

static FrameHurtbox ParseHurtbox(const json& j) {
	FrameHurtbox hb = {0, 0, 0, 0};
	if (j.is_array() && j.size() == 4) {
		hb.x1 = j[0].get<int>();
		hb.y1 = j[1].get<int>();
		hb.x2 = j[2].get<int>();
		hb.y2 = j[3].get<int>();
	}
	return hb;
}

static AnimSequence ParseSequence(const json& j) {
	AnimSequence seq;
	seq.loop = j.value("loop", false);
	const auto& frames = j.at("frames");
	for (const auto& fj : frames) {
		FrameDef fd;
		fd.bank     = fj.at("bank").get<int>();
		fd.index    = fj.at("index").get<int>();
		fd.duration = fj.value("duration", 1);
		if (fd.duration < 1) fd.duration = 1;
		fd.hurtbox  = ParseHurtbox(fj.value("hurtbox", json::array()));
		seq.frames.push_back(fd);
	}
	return seq;
}

static bool ParseActorDef(const std::string& path, ActorDef& out) {
	std::ifstream f(path);
	if (!f.is_open()) {
		fprintf(stderr, "[actordef] cannot open %s\n", path.c_str());
		return false;
	}
	try {
		json j;
		f >> j;
		out.id = j.at("id").get<std::string>();
		if (j.contains("sequences")) {
			for (auto it = j["sequences"].begin(); it != j["sequences"].end(); ++it) {
				out.sequences[it.key()] = ParseSequence(it.value());
			}
		}
		return true;
	} catch (const std::exception& e) {
		fprintf(stderr, "[actordef] parse error in %s: %s\n", path.c_str(), e.what());
		return false;
	}
}

// ---------------------------------------------------------------------------
// Directory listing (POSIX + Win32)
// ---------------------------------------------------------------------------

static std::vector<std::string> ListJsonFiles(const std::string& dir) {
	std::vector<std::string> out;
#ifdef _WIN32
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA((dir + "\\*.json").c_str(), &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			out.push_back(dir + "\\" + fd.cFileName);
		} while (FindNextFileA(h, &fd));
		FindClose(h);
	}
#else
	DIR* d = opendir(dir.c_str());
	if (!d) return out;
	struct dirent* entry;
	while ((entry = readdir(d)) != nullptr) {
		std::string name(entry->d_name);
		if (name.size() > 5 && name.substr(name.size() - 5) == ".json") {
			out.push_back(dir + "/" + name);
		}
	}
	closedir(d);
#endif
	return out;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int LoadActorDefs(const std::string& dir,
                  std::unordered_map<std::string, ActorDef>& out) {
	int loaded = 0;
	for (const auto& path : ListJsonFiles(dir)) {
		ActorDef def;
		if (ParseActorDef(path, def) && !def.id.empty()) {
			out[def.id] = std::move(def);
			++loaded;
		}
	}
	return loaded;
}

// ---------------------------------------------------------------------------
// HTTP fetch helpers (curl)
// ---------------------------------------------------------------------------

namespace {

struct StrBuf {
	std::string data;
	static size_t Write(void* ptr, size_t sz, size_t n, void* ud) {
		auto* buf = static_cast<StrBuf*>(ud);
		size_t incoming = sz * n;
		if (buf->data.size() + incoming > 4 * 1024 * 1024) return 0; // 4 MB cap
		buf->data.append(static_cast<const char*>(ptr), incoming);
		return incoming;
	}
};

static std::string CurlGet(const std::string& url) {
	StrBuf buf;
	CURL* c = curl_easy_init();
	if (!c) return "";
	curl_easy_setopt(c, CURLOPT_URL, url.c_str());
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, StrBuf::Write);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &buf);
	curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 3L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, 5L);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(c, CURLOPT_USERAGENT, "silencer-actordef/1");
	CURLcode rc = curl_easy_perform(c);
	curl_easy_cleanup(c);
	if (rc != CURLE_OK) return "";
	return buf.data;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int FetchActorDefs(const char* apiBase,
                   std::unordered_map<std::string, ActorDef>& out) {
	if (!apiBase || apiBase[0] == '\0') return 0;

	// GET /api/actors → JSON array of id strings, e.g. ["player","guard"]
	std::string listBody = CurlGet(std::string(apiBase) + "/api/actors");
	if (listBody.empty()) {
		fprintf(stderr, "[actordef] fetch: could not reach %s/api/actors\n", apiBase);
		return 0;
	}

	json ids;
	try { ids = json::parse(listBody); } catch (...) {
		fprintf(stderr, "[actordef] fetch: bad JSON from actor list\n");
		return 0;
	}
	if (!ids.is_array()) return 0;

	int loaded = 0;
	for (const auto& idj : ids) {
		if (!idj.is_string()) continue;
		std::string id = idj.get<std::string>();

		std::string body = CurlGet(std::string(apiBase) + "/api/actors/" + id);
		if (body.empty()) {
			fprintf(stderr, "[actordef] fetch: failed to get actor %s\n", id.c_str());
			continue;
		}
		try {
			json j = json::parse(body);
			ActorDef def;
			def.id = id;
			if (j.contains("sequences")) {
				for (auto it = j["sequences"].begin(); it != j["sequences"].end(); ++it) {
					def.sequences[it.key()] = ParseSequence(it.value());
				}
			}
			out[id] = std::move(def);
			++loaded;
			fprintf(stderr, "[actordef] fetched \"%s\" from server\n", id.c_str());
		} catch (const std::exception& e) {
			fprintf(stderr, "[actordef] fetch: parse error for %s: %s\n", id.c_str(), e.what());
		}
	}
	return loaded;
}
