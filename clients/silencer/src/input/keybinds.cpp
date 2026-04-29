#include "keybinds.h"
#include "os.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <set>

#ifdef _WIN32
// Without this, <windows.h> drags in <winsock.h> (winsock 1.x) and any
// translation unit that later includes <winsock2.h> — e.g. when CMake's
// unity build bundles us with net/controlserver.cpp — collides on
// sockaddr/fd_set/etc.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using nlohmann::json;

// ---------------------------------------------------------------------------
// Action table
// ---------------------------------------------------------------------------
//
// Single source of truth. Order is the order shown in the controls UI.

const ActionInfo ACTION_TABLE[(int)Action::Count] = {
	{ Action::MoveUp,         "move_up",         "Move Up"         },
	{ Action::MoveDown,       "move_down",       "Move Down"       },
	{ Action::MoveLeft,       "move_left",       "Move Left"       },
	{ Action::MoveRight,      "move_right",      "Move Right"      },
	{ Action::LookUpLeft,     "look_up_left",    "Aim Up/Left"     },
	{ Action::LookUpRight,    "look_up_right",   "Aim Up/Right"    },
	{ Action::LookDownLeft,   "look_down_left",  "Aim Down/Left"   },
	{ Action::LookDownRight,  "look_down_right", "Aim Down/Right"  },
	{ Action::Jump,           "jump",            "Jump"            },
	{ Action::Jetpack,        "jetpack",         "Jetpack"         },
	{ Action::Activate,       "activate",        "Activate/Hack"   },
	{ Action::Use,            "use",             "Use Inventory"   },
	{ Action::Fire,           "fire",            "Fire"            },
	{ Action::Chat,           "chat",            "Chat"            },
	{ Action::NextInv,        "next_inv",        "Next Inventory"  },
	{ Action::NextCam,        "next_cam",        "Next Camera"     },
	{ Action::PrevCam,        "prev_cam",        "Previous Camera" },
	{ Action::Detonate,       "detonate",        "Detonate"        },
	{ Action::Disguise,       "disguise",        "Disguise"        },
	{ Action::NextWeapon,     "next_weapon",     "Next Weapon"     },
	{ Action::Weapon1,        "weapon_1",        "Weapon 1"        },
	{ Action::Weapon2,        "weapon_2",        "Weapon 2"        },
	{ Action::Weapon3,        "weapon_3",        "Weapon 3"        },
	{ Action::Weapon4,        "weapon_4",        "Weapon 4"        },
	{ Action::UiUp,           "ui_up",           "UI Up"           },
	{ Action::UiDown,         "ui_down",         "UI Down"         },
	{ Action::UiLeft,         "ui_left",         "UI Left"         },
	{ Action::UiRight,        "ui_right",        "UI Right"        },
};

const ActionInfo* FindAction(const std::string& id) {
	for (const auto& info : ACTION_TABLE) {
		if (id == info.id) return &info;
	}
	return nullptr;
}

const ActionInfo& GetActionInfo(Action a) {
	return ACTION_TABLE[(int)a];
}

// ---------------------------------------------------------------------------
// Binding key parse / stringify
// ---------------------------------------------------------------------------

bool ParseBindingKey(const std::string& s, BindingKey& out) {
	auto colon = s.find(':');
	if (colon == std::string::npos) return false;
	std::string prefix = s.substr(0, colon);
	std::string rest   = s.substr(colon + 1);
	if (rest.empty()) return false;

	if (prefix == "KEY") {
		SDL_Scancode sc = SDL_GetScancodeFromName(rest.c_str());
		if (sc == SDL_SCANCODE_UNKNOWN) return false;
		out.device  = BindingDevice::Keyboard;
		out.code    = (int)sc;
		out.axisDir = 0;
		return true;
	}
	if (prefix == "MOUSE") {
		int btn = std::atoi(rest.c_str());
		if (btn < 1 || btn > 16) return false;
		out.device  = BindingDevice::Mouse;
		out.code    = btn;
		out.axisDir = 0;
		return true;
	}
	if (prefix == "PAD") {
		// Try button first.
		SDL_GamepadButton b = SDL_GetGamepadButtonFromString(rest.c_str());
		if (b != SDL_GAMEPAD_BUTTON_INVALID) {
			out.device  = BindingDevice::GamepadButton;
			out.code    = (int)b;
			out.axisDir = 0;
			return true;
		}
		// Else axis with trailing sign: "lefty-" / "righttrigger+".
		char sign = rest.back();
		if (sign == '+' || sign == '-') {
			std::string axisName = rest.substr(0, rest.size() - 1);
			SDL_GamepadAxis a = SDL_GetGamepadAxisFromString(axisName.c_str());
			if (a != SDL_GAMEPAD_AXIS_INVALID) {
				out.device  = BindingDevice::GamepadAxis;
				out.code    = (int)a;
				out.axisDir = (sign == '+') ? +1 : -1;
				return true;
			}
		}
		// Triggers can be addressed without a sign; default to "+".
		SDL_GamepadAxis a = SDL_GetGamepadAxisFromString(rest.c_str());
		if (a != SDL_GAMEPAD_AXIS_INVALID) {
			out.device  = BindingDevice::GamepadAxis;
			out.code    = (int)a;
			out.axisDir = +1;
			return true;
		}
		return false;
	}
	return false;
}

