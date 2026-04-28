#ifndef KEYBINDS_H
#define KEYBINDS_H

#include "shared.h"
#include <SDL3/SDL_gamepad.h>
#include <string>
#include <vector>
#include <cstdint>

// All player-controlled actions in the game. The Action enum is the single
// source of truth — IndexToConfigKey, keynames[], and the per-action poll
// cascade all consume ACTION_TABLE below instead of duplicating this list.
//
// Order is the order shown in the controls UI and the order rows appear in
// profile JSON files. New actions go at the end.
enum class Action : uint8_t {
	MoveUp, MoveDown, MoveLeft, MoveRight,
	LookUpLeft, LookUpRight, LookDownLeft, LookDownRight,
	Jump, Jetpack, Activate, Use, Fire,
	Chat, NextInv, NextCam, PrevCam, Detonate,
	Disguise, NextWeapon,
	Weapon1, Weapon2, Weapon3, Weapon4,
	UiUp, UiDown, UiLeft, UiRight,
	Count
};

struct ActionInfo {
	Action      action;
	const char* id;     // "fire" — stable string for files & CLI
	const char* label;  // "Fire" — human-readable, shown in UI
};

extern const ActionInfo ACTION_TABLE[(int)Action::Count];

// Lookup helpers (linear scans over a 28-entry table — call freely).
const ActionInfo* FindAction(const std::string& id);
const ActionInfo& GetActionInfo(Action a);

// Tagged binding key. Round-trips with strings of the form:
//   KEY:Up           keyboard scancode (SDL_GetScancodeName/FromName)
//   MOUSE:1          mouse button index (1=left, 2=middle, 3=right)
//   PAD:south        gamepad button (SDL_GetGamepad{Button}From/StringForButton)
//   PAD:lefty-       gamepad axis past deadzone, sign in last char
enum class BindingDevice : uint8_t {
	Keyboard,
	Mouse,
	GamepadButton,
	GamepadAxis,
};

struct BindingKey {
	BindingDevice device;
	int           code;     // SDL_Scancode | mouse btn | SDL_GamepadButton | SDL_GamepadAxis
	int8_t        axisDir;  // ±1 if device == GamepadAxis, else 0
};

// Parse "KEY:Up" / "PAD:south" / "PAD:lefty-" / "MOUSE:1" → BindingKey.
// Returns false on unrecognized prefix or unknown name.
bool ParseBindingKey(const std::string& s, BindingKey& out);
std::string Stringify(const BindingKey& k);

struct Binding {
	std::vector<BindingKey> keys;   // size 1 = single key, size N = AND chord
};

struct ActionBindings {
	std::vector<Binding> bindings;  // OR across this list
};

// Snapshot of gamepad state for one frame — read once per frame from the
// first connected gamepad. Empty/zero when no pad is connected.
struct GamepadState {
	bool        connected = false;
	uint32_t    buttons   = 0;        // bit i = SDL_GamepadButton i pressed
	int16_t     axes[SDL_GAMEPAD_AXIS_COUNT] = {};
	uint32_t    mouseButtons = 0;     // bit (i-1) = mouse button i pressed
};

// Threshold past which an analog axis counts as "pressed". SDL3 axis range
// is [-32768, 32767]; we treat |v| > AXIS_DEADZONE as held.
static constexpr int16_t AXIS_DEADZONE = 16384;

class KeyMap {
public:
	// Reset to all-empty bindings (action exists but is unbound).
	void Clear();

	// Load a profile from a single JSON file. Returns false on read/parse
	// failure; partially-loaded state is undefined and the caller should
	// reset before retrying.
	bool LoadFile(const std::string& path);

	// Atomic save: write to <path>.tmp then rename. On failure leaves the
	// existing file intact.
	bool SaveFile(const std::string& path) const;

	// Per-frame evaluator. kb is SDL_GetKeyboardState's array.
	bool IsPressed(Action a, const Uint8* kb, const GamepadState& gp) const;

	// Direct accessor — used by the controls UI and CLI dispatch.
	ActionBindings& Get(Action a)       { return actions_[(int)a]; }
	const ActionBindings& Get(Action a) const { return actions_[(int)a]; }

	std::string name;
	std::string label;

private:
	ActionBindings actions_[(int)Action::Count];
};

// File-system helpers. The two roots are returned by GetResDir() / GetDataDir().
// These resolve to <root>/keybinds/.
std::string KeybindsResDir();
std::string KeybindsDataDir();

// List all profiles visible to the player: union of (datadir, resdir) by name.
// Returns sorted unique names. Built-in (resdir-only) names are also returned
// in `builtins` separately.
struct ProfileListing {
	std::vector<std::string> all;      // every profile that resolves to a file
	std::vector<std::string> writable; // those present in the datadir
	std::vector<std::string> builtins; // those present in the resdir
};
ProfileListing ListProfiles();

// Resolve a profile name to a readable file path (datadir wins).
// Returns empty string if neither location has it.
std::string ResolveProfilePath(const std::string& name);

// Always returns the datadir path for `name`, whether or not it exists.
std::string WritableProfilePath(const std::string& name);

#endif
