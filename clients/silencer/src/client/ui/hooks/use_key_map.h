#pragma once

#include "input/keybinds.h" // Action, BindingDevice, COMBO_CAP, CHORD_CAP

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace client::ui {

// One chip in a combo: a single resolved key/button/axis with its display label
// already rendered by the composition root (keyboard -> GetKeyName; pad ->
// GamepadShortLabel). The UI never re-derives labels.
struct KeyMapChip {
  BindingDevice device = BindingDevice::Keyboard;
  int code = 0;
  int8_t axis_dir = 0;
  std::string label = {}; // "Space", "LB", "L-Y"
};

// One combo = an AND-chord row (a Binding). 1..CHORD_CAP chips.
struct KeyMapCombo {
  std::vector<KeyMapChip> chips = {};
};

// One action's OR-list of combos = the "rows of combos" for that action.
struct KeyMapAction {
  Action action = {};
  std::string label = {}; // GetActionInfo(action).label, e.g. "Fire"
  std::vector<KeyMapCombo> combos = {};
};

// The key-map model (doc §6/§7b): rows-of-combos read view + the six mutation
// intents. The composition root builds `actions` from the live KeyMap and
// installs the closures over the public Game/keybinds seam. Mutations that
// change persisted state are deferred (queued + drained after render). Caps
// (COMBO_CAP/CHORD_CAP) are enforced in the closures (reject, never truncate).
// Binding mutations fork-if-builtin first so edits never shadow an on-disk
// built-in profile. App-shell (never names Game) — namespace client::ui.
struct KeyMap {
  std::vector<KeyMapAction> actions = {}; // one per Action, in ACTION_TABLE order
  std::string preset_label = {};          // active profile display name
  bool preset_is_builtin = false;         // default/wasd/gamepad
  bool dirty = false;                     // unsaved edits since last load/commit

  // Append a new combo (AND-chord) to `action`'s OR-list. Rejected if it would
  // exceed COMBO_CAP, the chord exceeds CHORD_CAP, or chips is empty.
  std::function<void(Action action, std::vector<KeyMapChip> chips)> add_combo = {};

  // Replace the combo at `combo_index` in `action` with `chips`.
  std::function<void(Action action, int combo_index, std::vector<KeyMapChip> chips)>
      replace_combo = {};

  // Remove the combo at `combo_index` from `action`.
  std::function<void(Action action, int combo_index)> remove_combo = {};

  // Clear all combos for `action` (action stays, becomes unbound).
  std::function<void(Action action)> clear_action = {};

  // Advance Config::active_keybind_profile to the next profile and reload.
  std::function<void()> cycle_preset = {};

  // Persist the active profile to disk (no-op for built-ins) + save Config.
  std::function<void()> commit = {};

  // Discard edits: reload active keymap from disk + reload Config.
  std::function<void()> revert = {};
};

KeyMap use_key_map();

} // namespace client::ui