std::string Stringify(const BindingKey& k) {
	switch (k.device) {
		case BindingDevice::Keyboard: {
			const char* n = SDL_GetScancodeName((SDL_Scancode)k.code);
			if (!n || !*n) return std::string("KEY:Unknown");
			return std::string("KEY:") + n;
		}
		case BindingDevice::Mouse: {
			char buf[16];
			std::snprintf(buf, sizeof(buf), "MOUSE:%d", k.code);
			return buf;
		}
		case BindingDevice::GamepadButton: {
			const char* n = SDL_GetGamepadStringForButton((SDL_GamepadButton)k.code);
			if (!n || !*n) return std::string("PAD:unknown");
			return std::string("PAD:") + n;
		}
		case BindingDevice::GamepadAxis: {
			const char* n = SDL_GetGamepadStringForAxis((SDL_GamepadAxis)k.code);
			if (!n || !*n) return std::string("PAD:unknown");
			std::string s = std::string("PAD:") + n;
			s += (k.axisDir < 0) ? '-' : '+';
			return s;
		}
	}
	return "";
}

// ---------------------------------------------------------------------------
// KeyMap
// ---------------------------------------------------------------------------

void KeyMap::Clear() {
	for (auto& a : actions_) a.bindings.clear();
	name.clear();
	label.clear();
}

static bool IsKeyHeld(const BindingKey& k, const Uint8* kb, const GamepadState& gp) {
	switch (k.device) {
		case BindingDevice::Keyboard:
			return kb && kb[k.code] != 0;
		case BindingDevice::Mouse:
			return (gp.mouseButtons & (1u << (k.code - 1))) != 0;
		case BindingDevice::GamepadButton:
			return gp.connected && (gp.buttons & (1u << k.code)) != 0;
		case BindingDevice::GamepadAxis: {
			if (!gp.connected) return false;
			if (k.code < 0 || k.code >= SDL_GAMEPAD_AXIS_COUNT) return false;
			int v = gp.axes[k.code];
			return k.axisDir > 0 ? v > AXIS_DEADZONE : v < -AXIS_DEADZONE;
		}
	}
	return false;
}

bool KeyMap::IsPressed(Action a, const Uint8* kb, const GamepadState& gp) const {
	const ActionBindings& ab = actions_[(int)a];
	for (const Binding& b : ab.bindings) {
		if (b.keys.empty()) continue;
		bool all = true;
		for (const BindingKey& k : b.keys) {
			if (!IsKeyHeld(k, kb, gp)) { all = false; break; }
		}
		if (all) return true;
	}
	return false;
}

// Parse a single binding entry: either a string (single key) or an array of
// strings (chord). Returns false on any unparseable string.
static bool ParseBinding(const json& je, Binding& out) {
	out.keys.clear();
	if (je.is_string()) {
		BindingKey k;
		if (!ParseBindingKey(je.get<std::string>(), k)) return false;
		out.keys.push_back(k);
		return true;
	}
	if (je.is_array()) {
		for (const auto& s : je) {
			if (!s.is_string()) return false;
			BindingKey k;
			if (!ParseBindingKey(s.get<std::string>(), k)) return false;
			out.keys.push_back(k);
		}
		return !out.keys.empty();
	}
	return false;
}

bool KeyMap::LoadFile(const std::string& path) {
	std::ifstream f(path);
	if (!f.is_open()) return false;
	json j;
	try { f >> j; }
	catch (const std::exception& e) {
		fprintf(stderr, "[keybinds] parse error in %s: %s\n", path.c_str(), e.what());
		return false;
	}
	Clear();
	name  = j.value("name",  std::string());
	label = j.value("label", std::string());
	if (!j.contains("actions") || !j["actions"].is_object()) return true;
	for (auto it = j["actions"].begin(); it != j["actions"].end(); ++it) {
		const ActionInfo* info = FindAction(it.key());
		if (!info) continue; // unknown action id silently skipped (forward compat)
		const json& body = it.value();
		if (!body.contains("bindings") || !body["bindings"].is_array()) continue;
		ActionBindings& ab = actions_[(int)info->action];
		for (const auto& je : body["bindings"]) {
			Binding b;
			if (ParseBinding(je, b)) ab.bindings.push_back(std::move(b));
		}
	}
	return true;
}

