#ifndef ACTION_CATALOG_H
#define ACTION_CATALOG_H

#include <cstdint>
#include <string>

// All player-controlled actions in the game. The Action enum is the single
// source of truth: action-table rows, profile JSON keys, control UI rows, and
// the per-action poll cascade all consume ACTION_TABLE instead of duplicating
// this list.
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
	UiConfirm, UiCancel,
	Count
};

struct ActionInfo {
	Action      action;
	const char* id;     // "fire" - stable string for files and CLI
	const char* label;  // "Fire" - human-readable, shown in UI
};

extern const ActionInfo ACTION_TABLE[(int)Action::Count];

// Lookup helpers (linear scans over a 28-entry table; call freely).
const ActionInfo* FindAction(const std::string& id);
const ActionInfo& GetActionInfo(Action a);

#endif
