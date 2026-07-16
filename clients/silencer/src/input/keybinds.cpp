#include "keybinds.h"
#include "os.h"
#include "config.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <set>

#ifdef _WIN32
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
	{ Action::UiConfirm,      "ui_confirm",      "UI Confirm"      },
	{ Action::UiCancel,       "ui_cancel",       "UI Cancel"       },
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

bool KeyMap::IsPressedByScancode(Action a, int sc, const Uint8* kb) const {
	const ActionBindings& ab = actions_[(int)a];
	for (const Binding& b : ab.bindings) {
		bool contains = false;
		for (const BindingKey& k : b.keys) {
			if (k.device == BindingDevice::Keyboard && k.code == sc) { contains = true; break; }
		}
		if (!contains) continue;
		bool all = true;
		for (const BindingKey& k : b.keys) {
			if (k.device != BindingDevice::Keyboard || !kb || !kb[k.code]) { all = false; break; }
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
		// Reject (don't truncate) an over-cap chord: a combo past CHORD_CAP
		// keys is malformed for the rows-of-combos model.
		return !out.keys.empty() && (int)out.keys.size() <= CHORD_CAP;
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
			// Reject over-cap rows: keep at most COMBO_CAP combos per action.
			if ((int)ab.bindings.size() >= COMBO_CAP) break;
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

bool IsValidProfileName(const std::string& name) {
	if (name.empty() || name.size() > 64) return false;
	for (char c : name) {
		bool ok = (c >= 'a' && c <= 'z') ||
		          (c >= 'A' && c <= 'Z') ||
		          (c >= '0' && c <= '9') ||
		          c == '_' || c == '-';
		if (!ok) return false;
	}
	return true;
}

std::string WritableProfilePath(const std::string& name) {
	if (!IsValidProfileName(name)) return "";
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
	if (!IsValidProfileName(name)) return "";
	std::string a = WritableProfilePath(name);
	if (!a.empty() && FileExists(a)) return a;
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

static bool IsBuiltinProfile(const std::string& name) {
	return name == "default" || name == "wasd" || name == "gamepad";
}

void LoadActiveKeymap(KeyMap& keymap) {
	const char* name = Config::GetInstance().active_keybind_profile;
	if (!name || !*name) name = "default";
	std::string path = ResolveProfilePath(name);
	if (path.empty()) {
		path = ResolveProfilePath("default");
	}
	keymap.Clear();
	if (!path.empty()) {
		keymap.LoadFile(path);
	}
	if (keymap.name.empty()) keymap.name = name;
}

void CycleKeybindPreset(KeyMap& keymap) {
	ProfileListing pl = ListProfiles();
	if (pl.all.empty()) return;
	std::string current = Config::GetInstance().active_keybind_profile;
	size_t next = 0;
	for (size_t i = 0; i < pl.all.size(); i++) {
		if (pl.all[i] == current) { next = (i + 1) % pl.all.size(); break; }
	}
	const std::string& chosen = pl.all[next];
	std::strncpy(Config::GetInstance().active_keybind_profile, chosen.c_str(),
	             sizeof(Config::GetInstance().active_keybind_profile) - 1);
	Config::GetInstance().active_keybind_profile[sizeof(Config::GetInstance().active_keybind_profile) - 1] = '\0';
	LoadActiveKeymap(keymap);
}

void ForkActiveProfileIfBuiltin(KeyMap& keymap) {
	std::string active = Config::GetInstance().active_keybind_profile;
	if (!IsBuiltinProfile(active)) return;
	std::string forked = active + "-custom";
	std::string forkedLabel = (keymap.label.empty() ? active : keymap.label) + "-Custom";
	std::strncpy(Config::GetInstance().active_keybind_profile, forked.c_str(),
	             sizeof(Config::GetInstance().active_keybind_profile) - 1);
	Config::GetInstance().active_keybind_profile[sizeof(Config::GetInstance().active_keybind_profile) - 1] = '\0';
	keymap.name = forked;
	keymap.label = forkedLabel;
}

const char * KeyMap::GetKeyName(SDL_Scancode sym){
#ifdef OUYA // Custom scancodes for ouya controller
	switch((int)sym){
		case SDL_SCANCODE_LALT: return "L2"; break;
		case SDL_SCANCODE_RALT: return "R2"; break;
		case SDL_SCANCODE_HOME: return "Menu"; break;
		case SDL_SCANCODE_RETURN: return "O"; break;
		case SDL_SCANCODE_ESCAPE: return "A"; break;
		case 99: return "U"; break;
		case 100: return "Y"; break;
		case 102: return "L1"; break;
		case 103: return "R1"; break;
		case 106: return "L3"; break;
		case 107: return "R3"; break;
		case SDL_SCANCODE_KP_2: return "RUp"; break;
		case SDL_SCANCODE_KP_4: return "RLeft"; break;
		case SDL_SCANCODE_KP_6: return "RRight"; break;
		case SDL_SCANCODE_KP_8: return "RDown"; break;
	}
#endif
	switch(sym){
		case SDL_SCANCODE_UNKNOWN: return ""; break;
		case SDL_SCANCODE_UP: return "Up"; break;
		case SDL_SCANCODE_DOWN: return "Down"; break;
		case SDL_SCANCODE_LEFT: return "Left"; break;
		case SDL_SCANCODE_RIGHT: return "Right"; break;
		case SDL_SCANCODE_TAB: return "Tab"; break;
		case SDL_SCANCODE_CAPSLOCK: return "CapsLock"; break;
		case SDL_SCANCODE_RSHIFT: return "RShift"; break;
		case SDL_SCANCODE_LSHIFT: return "LShift"; break;
		case SDL_SCANCODE_RETURN: return "Enter"; break;
		case SDL_SCANCODE_SEMICOLON: return ";"; break;
		case SDL_SCANCODE_COMMA: return ","; break;
		case SDL_SCANCODE_PERIOD: return "."; break;
		case SDL_SCANCODE_LEFTBRACKET: return "("; break;
		case SDL_SCANCODE_RIGHTBRACKET: return ")"; break;
		case SDL_SCANCODE_BACKSLASH: return "Backslash"; break;
		case SDL_SCANCODE_BACKSPACE: return "Backspace"; break;
		case SDL_SCANCODE_SLASH: return "Slash"; break;
		case SDL_SCANCODE_SPACE: return "Space"; break;
		case SDL_SCANCODE_RALT: return "RAlt"; break;
		case SDL_SCANCODE_LALT: return "LAlt"; break;
		case SDL_SCANCODE_RCTRL: return "RCtrl"; break;
		case SDL_SCANCODE_LCTRL: return "LCtrl"; break;
		case SDL_SCANCODE_EQUALS: return "="; break;
		case SDL_SCANCODE_MINUS: return "Minus"; break;
		case SDL_SCANCODE_RGUI: return "RWin"; break;
		case SDL_SCANCODE_LGUI: return "LWin"; break;
		case SDL_SCANCODE_APOSTROPHE: return "'"; break;
		case SDL_SCANCODE_GRAVE: return "'"; break;
		case SDL_SCANCODE_ESCAPE: return "Escape"; break;
		case SDL_SCANCODE_INSERT: return "Insert"; break;
		case SDL_SCANCODE_HOME: return "Home"; break;
		case SDL_SCANCODE_END: return "End"; break;
		case SDL_SCANCODE_PAGEUP: return "Page Up"; break;
		case SDL_SCANCODE_PAGEDOWN: return "Page Down"; break;
		case SDL_SCANCODE_NUMLOCKCLEAR: return "NumLock"; break;
		case SDL_SCANCODE_SCROLLLOCK: return "ScrollLock"; break;
		case SDL_SCANCODE_KP_0: return "NumPad 0"; break;
		case SDL_SCANCODE_KP_1: return "NumPad 1"; break;
		case SDL_SCANCODE_KP_2: return "NumPad 2"; break;
		case SDL_SCANCODE_KP_3: return "NumPad 3"; break;
		case SDL_SCANCODE_KP_4: return "NumPad 4"; break;
		case SDL_SCANCODE_KP_5: return "NumPad 5"; break;
		case SDL_SCANCODE_KP_6: return "NumPad 6"; break;
		case SDL_SCANCODE_KP_7: return "NumPad 7"; break;
		case SDL_SCANCODE_KP_8: return "NumPad 8"; break;
		case SDL_SCANCODE_KP_9: return "NumPad 9"; break;
		case SDL_SCANCODE_KP_PERIOD: return "NumPad ."; break;
		case SDL_SCANCODE_KP_DIVIDE: return "NumPad /"; break;
		case SDL_SCANCODE_KP_ENTER: return "NumPad E"; break;
		case SDL_SCANCODE_KP_EQUALS: return "NumPad ="; break;
		case SDL_SCANCODE_KP_MINUS: return "NumPad -"; break;
		case SDL_SCANCODE_KP_MULTIPLY: return "NumPad x"; break;
		case SDL_SCANCODE_KP_PLUS: return "NumPad +"; break;
		case SDL_SCANCODE_F1: return "F1"; break;
		case SDL_SCANCODE_F2: return "F2"; break;
		case SDL_SCANCODE_F3: return "F3"; break;
		case SDL_SCANCODE_F4: return "F4"; break;
		case SDL_SCANCODE_F5: return "F5"; break;
		case SDL_SCANCODE_F6: return "F6"; break;
		case SDL_SCANCODE_F7: return "F7"; break;
		case SDL_SCANCODE_F8: return "F8"; break;
		case SDL_SCANCODE_F9: return "F9"; break;
		case SDL_SCANCODE_F10: return "F10"; break;
		case SDL_SCANCODE_F11: return "F11"; break;
		case SDL_SCANCODE_F12: return "F12"; break;
		case SDL_SCANCODE_F13: return "F13"; break;
		case SDL_SCANCODE_F14: return "F14"; break;
		case SDL_SCANCODE_F15: return "F15"; break;
		case SDL_SCANCODE_A: return "A"; break;
		case SDL_SCANCODE_B: return "B"; break;
		case SDL_SCANCODE_C: return "C"; break;
		case SDL_SCANCODE_D: return "D"; break;
		case SDL_SCANCODE_E: return "E"; break;
		case SDL_SCANCODE_F: return "F"; break;
		case SDL_SCANCODE_G: return "G"; break;
		case SDL_SCANCODE_H: return "H"; break;
		case SDL_SCANCODE_I: return "I"; break;
		case SDL_SCANCODE_J: return "J"; break;
		case SDL_SCANCODE_K: return "K"; break;
		case SDL_SCANCODE_L: return "L"; break;
		case SDL_SCANCODE_M: return "M"; break;
		case SDL_SCANCODE_N: return "N"; break;
		case SDL_SCANCODE_O: return "O"; break;
		case SDL_SCANCODE_P: return "P"; break;
		case SDL_SCANCODE_Q: return "Q"; break;
		case SDL_SCANCODE_R: return "R"; break;
		case SDL_SCANCODE_S: return "S"; break;
		case SDL_SCANCODE_T: return "T"; break;
		case SDL_SCANCODE_U: return "U"; break;
		case SDL_SCANCODE_V: return "V"; break;
		case SDL_SCANCODE_W: return "W"; break;
		case SDL_SCANCODE_X: return "X"; break;
		case SDL_SCANCODE_Y: return "Y"; break;
		case SDL_SCANCODE_Z: return "Z"; break;
		case SDL_SCANCODE_1: return "1"; break;
		case SDL_SCANCODE_2: return "2"; break;
		case SDL_SCANCODE_3: return "3"; break;
		case SDL_SCANCODE_4: return "4"; break;
		case SDL_SCANCODE_5: return "5"; break;
		case SDL_SCANCODE_6: return "6"; break;
		case SDL_SCANCODE_7: return "7"; break;
		case SDL_SCANCODE_8: return "8"; break;
		case SDL_SCANCODE_9: return "9"; break;
		case SDL_SCANCODE_0: return "0"; break;
		default: return "?"; break;
	}
}

std::string GamepadShortLabel(const std::string& raw, SDL_GamepadType type) {
	// Strip trailing axis direction modifier (+/-)
	std::string base = raw;
	if(!base.empty() && (base.back() == '+' || base.back() == '-'))
		base.pop_back();

	static const struct { const char* sdl; const char* xbox; const char* ps; } kTable[] = {
		{"south",         "A",       "Cross"},
		{"north",         "Y",       "Tri"},
		{"east",          "B",       "Circle"},
		{"west",          "X",       "Square"},
		{"back",          "Back",    "Select"},
		{"guide",         "Guide",   "PS"},
		{"start",         "Menu",    "Options"},
		{"leftstick",     "LS",      "L3"},
		{"rightstick",    "RS",      "R3"},
		{"leftshoulder",  "LB",      "L1"},
		{"rightshoulder", "RB",      "R1"},
		{"dpup",          "D-Up",    "D-Up"},
		{"dpdown",        "D-Dn",    "D-Dn"},
		{"dpleft",        "D-Lt",    "D-Lt"},
		{"dpright",       "D-Rt",    "D-Rt"},
		{"lefttrigger",   "LT",      "L2"},
		{"righttrigger",  "RT",      "R2"},
		{"leftx",         "L-X",     "L-X"},
		{"lefty",         "L-Y",     "L-Y"},
		{"rightx",        "R-X",     "R-X"},
		{"righty",        "R-Y",     "R-Y"},
		{"misc1",         "Share",   "Share"},
		{"touchpad",      "Touch",   "Touch"},
	};
	bool isPs = (type == SDL_GAMEPAD_TYPE_PS3 ||
	             type == SDL_GAMEPAD_TYPE_PS4 ||
	             type == SDL_GAMEPAD_TYPE_PS5);
	for(const auto& e : kTable){
		if(base == e.sdl)
			return isPs ? e.ps : e.xbox;
	}
	return raw; // unknown: return original (already has suffix stripped)
}