static json BindingToJson(const Binding& b) {
	if (b.keys.size() == 1) return json(Stringify(b.keys[0]));
	json arr = json::array();
	for (const auto& k : b.keys) arr.push_back(Stringify(k));
	return arr;
}

bool KeyMap::SaveFile(const std::string& path) const {
	json j;
	std::string saveName  = name.empty()  ? std::string("default") : name;
	std::string saveLabel = label.empty() ? saveName                : label;
	j["name"]  = saveName;
	j["label"] = saveLabel;
	json actions = json::object();
	for (int i = 0; i < (int)Action::Count; ++i) {
		const ActionInfo& info = ACTION_TABLE[i];
		json actBody;
		json bindings = json::array();
		for (const auto& b : actions_[i].bindings) bindings.push_back(BindingToJson(b));
		actBody["bindings"] = bindings;
		actions[info.id] = actBody;
	}
	j["actions"] = actions;

	// Atomic save: write tmp, rename over.
	std::string tmp = path + ".tmp";
	{
		std::ofstream f(tmp);
		if (!f.is_open()) return false;
		f << j.dump(2);
		if (!f.good()) return false;
	}
#ifdef _WIN32
	// Windows rename refuses to overwrite — delete first, race-tolerantly.
	std::remove(path.c_str());
	if (std::rename(tmp.c_str(), path.c_str()) != 0) return false;
#else
	if (std::rename(tmp.c_str(), path.c_str()) != 0) {
		std::remove(tmp.c_str());
		return false;
	}
#endif
	return true;
}

// ---------------------------------------------------------------------------
// Filesystem layout
// ---------------------------------------------------------------------------

std::string KeybindsResDir() {
	// Linux + Windows: GetResDir() resolves to the install prefix or the
	// dir alongside the .exe (see os.cpp). macOS GetResDir() returns "":
	// assets live in the .app bundle at Contents/assets/, reached by
	// chdir from main.cpp's CDResDir(). Cache the resolved path the first
	// time CDResDir has been called.
	std::string d = GetResDir();
	if (!d.empty()) return d + "keybinds/";
#ifdef __APPLE__
	static std::string cached;
	if (!cached.empty()) return cached;
	// CDResDir() chdir's into the resdir; capture it once via getcwd.
	// Don't change cwd ourselves — caller may be in datadir at the moment.
	char prev[4096];
	if (!getcwd(prev, sizeof(prev))) prev[0] = '\0';
	CDResDir();
	char here[4096];
	if (getcwd(here, sizeof(here))) {
		cached = std::string(here) + "/keybinds/";
	}
	if (prev[0]) chdir(prev);
	return cached;
#else
	return "";
#endif
}

std::string KeybindsDataDir() {
	std::string d = GetDataDir();
	if (d.empty()) return d;
	std::string out = d + "keybinds/";
	CreateDirectory(out.c_str());
	return out;
}

std::string WritableProfilePath(const std::string& name) {
	return KeybindsDataDir() + name + ".json";
}

static bool FileExists(const std::string& path) {
#ifdef _WIN32
	DWORD a = GetFileAttributesA(path.c_str());
	return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

std::string ResolveProfilePath(const std::string& name) {
	std::string a = WritableProfilePath(name);
	if (FileExists(a)) return a;
	std::string r = KeybindsResDir();
	if (!r.empty()) {
		std::string b = r + name + ".json";
		if (FileExists(b)) return b;
	}
	return "";
}

static std::vector<std::string> ListJsonStems(const std::string& dir) {
	std::vector<std::string> out;
	if (dir.empty()) return out;
#ifdef _WIN32
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA((dir + "*.json").c_str(), &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			std::string n = fd.cFileName;
			if (n.size() > 5 && n.substr(n.size() - 5) == ".json") {
				out.push_back(n.substr(0, n.size() - 5));
			}
		} while (FindNextFileA(h, &fd));
		FindClose(h);
	}
#else
	DIR* d = opendir(dir.c_str());
	if (!d) return out;
	struct dirent* entry;
	while ((entry = readdir(d)) != nullptr) {
		std::string n(entry->d_name);
		if (n.size() > 5 && n.substr(n.size() - 5) == ".json") {
			out.push_back(n.substr(0, n.size() - 5));
		}
	}
	closedir(d);
#endif
	return out;
}

ProfileListing ListProfiles() {
	ProfileListing out;
	out.writable = ListJsonStems(KeybindsDataDir());
	out.builtins = ListJsonStems(KeybindsResDir());
	std::set<std::string> all(out.writable.begin(), out.writable.end());
	all.insert(out.builtins.begin(), out.builtins.end());
	out.all.assign(all.begin(), all.end());
	std::sort(out.writable.begin(), out.writable.end());
	std::sort(out.builtins.begin(), out.builtins.end());
	return out;
}
