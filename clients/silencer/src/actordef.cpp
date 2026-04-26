#include "actordef.h"
#include "json.hpp"
#include <fstream>
#include <stdexcept>
#include <cstdio>

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
